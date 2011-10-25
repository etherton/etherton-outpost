/*
    Implementation of James Hlavaty's 1991 board game "Outpost"
    Based on rules as published in Stronghold Games edition.

    Commercial use prohibited.  If you like the game, buy a copy of it from www.strongholdgames.com!
 
    Source code Copyright 2011, David C. Etherton.  All Rights Reserved.
*/

#include <ctype.h>
#include <stdlib.h>

#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

typedef unsigned char byte_t;

enum productionEnum_t { ORE, WATER, TITANIUM, RESEARCH, MICROBIOTICS, NEW_CHEMICALS, ORBITAL_MEDICINE, RING_ORE, MOON_ORE, PRODUCTION_COUNT };

const char *factoryNames[PRODUCTION_COUNT] = { "Ore", "Water", "Titanium", "Research", "Microbiotics", "NewChemicals", "OrbitalMedicine", "RingOre", "MoonOre" };

enum upgradeEnum_t { 
    DATA_LIBRARY, 
    WAREHOUSE, 
    HEAVY_EQUIPMENT, 
    NODULE,     // Era 1
    SCIENTISTS, 
    ORBITAL_LAB, 
    ROBOTICS, 
    LABORATORY, 
    ECOPLANTS, 
    OUTPOST,     // Era 2
    SPACE_STATION, 
    PLANETARY_CRUISER, 
    MOON_BASE,
    UPGRADE_COUNT
};

const char *upgradeNames[UPGRADE_COUNT] = { "DataLibrary", "Warehouse", "HeavyEquipment", "Nodule", "Scientists", "OrbitalLab", "Robotics",
    "Laboratory", "Ecoplants", "Outpost", "SpaceStation", "PlanetaryCruiser", "MoonBase" };


const byte_t upgradeCosts[UPGRADE_COUNT] = { 15,25,30,25,40, 50,50,80,30,100, 120,160,200 };

#define NELEM(x)    (sizeof(x)/sizeof(x[0]))


inline int d4() { return rand() % 4; }
inline int d10() { return rand() % 10; }
inline int d12() { return rand() % 12; }

struct card_t {
    byte_t value,          // biggest card in game is 88, so 7 bits would be enough.
        prodType:4,         // productionEnum_t
        handSize:3,         // 0 for Research and Microbiotics.  4 for mega water, titanium, or new chem.  1 otherwise.
        returnToDiscard:1;  // 1 if it goes to discard pile, 0 if "proxy" card or mega
    bool operator<(const card_t &that) const {
        return value < that.value || (value == that.value && prodType < that.prodType);
    }
};

typedef size_t cardIndex_t;
typedef size_t playerIndex_t;
typedef unsigned amt_t;
typedef int money_t;

struct cardDistribution_t {
    byte_t value, count;
};

class productionDeck_t {
    string name;
    vector<byte_t> deck;
    typedef vector<byte_t>::iterator deckIt_t;
    vector<byte_t> discards;
    typedef vector<byte_t>::iterator discardsIt_t;
    byte_t prodType;
    byte_t average;
    byte_t megaSize;
    byte_t countsInHandSize;
public:
    void init(productionEnum_t n,const cardDistribution_t *dist,size_t count,byte_t avg,byte_t mega,byte_t isBig) {
        deck.clear();
        for (size_t i=0; i<count; i++) {
            for (int j=0; j<dist[i].count; j++)
                deck.push_back(dist[i].value);
        }
        shuffleDeck();
        prodType = n;
        average = avg;
        megaSize = mega;
        countsInHandSize = isBig;
    }

    void shuffleDeck() {
        random_shuffle(deck.begin(), deck.end());
    }

    byte_t getMegaValue() const {
        return megaSize;
    }

    card_t drawCard() {
        if (deck.size() == 0 && discards.size() != 0) {
            // discards go to the draw pile, and discard deck is now empty
            deck.swap(discards);
            shuffleDeck();
        }

        card_t newCard;
        newCard.prodType = prodType;
        newCard.handSize = countsInHandSize;
        // if the discard pile was empty too, synthesize a fake card having the average value.
        // note in the real game you'd track this on paper and it would disappear when spent.
        // in this version, the fake card will make it back into the (now larger) production deck.
        if (deck.size() == 0) {
            newCard.value = average;
            newCard.returnToDiscard = false;
        }
        else {
            // otherwise take the top of the draw deck and consume it, return it to caller
            newCard.value = deck.back();
            newCard.returnToDiscard = true;
            deck.pop_back();
        }
        return newCard;
    }
    
    void discardCard(byte_t value) {
        discards.push_back(value);
    }

    void dump() {
        cout << factoryNames[prodType] << " deck: ";
        for (deckIt_t i=deck.begin(); i!=deck.end(); i++) {
            cout << int(*i) << " ";
        }
        cout << "<- top\n";
    }
};

typedef vector<productionDeck_t> bank_t;

class player_t;

class brain_t {
protected:
    string name;
    player_t *player;
public:
    brain_t(string n) : name(n) { }
    virtual ~brain_t() { }
    const string& getName() const { return name; }
    void setPlayer(player_t &p) { player = &p; }

    virtual bool wantMega(productionEnum_t) = 0;
    virtual cardIndex_t pickDiscard(vector<card_t> &hand) = 0;
    virtual cardIndex_t pickCardToAuction(vector<card_t> &hand,vector<upgradeEnum_t> &upgradeMarket,money_t &bid) = 0;
    virtual money_t raiseOrPass(vector<card_t> &hand,upgradeEnum_t upgrade,money_t bid) = 0;
    virtual money_t payFor(money_t cost,vector<card_t> &hand,bank_t &bank,amt_t minimumResearchCards) = 0; // returns actual amount paid which may be higher
    virtual amt_t purchaseFactories(const vector<byte_t> &maxByType,productionEnum_t &whichFactory) = 0;
    virtual amt_t purchaseColonists(money_t perColonist,amt_t maxAllowed) = 0;
    virtual amt_t purchaseRobots(money_t perRobot,amt_t maxAllowed,amt_t maxUsable) = 0;
    virtual void assignPersonnel(vector<byte_t> &factories,vector<byte_t> &mannedByColonists,vector<byte_t> &mannedByRobots,amt_t robotLimit) = 0;
};

class player_t {
    vector<card_t> hand;
    typedef vector<card_t>::iterator cardIt_t;
    byte_t colonists, colonistLimit, extraColonistLimit, robots, productionSize, productionLimit, unmannedSlots;
    money_t totalCredits, totalUpgradeCosts;
    vector<byte_t> factories;
    vector<byte_t> mannedByColonists;
    vector<byte_t> mannedByRobots;
    vector<byte_t> upgrades;
    brain_t *brain;
public:
    player_t() {
        colonists = 3;
        colonistLimit = 5;
        extraColonistLimit = 0;
        robots = 0; // active robots limit is colonistLimit times number of robotics upgrades.
        productionLimit = 10;
        productionSize = 0;
        factories.resize(PRODUCTION_COUNT);
        // the last element of these two arrays is used to hold personnnel not assigned to any factory
        mannedByColonists.resize(PRODUCTION_COUNT+1);
        mannedByRobots.resize(PRODUCTION_COUNT+1);
        upgrades.resize(UPGRADE_COUNT);
        totalCredits = 0;
        totalUpgradeCosts = 0;
        brain = 0;

        factories[ORE] = 2;
        mannedByColonists[ORE] = 2;
        factories[WATER] = 1;
        mannedByColonists[WATER] = 1;
    }
    
      ~player_t() {
        delete brain;
    }

    void setBrain(brain_t &b) {
        if (brain)
            delete brain;
        brain = &b;
    }

    void addCard(productionDeck_t &fromDeck) {
        addCard(fromDeck.drawCard());
    }

    void addCard(card_t newCard) {
        hand.push_back(newCard);
        productionSize += newCard.handSize;
        totalCredits += newCard.value;
    }
    
    void discardCard(bank_t &bank,cardIndex_t which) {
        card_t discard = hand[which];
        // erase the card from hand
        hand.erase(hand.begin() + which);
        productionSize -= discard.handSize;
        totalCredits -= discard.value;
        if (discard.returnToDiscard)  // mega cards don't go into same deck
            bank[discard.prodType].discardCard(discard.value);
    }

    void drawProductionCards(bank_t &bank,bool firstTurn) {
        for (productionEnum_t i=ORE; i<PRODUCTION_COUNT; i++) {
            int toDraw = mannedByColonists[i] + mannedByRobots[i];
            // special case: each scientist upgrade produces a research card without being populated.
            if (i==RESEARCH)
                toDraw += upgrades[SCIENTISTS];
            else if (i==MICROBIOTICS)
                toDraw += upgrades[ORBITAL_LAB];
            toDraw <<= firstTurn;   // double production on first turn
            while (toDraw >= 4 && bank[i].getMegaValue() && brain->wantMega(i)) {
                card_t megaCard = { bank[i].getMegaValue(), i, 4, false };
                addCard(megaCard);
                toDraw -= 4;
                cout << getName() << " draws a " << factoryNames[i] << " mega production card.\n";
            }
            if (toDraw)
                cout << getName() << " draws " << toDraw << " " << factoryNames[i] << " production cards.\n";
            while (toDraw) {
                addCard(bank[i]);
                --toDraw;
            }
        }
        sort(hand.begin(),hand.end());
    }
    
    void discardExcessProductionCards(bank_t &bank) {
        amt_t discarded = 0;
        while (productionSize > productionLimit) {
            discardCard(bank,brain->pickDiscard(hand));
            ++discarded;    // some cards take four slots
        }
        if (discarded)
            cout << getName() << " discarded " << discarded << " production cards.\n";
    }
    
    void newFactoryFromUpgrade(productionEnum_t which) {
        factories[which]++;
        // TODO - ask if we want to immediately staff the factory
        // this is important if you won a factory after your player turn and want to use it next turn.
    }
    
    void addUpgrade(upgradeEnum_t upgrade) {
        upgrades[upgrade]++;
        // this is used for breaking ties on victory points
        totalUpgradeCosts += upgradeCosts[upgrade];
        
        // implement purchase bonuses
        if (upgrade == WAREHOUSE)
            productionLimit += 5;
        else if (upgrade == NODULE)
            colonistLimit += 3;
        else if (upgrade == ROBOTICS)
            robots++;
        else if (upgrade == LABORATORY)
            newFactoryFromUpgrade(RESEARCH);
        else if (upgrade == OUTPOST) {
            colonistLimit += 5;
            productionLimit += 5;
            newFactoryFromUpgrade(TITANIUM);
        }
        // the last three upgrades are all special factories that must be manned
        // but also can be manned regardless of the population limit.
        else if (upgrade >= SPACE_STATION) {
            newFactoryFromUpgrade(productionEnum_t(upgrade - SPACE_STATION + ORBITAL_MEDICINE));
            extraColonistLimit++;
        }
    }
    
    unsigned computeVictoryPoints() {
        unsigned vps = 0;
        // compute victory points for static upgrades
        for (upgradeEnum_t i=DATA_LIBRARY; i<UPGRADE_COUNT; i++) {
            static const byte_t vpsForUpgrade[UPGRADE_COUNT] = { 1,1,1,2,2, 3,3,5,5,5, 0,0,0 };
            vps += vpsForUpgrade[i] * upgrades[i];
        }
        // now include victory points for factories which are manned
        for (productionEnum_t i=ORE; i<PRODUCTION_COUNT; i++) {
            static const byte_t vpsForMannedFactory[UPGRADE_COUNT] = { 1,1,2,2,0, 0,3,10,15,20 };
            vps += vpsForMannedFactory[i] * (mannedByColonists[i] + mannedByRobots[i]);
        }
        return vps;
    }
    
    void getMaxFactories(vector<byte_t> &outFactories) {
        outFactories.clear();
        outFactories.resize(PRODUCTION_COUNT);
        outFactories[ORE] = totalCredits / 10;
        outFactories[WATER] = totalCredits / 20;
        if (upgrades[HEAVY_EQUIPMENT])
            outFactories[TITANIUM] = totalCredits / 30;
        if (upgrades[LABORATORY])
            outFactories[RESEARCH] = totalCredits / 30;
        // Each new chemicals factory must be paid for with at least one research card
        int numResearch = 0;
        for (cardIt_t i=hand.begin(); i!=hand.end(); i++) {
            numResearch += (i->prodType == RESEARCH);
        }
        outFactories[NEW_CHEMICALS] = totalCredits / 60;
        if (outFactories[NEW_CHEMICALS] > numResearch)
            outFactories[NEW_CHEMICALS] = numResearch;
        // MICROBIOTICS, ORBITAL_MEDICINE, RING_ORE, and MOON_ORE factories are never
        // directly purchased; they are part of upgrade purchases.
    }
    
    void payFor(money_t cost,bank_t &bank,int minResearchCards) {
        brain->payFor(cost,hand,bank,minResearchCards);
    }
    
    cardIndex_t pickCardToAuction(vector<upgradeEnum_t> &upgradeMarket,money_t &bid) {
        return brain->pickCardToAuction(hand,upgradeMarket,bid);
    }
    
    money_t raiseOrPass(upgradeEnum_t upgrade,money_t bid) {
        return brain->raiseOrPass(hand,upgrade,bid);
    }

    void purchaseFactories(bool firstTurn,bank_t &bank) {
        for (;;) {
            vector<byte_t> forPurchase;
            getMaxFactories(forPurchase);
            productionEnum_t whichFactory;
            // special case - can always afford a water factory
            if (firstTurn && !forPurchase[WATER] && hand.size() == 6)
                forPurchase[WATER] = 1;
            // otherwise if we cannot afford any ore, don't bother asking
            else if (!forPurchase[ORE])
                break;
            else
                firstTurn = false;  // clear flag if we don't need to remember this special case.

            // ... ask which factory we want to purchase, and how many ...
            amt_t numToBuy = brain->purchaseFactories(forPurchase,whichFactory);
            if (numToBuy) {
                static const byte_t factoryCost[] = { 10,20,30,30,0,60,0,0,0 };
                // if it's the first turn water special case, pay what we have instead of its actual cost.
                brain->payFor(firstTurn&&whichFactory==WATER? totalCredits : numToBuy * factoryCost[whichFactory],hand,bank,whichFactory==NEW_CHEMICALS? numToBuy : 0);
                factories[whichFactory] += numToBuy;
                unmannedSlots += numToBuy;
                // when we cycle up again there will be no special case.
            }
            else
                break;
        }
    }
    
    void purchaseAndAssignPersonnel(bank_t &bank) {
        if (colonists < colonistLimit + extraColonistLimit) {
            money_t price = upgrades[ECOPLANTS]? 5: 10;
            amt_t limit = colonistLimit + extraColonistLimit - colonists;
            if (limit > totalCredits / price)
                limit = totalCredits / price;
            amt_t purchased = limit? brain->purchaseColonists(price,limit) : 0;
            if (purchased) {
                colonists += purchased;
                mannedByColonists[PRODUCTION_COUNT] += purchased;
                payFor(purchased * price,bank,0);
            }
        }
        // you must have ROBOTICS in order to buy any.  there is no limit to how many you can buy,
        // but you can only operate one per colonist per ROBOTICS upgrade owned.
        if (upgrades[ROBOTICS]) {
            money_t price = 10;
            amt_t limit = totalCredits / price;
            amt_t purchased = limit? brain->purchaseRobots(price,limit,(upgrades[ROBOTICS] * (colonistLimit + extraColonistLimit)) - robots) : 0;
            if (purchased) {
                robots += purchased;
                mannedByRobots[PRODUCTION_COUNT] += purchased;
                payFor(purchased * price,bank,0);
            }
        }        
        brain->assignPersonnel(factories,mannedByColonists,mannedByRobots,upgrades[ROBOTICS] * (colonistLimit + extraColonistLimit));
    }
    
    void displayHoldings() {
        if (totalUpgradeCosts) {
            cout << getName() << "'s upgrades: ";
            for (upgradeEnum_t i=DATA_LIBRARY; i<UPGRADE_COUNT; i++)
                if (upgrades[i])
                    cout << int(upgrades[i]) << "/" << upgradeNames[i] << "; ";
            cout << endl;
        }
        cout << getName() << "'s factories: ";
        for (productionEnum_t i=ORE; i<PRODUCTION_COUNT; i++)
            if (factories[i])
                cout << int(factories[i]) << "/" << factoryNames[i] << "(" << int(mannedByColonists[i]) << "+" << int(mannedByRobots[i]) << "); ";
        cout << "Unused(" << int(mannedByColonists[PRODUCTION_COUNT]) << "+" << int(mannedByRobots[PRODUCTION_COUNT]) << ");\n";
    }
    
    const string& getName() const { return brain->getName(); }
    
    money_t computeDiscount(upgradeEnum_t upgrade) const {
        if (upgrade == SCIENTISTS || upgrade == LABORATORY)
            return upgrades[DATA_LIBRARY] * 10;
        else if (upgrade == WAREHOUSE || upgrade == NODULE)
            return upgrades[HEAVY_EQUIPMENT] * 5;
        else if (upgrade == OUTPOST)
            return upgrades[HEAVY_EQUIPMENT] * 15 + upgrades[ECOPLANTS] * 10;
        else
            return 0;
    }
    
    money_t getTotalCredits() const { return totalCredits; }
    
    money_t getTotalUpgradeCosts() const { return totalUpgradeCosts; }
    
    void dump() {
        cout << "player " << brain->getName() << ", " << int(colonists) << "/" << int(colonistLimit) << " colonists, " << int(productionSize) << "/" << int(productionLimit) << " cards\n\tfactories: ";
        for (int i=ORE; i<PRODUCTION_COUNT; i++)
            if (factories[i])
                cout << factoryNames[i] << ":" << int(factories[i]) << "(" << int(mannedByColonists[i]) << 
                    "/" << int(mannedByRobots[i]) << ") ";
        cout << "\n\tcards: ";
        for (cardIt_t i=hand.begin(); i!=hand.end(); i++)
            cout << factoryNames[i->prodType] << "/" << int(i->value) << "$ ";
        cout << "(" << totalCredits << "$ total)\n";
    }
};


class game_t {
    vector<productionDeck_t> bank;
    vector<byte_t> upgradeDrawPiles;
    vector<upgradeEnum_t> upgradeMarket;
    vector<byte_t> currentMarketCounts;
    vector<player_t> players;
    typedef vector<player_t>::iterator playerIt_t;
    struct playerPos_t {
        bool operator<(const playerPos_t &that) const {
            return vps > that.vps || (vps == that.vps && (totalUpgradeCosts > that.totalUpgradeCosts || 
                                                          (totalUpgradeCosts == that.totalUpgradeCosts && randomNoise > that.randomNoise)));
        }
        unsigned vps;
        money_t totalUpgradeCosts;
        unsigned randomNoise;
        playerIndex_t selfIndex;
    };
    vector<playerPos_t> playerOrder;
    typedef vector<playerPos_t>::iterator playerOrderIt_t;
    byte_t era, marketLimit;
    bool previousMarketEmpty;
public:
    game_t(playerIndex_t playerCount) {
        // default ctor sets up a bunch of game state
        players.resize(playerCount);

        era = 1;
        previousMarketEmpty = false;
        upgradeMarket.clear();
        currentMarketCounts.clear();
        currentMarketCounts.resize(UPGRADE_COUNT);
        marketLimit = playerCount >> 1;
    }
    
    void setupProductionDecks() {
        static const cardDistribution_t OreDeck[] = { {1,4}, {2,6}, {3,6}, {4,6}, {5,4} };
        static const cardDistribution_t WaterDeck[] = { {4,2}, {5,5}, {6,7}, {7,9}, {8,7}, {9,5}, {10,3} };
        static const cardDistribution_t TitaniumDeck[] = { {7,5}, {8,7}, {9,9}, {10,11}, {11,9}, {12,7}, {13,5} };
        static const cardDistribution_t ResearchDeck[] = { {9,2}, {10,3}, {11,4}, {12,5}, {13,6}, {14,5}, {15,4}, {16,3}, {17,2} };
        static const cardDistribution_t MicrobioticsDeck[] = { {14,1}, {15,2}, {16,3}, {17,4}, {18,3}, {19,2}, {20,1} };
        static const cardDistribution_t NewChemicalsDeck[] = { {14,2}, {16,3}, {18,4}, {20,5}, {22,4}, {24,3}, {26,2} };
        static const cardDistribution_t OrbitalMedicineDeck[] = { {20,2}, {25,3}, {30,4}, {35,3}, {40,2} };
        static const cardDistribution_t RingOreDeck[] = { {30,1}, {35,3}, {40,4}, {45,3}, {50,1} };
        static const cardDistribution_t MoonOreDeck[] = { {40,1}, {45,3}, {50,4}, {55,3}, {60,1} };

        bank.clear();
        bank.resize(PRODUCTION_COUNT);
        bank[ORE].init(ORE,OreDeck,NELEM(OreDeck),3,0,true);
        bank[WATER].init(WATER,WaterDeck,NELEM(WaterDeck),7,30,true);
        bank[TITANIUM].init(TITANIUM,TitaniumDeck,NELEM(TitaniumDeck),10,44,true);
        bank[RESEARCH].init(RESEARCH,ResearchDeck,NELEM(ResearchDeck),13,0,false);
        bank[MICROBIOTICS].init(MICROBIOTICS,MicrobioticsDeck,NELEM(MicrobioticsDeck),17,0,false);
        bank[NEW_CHEMICALS].init(NEW_CHEMICALS,NewChemicalsDeck,NELEM(NewChemicalsDeck),20,88,true);
        bank[ORBITAL_MEDICINE].init(ORBITAL_MEDICINE,OrbitalMedicineDeck,NELEM(OrbitalMedicineDeck),30,0,true);
        bank[RING_ORE].init(RING_ORE,RingOreDeck,NELEM(RingOreDeck),40,0,true);
        bank[MOON_ORE].init(MOON_ORE,MoonOreDeck,NELEM(MoonOreDeck),50,0,true);
    }

    void setupUpgradeDecks(playerIndex_t playerCount) {
        upgradeDrawPiles.clear();
        upgradeDrawPiles.reserve(PRODUCTION_COUNT);
        if (playerCount == 2) {
            int even = 0, odd = 0;
            upgradeEnum_t i;
            for (i=DATA_LIBRARY; i<UPGRADE_COUNT; i++) {
                if (rand() & 1) {
                    upgradeDrawPiles.push_back(1);
                    if (++odd == 10)
                        break;
                }
                else {
                    upgradeDrawPiles.push_back(2);
                    if (++even == 10)
                        break;
                }
            }
            // at most 10 upgrade types allowed with one count
            for (; i<UPGRADE_COUNT; i++) {
                upgradeDrawPiles.push_back((odd == 10)? 2 : 1);
            }
        }
        else {
            static const byte_t upgrades_1_10[10] = { 0,0,0, 2,3,3,4,5,5,6 };
            static const byte_t upgrades_11_13[10] = { 0,0,0, 2,3,4,4,5,6,6 };
            for (upgradeEnum_t i=DATA_LIBRARY; i<SPACE_STATION; i++)
                upgradeDrawPiles.push_back(upgrades_1_10[playerCount]);
            for (upgradeEnum_t i=SPACE_STATION; i<UPGRADE_COUNT; i++)
                upgradeDrawPiles.push_back(upgrades_11_13[playerCount]);
        }
    }

    void setInitialPlayerState(playerIndex_t playerCount) {
        // do initial production draws for each player
        for (playerIt_t i=players.begin(); i!= players.end(); i++)
            // production is doubled on first turn.
            i->drawProductionCards(bank,true);
        
        // randomly assign player order on first turn
        // (the random noise will be sole deciding factor)
        computeVictoryPoints();
    }


    void setupGame() {
        setupProductionDecks();
        setupUpgradeDecks(players.size());
        setInitialPlayerState(players.size());
        replaceUpgradeCards();
    }

    void setPlayerBrain(playerIndex_t index,brain_t &brain) {
        players[index].setBrain(brain);
        brain.setPlayer(players[index]);
        // players[index].dump();
    }
  
   
    void computeVictoryPoints() {
        playerOrder.clear();
        for (playerIndex_t i=0; i<players.size(); i++) {
            playerPos_t p = { players[i].computeVictoryPoints(), players[i].getTotalUpgradeCosts(), rand(), i };
            playerOrder.push_back(p);
        }
        // sort in ascending order (default uses operator<)
        sort(playerOrder.begin(),playerOrder.end());
    }
    
    void displayPlayerOrder() {
        for (playerIndex_t i=0; i<playerOrder.size(); i++)
            cout << players[playerOrder[i].selfIndex].getName() << " is Player " << i+1 << " this round (" << playerOrder[i].vps << " VPs).\n";
    }

    void replaceUpgradeCards() {
        // figure out whether the market is totally empty or not
        bool marketEmpty = upgradeMarket.size() == 0;
        for (upgradeEnum_t i=DATA_LIBRARY; i<(era==1?SCIENTISTS:SPACE_STATION) && marketEmpty; i++)
            if (upgradeDrawPiles[i])
                marketEmpty = false;
        static const byte_t minVpsForEra3[] = { 0,0,40,35,40,30,35,40,30,35 };

        // figure out which era we're in now.
        if (era == 1 && (playerOrder[0].vps >= 10 || (marketEmpty && previousMarketEmpty))) {
            cout << "*** Entering era 2!\n";
            era = 2;
        }
        else if (era == 2 && (playerOrder[0].vps >= minVpsForEra3[players.size()] || (marketEmpty && previousMarketEmpty))) {
            cout << "*** Entering era 3!\n";
            era = 3;
        }
        previousMarketEmpty = marketEmpty;
        
        while (upgradeMarket.size() < players.size()) {
            // First check if any roll has a chance to succeeed
            upgradeEnum_t firstMarket = era==3? WAREHOUSE : DATA_LIBRARY;
            int marketSize = era==3? 12 : era==2? 10 : 4;
            bool anyValid = false;
            // note that we start at zero because even in Era 3 an unpurchased Data Library could still come up for auction.
            for (upgradeEnum_t i=DATA_LIBRARY; !anyValid && i<firstMarket+marketSize; i++)
                // if there is a card left of this type and we 
                if (upgradeDrawPiles[i] && currentMarketCounts[i] != marketLimit)
                    anyValid = true;
            
            if (!anyValid)
                break;
            
            upgradeEnum_t roll = static_cast<upgradeEnum_t>(firstMarket + (rand() % marketSize));
            for(;;) {
                if (upgradeDrawPiles[roll] && currentMarketCounts[roll] != marketLimit)
                    break;
                else if (roll) // try next upgrade downward
                    --roll;
                else    // pick a new roll if we hit the bottom of the list
                    roll = static_cast<upgradeEnum_t>(firstMarket + (rand() % marketSize));
            }
            
            cout << upgradeNames[roll] << " added to market.\n";
            upgradeDrawPiles[roll]--;
            currentMarketCounts[roll]++;
            upgradeMarket.push_back(roll);
        }
        cout << "Remaining upgrades: ";
        int nonzero = 0;
        for (upgradeEnum_t i=DATA_LIBRARY; i<UPGRADE_COUNT; i++)
            if (upgradeDrawPiles[i])
                cout << int(upgradeDrawPiles[i]) << "/" << upgradeNames[i] << (++nonzero==6?";\n\t" : "; ");
        if (nonzero == 0)
            cout << "[ none ]";
        cout << endl;
    }

    void drawProductionCards() {
        for (playerOrderIt_t i=playerOrder.begin(); i!=playerOrder.end(); i++) {
            players[i->selfIndex].drawProductionCards(bank,false);
        }
    }

    void discardExcessProductionCards() {
        for (playerOrderIt_t i=playerOrder.begin(); i!=playerOrder.end(); i++) {
            players[i->selfIndex].discardExcessProductionCards(bank);
        }
    }

    void auctionUpgradeCards(playerIndex_t selfIndex) {
        cardIndex_t nextAuction;
        money_t bid;
        cout << "There are " << upgradeMarket.size() << " cards available for auction.\n";
        while (upgradeMarket.size() && (nextAuction = players[selfIndex].pickCardToAuction(upgradeMarket,bid)) != upgradeMarket.size()) {
            // remove the card from the market
            upgradeEnum_t upgrade = upgradeMarket[nextAuction];
            upgradeMarket.erase(upgradeMarket.begin() + nextAuction);
            currentMarketCounts[upgrade]--;
            cout << players[selfIndex].getName() << " places a " << upgradeNames[upgrade] << " up for auction with an opening bid of " << bid << ".\n";
            
            // run the auction until everybody else passes.
            unsigned numPassedInARow = 0;
            playerIndex_t highBidder = selfIndex;
            playerIndex_t nextBidder = selfIndex;
            for (;;) {
                if (++nextBidder == players.size())
                    nextBidder = 0;
                money_t newBid = players[nextBidder].raiseOrPass(upgrade,bid);
                // somebody wants to bid?
                if (newBid) {
                    highBidder = nextBidder;
                    bid = newBid;
                    numPassedInARow = 0;
                    cout << players[highBidder].getName() << " raises the bid to " << bid << ".\n";
                }
                // everybody else has passed?
                else if (++numPassedInARow == players.size()-1)
                    break;
                else
                    cout << players[nextBidder].getName() << " passes.\n";
            }
            
            cout << players[highBidder].getName() << " wins the auction for a " << upgradeNames[upgrade] << " with " << bid << " credits.\n";
            money_t discount = players[highBidder].computeDiscount(upgrade);
            if (bid > discount)
                players[highBidder].payFor(bid - discount,bank,0);
            players[highBidder].addUpgrade(upgrade);
            // TODO: you may immediately move personnel to a new factory even if you've already had your turn
        }
    }

    void performPlayerTurns(bool firstTurn) {
        for (playerOrderIt_t i=playerOrder.begin(); i!=playerOrder.end(); i++) {
            players[i->selfIndex].displayHoldings();
            auctionUpgradeCards(i->selfIndex);
            players[i->selfIndex].displayHoldings();
            players[i->selfIndex].purchaseFactories(firstTurn,bank);
            players[i->selfIndex].displayHoldings();
            players[i->selfIndex].purchaseAndAssignPersonnel(bank);
            cout << endl;
        }        
    }

    bool checkVictoryConditions() {
        computeVictoryPoints();
        if (playerOrder.front().vps < 75)
            return false;
        
        cout << "Game over, final rankings:\n";
        for (playerIndex_t i=0; i<playerOrder.size(); i++) {
            cout << "#" << i+1 <<". " << players[playerOrder[i].selfIndex].getName() << ", with " << playerOrder[i].vps << " and " << playerOrder[i].totalUpgradeCosts << " upgrade value.\n";
        }        
        return true;
    }
};


class computerBrain_t: public brain_t {
    const game_t &game;
public:
    computerBrain_t(std::string name,const game_t &theGame) : brain_t(name), game(theGame) { } 
    bool wantMega(productionEnum_t) { 
        // this decision is hard -- need to factor in what cards we think are left in the deck,
        // and whether we're making any big purchases this turn, and whether we'd be over our
        // hand limit and have to discard.
        return false; 
    }
    cardIndex_t pickDiscard(vector<card_t> &hand) {
        // Just pick the cheapest card we have.
        return 0;
    }
    cardIndex_t pickCardToAuction(vector<card_t> &hand,vector<upgradeEnum_t> &upgradeMarket,int &bid) {
        // for now - never initiates an auction
        return upgradeMarket.size();
    }
    money_t raiseOrPass(vector<card_t> &hand,upgradeEnum_t upgrade,money_t bid) {
        return 0;   // never bids
    }
    money_t payFor(money_t cost,vector<card_t> &hand,bank_t &bank,amt_t minResearchCards) {
        return 0;
    }
    amt_t purchaseFactories(const vector<byte_t> &maxByType,productionEnum_t &whichFactory) {
        return 0;
    }
    amt_t purchaseColonists(money_t perColonist,amt_t maxAllowed) {
        // buy as many colonists as we can afford and still house
        if (perColonist * maxAllowed > player->getTotalCredits())
            return player->getTotalCredits() / perColonist;
        else
            return maxAllowed;
    }
    amt_t purchaseRobots(money_t perRobot,amt_t maxAllowed,amt_t maxUsable) {
        // buy as many robots as we can afford
        return 0;
    }
    void assignPersonnel(vector<byte_t> &factories,vector<byte_t> &mannedByColonists,vector<byte_t> &mannedByRobots,amt_t robotLimit) {
    }
};

static const unsigned EMPTY = 0x7FFFFFFF;

static unsigned readUnsigned() {
    string answer;
    getline(cin,answer);
    if (answer.size())
        return atoi(answer.c_str());
    else
        return EMPTY;
}

static char readLetter() {
    string answer;
    getline(cin,answer);
    return toupper(answer[0]);
}

class playerBrain_t: public brain_t {
public:
    playerBrain_t(std::string name) : brain_t(name) { }
    bool wantMega(productionEnum_t t) {
        cout << name << ", do you want a megaproduction card for " << factoryNames[t] << "? ";
        return readLetter() == 'Y';
    }
    void displayProductionCards(vector<card_t> &hand) {
        for (cardIndex_t i=0; i<hand.size(); i++)
            cout << i << ". " << factoryNames[hand[i].prodType] << "/" << int(hand[i].value) << endl;
    }
    void displayProductionCardsOnSingleLine(vector<card_t> &hand) {
        if (hand.size()) {
            cout << "[ ";
            for (cardIndex_t i=0; i<hand.size(); i++)
                cout << factoryNames[hand[i].prodType] << "/" << int(hand[i].value) << " ";
            cout << "]\n";
        }
        else
            cout << "[ ** no production cards ** ]\n";
    }
   cardIndex_t pickDiscard(vector<card_t> &hand) {
        cout << name << ", you are over your hand limit.\n";
        displayProductionCards(hand);
        cardIndex_t which = 0;
        do {
            cout << name << ", which card to you want to discard? ";
            which = readUnsigned();
        } while (which >= hand.size());
        return which;
    }
    cardIndex_t pickCardToAuction(vector<card_t> &hand,vector<upgradeEnum_t> &upgradeMarket,money_t &bid) {
        for (;;) {
            for (cardIndex_t i=0; i<upgradeMarket.size(); i++)
                cout << i << ". " << upgradeNames[upgradeMarket[i]] << " (min bid is " << int(upgradeCosts[upgradeMarket[i]]) << ")\n";
            displayProductionCardsOnSingleLine(hand);
            cout << name << ", pick a card to auction? (you have " << player->getTotalCredits() << ") ";
            for (;;) {
                cardIndex_t which = readUnsigned();
                if (which >= upgradeMarket.size())
                    return upgradeMarket.size();
                else {
                    bid = raiseOrPass(hand,upgradeMarket[which],upgradeCosts[upgradeMarket[which]]-1);
                    if (!bid) {      // couldn't make valid opening bid, bounce them to selection menu
                        cout << "You cannot afford that.\n";
                        break;
                    }
                    else    // otherwise pass valid selection and bid to caller
                        return which;
                }
            }
        }
    }
    money_t raiseOrPass(vector<card_t> &hand,upgradeEnum_t upgrade,money_t bid) {
        money_t discount = player->computeDiscount(upgrade);
        // don't bother asking if we cannot afford one higher than current bid
        if (bid - discount >= player->getTotalCredits())
            return 0;

        displayProductionCardsOnSingleLine(hand);
        cout << name << ", you have " << player->getTotalCredits() << " and a discount of " << discount << " on this upgrade.\n";
        if (bid == upgradeCosts[upgrade] - 1)
            cout << name << ", please pick an opening bid for " << upgradeNames[upgrade] << " of at least " << bid+1 << " or empty line to pass: ";
        else
            cout << name << ", the bid for " << upgradeNames[upgrade] << " is now at " << bid << ", enter a higher bid or empty line to pass: ";
        for (;;) {
            money_t newBid = readUnsigned();
            if (newBid == 0 || newBid == EMPTY)
                return 0;
            else if (newBid <= bid)
                cout << "The bid must either be 0 to pass or something at least " << bid+1 << ".  Your bid? (0 to pass) ";
            else if (newBid > player->getTotalCredits() + discount)
                cout << "You only have " << player->getTotalCredits() << " (with a discount of " << discount << ") and cannot afford that bid.  Your bid? (0 to pass) ";
            else
                return newBid;
        }
    }
    money_t payFor(money_t cost,vector<card_t> &hand,bank_t &bank,amt_t minimumResearchCards) {
        money_t paid = 0;
        
        // if our total money minus our cheapest card is not enough to pay, toss everything
        // note that all other prerequisites would have been met before we got here.
        if (player->getTotalCredits() - hand[0].value < cost) {
            cout << name << ", that cost all of your poduction cards.\n";
            paid = player->getTotalCredits();
            while (hand.size())
                player->discardCard(bank,0);
        }
        else while (paid < cost || minimumResearchCards) {
            if (cost >= 0) {
                cout << name << ", you need to discard " << cost << " credits worth of cards";
                if (minimumResearchCards)
                    cout << " (at least " << minimumResearchCards << " more research cards)\n";
                else
                    cout << endl;
            }
            else
                cout << name << ", you still need to discard " << minimumResearchCards << " more research cards!\n";
            displayProductionCards(hand);
            cout << "Enter a card, by number, to discard: ";
            cardIndex_t which = readUnsigned();
            if (which < hand.size()) {
                cost -= hand[which].value;
                if (hand[which].prodType == RESEARCH && minimumResearchCards)
                    --minimumResearchCards;
                player->discardCard(bank,which);
            }
        }
        return paid;
    }
    amt_t purchaseFactories(const vector<byte_t> &maxByType,productionEnum_t &whichFactory) {
        cout << name << ", which factory would you like to purchase? (empty line for none)\n";
        for (productionEnum_t i=ORE; i<=NEW_CHEMICALS; i++)
            if (maxByType[i])
                cout << i << ". " << factoryNames[i] << " (at most " << int(maxByType[i]) << ")" << endl;
        whichFactory = (productionEnum_t) readUnsigned();
        if (whichFactory > NEW_CHEMICALS || !maxByType[whichFactory])
            return 0;
        cout << "How many factories would you like to buy?  (at most " << int(maxByType[whichFactory]) << ") ";
        amt_t numToBuy = readUnsigned();
        if (numToBuy > maxByType[whichFactory])
            numToBuy = maxByType[whichFactory];
        return numToBuy;
    }
    amt_t purchaseColonists(money_t perColonist,amt_t maxAllowed) {
        cout << name << ", how many colonists do you want to buy at " << perColonist << " each? (at most " << maxAllowed << ") ";
        amt_t amt = readUnsigned();
        if (amt <= maxAllowed)
            return amt;
        else
            return maxAllowed;
    }
    amt_t purchaseRobots(money_t perRobot,amt_t maxAllowed,amt_t maxUsable) {
        cout << name << ", how many robots do you want to buy at " << perRobot << " each? (at most " << maxAllowed << ", of which " << maxUsable << " can currently be used) ";
        amt_t amt = readUnsigned();
        if (amt <= maxAllowed)
            return amt;
        else
            return maxAllowed;
    }
    void assignPersonnel(vector<byte_t> &factories,vector<byte_t> &mannedByColonists,vector<byte_t> &mannedByRobots,amt_t robotLimit) {
        for (;;) {
            cout << name << ", here are your current allocations:\n";
            amt_t robotsInUse = 0;
            for (productionEnum_t i=ORE; i<PRODUCTION_COUNT; i++) {
                if (factories[i])
                    cout << i << ". " << factoryNames[i] << ": " << int(factories[i]) << " factories manned by " << int(mannedByColonists[i]) << " colonists and " << int(mannedByRobots[i]) << " robots.\n";
                robotsInUse += mannedByRobots[i];
            }
            cout << PRODUCTION_COUNT << ". Unallocated: " << int(mannedByColonists[PRODUCTION_COUNT]) << " colonists, " << int(mannedByRobots[PRODUCTION_COUNT]) << " robots (max allocated is " << robotLimit << ").\n";
            cout << "Transfer colonist (c), robot (r), or anything else to finish? ";
            char cmd = readLetter();
            if (cmd != 'C' && cmd != 'R')
                return;
            vector<byte_t> &manned = (cmd == 'C')? mannedByColonists : mannedByRobots;
            cout << "Transfer source? ";
            productionEnum_t src = (productionEnum_t)readUnsigned();
            if (src > PRODUCTION_COUNT || !manned[src]) {
                cout << "Sorry, that is an invalid or empty transfer source.\n";
                continue;
            }
            cout << "Number to transfer? ";
            amt_t xferAmt = readUnsigned();
            if (xferAmt > manned[src]) {
                cout << "Sorry, only " << int(manned[src]) << " personnel at that location.\n";
                continue;
            }
            cout << "Transfer destination? ";
            productionEnum_t dst = (productionEnum_t)readUnsigned();
            if (dst > PRODUCTION_COUNT || (dst != PRODUCTION_COUNT && factories[dst] < mannedByColonists[dst] + mannedByRobots[dst] + xferAmt)) {
                cout << "Sorry, that is an invalid transfer destination or there isn't enough room there.\n";
                continue;
            }
            else if (dst != PRODUCTION_COUNT && src == PRODUCTION_COUNT && cmd == 'R' && robotsInUse + xferAmt > robotLimit) {
                cout << "Sorry, that would place you over your robot limit of " << robotLimit << ".\n";
                continue;
            }
            // otherwise, perform the transfer
            manned[src] -= xferAmt;
            manned[dst] += xferAmt;
        }
    }
};


int main() {
    unsigned playerCount;

    // seed the RNG, but let it be overridden from user input
    unsigned seed = (unsigned) time(NULL);
    
    for (;;) {
        cout << "Number of players?  (2-9) ";
        playerCount = readUnsigned();
        if (playerCount < 2 || playerCount > 9)
            seed = playerCount;
        else
            break;
    }

    cout << "(using " << seed << " as RNG seed)" << endl;
    srand(seed);

    game_t game(playerCount);
    
    vector<string> computerNames;
    computerNames.push_back("*Alan T.");
    computerNames.push_back("*Steve J.");
    computerNames.push_back("*Grace H.");
    computerNames.push_back("*Donald K.");
    computerNames.push_back("*Dennis R.");
    computerNames.push_back("*Bjarne S.");
    computerNames.push_back("*Herb S.");
    computerNames.push_back("*Bill G.");
    computerNames.push_back("*James H.");
    random_shuffle(computerNames.begin(), computerNames.end());
  
    // attach brains to each player
    cout << "If you enter an empty string for a name, that and all future players will be run by computer.\n";
    cout << "Players should be entered in seating order (aka auction bidding order).\n";
    bool anyHumans = true;
    for (int i=0; i<playerCount; i++) {
        string name;
        if (anyHumans) {
            cout << "Player " << i+1 << " name? ";
            getline(cin,name);
            if (name.size() == 0)
                anyHumans = false;
        }
        brain_t *thisBrain;
        if (name.size() == 0) {
            name = computerNames.back();
            computerNames.pop_back();
            thisBrain = new computerBrain_t(name,game);
        }
        else
            thisBrain = new playerBrain_t(name);
            
          game.setPlayerBrain(i,*thisBrain);
    }

    // set up the play area, deal hands, etc
    game.setupGame();
    // do the first turn of the game (several phases are skipped, victory is impossible and so is an era change
    game.displayPlayerOrder();
    game.performPlayerTurns(true);
    amt_t round = 1;
    
    // now enter the normal turn progression
    do {
        cout << "\n\n    R O U N D   " << ++round << "\n\n";
        game.displayPlayerOrder();
        game.replaceUpgradeCards();
        game.drawProductionCards();
        game.discardExcessProductionCards();
        game.performPlayerTurns(false);
    } while (!game.checkVictoryConditions());
}
