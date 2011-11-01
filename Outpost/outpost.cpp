/*
    Implementation of James Hlavaty's 1991 board game "Outpost"
    Based on rules as published in Stronghold Games edition.

    Commercial use prohibited.  If you like the game, buy a copy of it from www.strongholdgames.com!
 
    Source code Copyright 2011, David C. Etherton.  All Rights Reserved.
*/

#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

#define active table
#define debug table

int debugLevel = 0;

class mystream_t {
    string buffer;
    int column, leftMargin, rightMargin;
public:
    mystream_t() : column(0), leftMargin(0), rightMargin(80) { 
    }
    
    void hadInput() { column = 0; }

    void output(const char *s) { 
        cout << s;
    }
    
    void wordbreak() {
        output("\n");
        column = 0;
        while (column < leftMargin) {
            output(" ");
            ++column;
        }
#ifndef _WIN32
        // recheck terminal width after every line in case it's resized at runtime.
        // could just do this on terminal input instead.
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 && w.ws_col)
            rightMargin = w.ws_col;
#endif
    }
    
    void setLeftMargin(int lm) { leftMargin = lm; }
    
    mystream_t &operator<<(const char*s) { 
        while (*s) {
            char c = *s++;
            if (c == ' ' || c == '\n') {
                if (buffer.size()) {
                    if (column + buffer.size() >= rightMargin)
                        wordbreak();
                    output(buffer.c_str());
                    column += buffer.size();
                    buffer.clear();
                }
                if (c == ' ') {
                    if (++column==rightMargin)
                        wordbreak();
                    else
                        output(" ");
                }
                else {
                    output("\n");
                    column = 0;
                }
            }
            else
                buffer.push_back(c);
        }
        return *this;
    }
    mystream_t &operator<<(unsigned long lu) {
        char buf[32];
        sprintf(buf,"%lu",lu);
        return operator<<(buf);
    }
    mystream_t &operator<<(unsigned u) {
        char buf[32];
        sprintf(buf,"%u",u);
        return operator<<(buf);
    }
    mystream_t &operator<<(int i) {
        char buf[32];
        sprintf(buf,"%d",i);
        return operator<<(buf);
    }
    mystream_t &operator<<(const string& s) { return operator<<(s.c_str()); }
};

mystream_t table;


typedef unsigned char byte_t;

enum productionEnum_t { ORE, WATER, TITANIUM, RESEARCH, MICROBIOTICS, NEW_CHEMICALS, ORBITAL_MEDICINE, RING_ORE, MOON_ORE, PRODUCTION_COUNT, UNUSED=PRODUCTION_COUNT };

const char *factoryNames[PRODUCTION_COUNT] = { "Ore", "Water", "Titanium", "Research", "Microbiotics", "NewChemicals", "OrbitalMedicine", "RingOre", "MoonOre" };

const char *factoryHelp[PRODUCTION_COUNT] = { 
    "Requires colonist or robot; 2 at start of game (earns  1-5$, avg 3$)",
    "Requires colonist or robot; 1 at start of game; has Mega worth 30$ (earns 4-10$, avg 7$)",
    "Requires Heavy Equipment and colonist or robot; has Mega worth 44$ (earns 7-13$, avg 10$)",
    "Requires Laboratory (and operator) or Scientists (no operator required) (earns 9-17$, avg 13$)",
    "Requires 1 Orbital Lab/factory (no operator required) (earns 14-20$, avg 17$)",
    "Requires colonist or robot; must be paid for with at least one Research card per factory purchased; has Mega worth 88$ (earns 14-26$, avg 20$)",
    "Requires 1 Space Station and colonist/factory (earns 20-40$, avg 30$)",
    "Requires 1 PlanetaryCruiser and colonist/factory (earns 30-50$, avg 40$)",
    "Requires 1 MoonBase and colonist/factory (earns 40-60$, avg 50$)"
};

static const byte_t factoryCosts[] = { 10,20,30,30,0,60,0,0,0 };

static const byte_t vpsForMannedFactory[PRODUCTION_COUNT] = { 1,1,2,2,0,3,10,15,20 };

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

static const byte_t vpsForUpgrade[UPGRADE_COUNT] = { 1,1,1,2,2, 3,3,5,5,5, 0,0,0 };

const char *upgradeHelp[UPGRADE_COUNT] = {
    "10$ discount/Scientists, 10$ discount/Laboratory",
    "+5 Production capacity",
    "Can build Titanium (~10$) factory; 5$ discount/Warehouse, 5$ discount/Nodule, 15$ discount/Outpost",
    "+3 Colonist capacity",

    "1 free Research (~13$) card/turn",    
    "1 free Microbiotics (~17$) card/turn",
    "1 free Robot, can buy and use Robots",
    "1 free Research factory; can build Research (~13$) factories",
    "Colonists cost 5; 10$ discount/Outpost",
    "+5 Colonist capacity, +5 Production capacity, 1 free Titanium (~10$) factory",
    
    "1 Orbital Medicine (~30$) card/turn when manned by colonist",
    "1 Ring Ore (~40$) card/turn when manned by colonist",
    "1 Moon Ore (~50$) card/turn when manned by colonist"
};

const char *basicRules =
    "\nO U T P O S T\n\n"
    "Based on the board game designed by James Hlavaty, current edition published by Stronghold Games (www.strongholdgames.com).\n\n"
    "The goal of Outpost is to reach 75 victory points before any of your opponents. "
    "You earn victory points by purchasing Colony Upgrades and operating Factories.\n\n"
    "The first four upgrades are available during Era 1; once somebody reaches 10VP, six more Era 2 upgrades become available. "
    "Finally, once somebody reaches 30-40VP (depending on number of players) the last three Era 3 upgrades become available.\n\n"

    "You earn money by operating Factories; you can spend that money on Colony Upgrades, Factories, Colonists, and Robots. "
    "You need Colonists or Robots in order to operate Factories. "
    "At the beginning of the game, you begin with 3 colonists and can hold up to 5 of them; additional ones cost 10 unless you buy Ecoplants, which lowers their price to 5. "
    "You can raise the Colonist limit by purchasing Nodules(+3) and Outposts(+5).\n\n"
    "If you buy the Robotics Colony Upgrade, you will gain the ability to purchase and use Robots to operate your factories as well. "
    "However, you can never use more than one Robot per Colonist per Robotics upgrade. "
    "The three Era 3 factories can only be operated by Colonists, but those Colonists do not count against Colonist capacity.\n\n"

    "At the beginning of each round, players are assigned turn order in descending number of victory points; face value of upgrades breaks ties. "
    "Each player draws one production card for each factory that was operated last turn, along with bonus production cards from Scientist and Orbital Lab upgrades. "
    "For Water, Titanium, and New Chemicals, if you have at least 4 factories of that type you may instead choose to draw a Mega card with a fixed value (30, 44, or 88, slightly "
    "more than four times the average production). These Mega cards still count as four cards for your hand limit. Research and Microbiotics production cards do NOT count against your hand limit. "
    "After that, any player over their Production capacity (their hand limit, which starts at 10 but can be raised by Warehouse and Outpost upgrades) must discard "
    "excess cards down to their hand limit.\n\n"

    "On their turn, a player chooses to auction zero or more upgrades; they declare an opening bid (at or above that upgrade's minimum bid) and "
    "bidding proceeds around the table; each player can pass or raise the bid. The auction ends when all other players pass after a bid, but "
    "a player who passed earlier in the auction can re-bid again if they get a chance. Winner pays for the auction by discarding production "
    "cards from their hand, less any discounts, but no change is given.\n\n"

    "Players can win auctions when it is not their turn; if the auction came with a free factory, they may immediately move an operator to staff it even if they already had their turn this round. "
    "Players can win any number of auctions in a given round, if they can afford to pay for them. "
    "Once a player cannot or chooses not to initiate any more auctions, they may now purchase factories, colonists, and robots. "
    "You pay for each type of item separately, but you can buy more than one of an item and pay only that total amount. "
    "Finally you may allocate colonists (and robots, if available) to your factories.\n\n"

    "Whenever you pay for something, you may always overpay but you will never get change back. The computer will recommend the best set of cards that satisfies your debt. "
    "During auctions, the computer will tell you the minimum bid and the actual amount you can exactly pay, which is sometimes higher.\n\n"

    "You can purchase Ore and Water factories at any time. You can purchase Titanium factories if you own Heavy Equipment upgrade.  You can purchase Research "
    "factories if you own Laboratory. You can purchase New Chemicals factories only if at least one Research card is used to pay for each one (which means "
    "that you must own either Scientists or Laboratory). Microbiotics, Orbital Medicine, Ring Ore, and Moon Ore factories cannot be directly purchased, "
    "but you receive one factory free with each matching Colony Upgrade. As a special case on the first turn of the game, you may turn in all six of your original "
    "production cards for one Water factory even if you couldn't otherwise afford one.\n\n"
    ;
    


const byte_t upgradeCosts[UPGRADE_COUNT] = { 15,25,30,25,40, 50,50,80,30,100, 120,160,200 };

#define NELEM(x)    (sizeof(x)/sizeof(x[0]))

static const unsigned EMPTY = 0x7FFFFFFF;

static unsigned readUnsigned() {
    string answer;
    getline(cin,answer);
    table.hadInput();
    if (answer.size())
        return atoi(answer.c_str());
    else
        return EMPTY;
}

static char readLetter() {
    string answer;
    getline(cin,answer);
    table.hadInput();
    return toupper(answer[0]);
}


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

struct player_t;

bool anyHumansInGame;

class brain_t {
protected:
    string name;
    player_t *player;
    money_t findBestCards(money_t cost,vector<card_t> &hand,amt_t minResearchCards,size_t *bestCardsOut);
public:
    brain_t(string n) : name(n) { }
    virtual ~brain_t() { }
    const string& getName() const { return name; }
    void setPlayer(player_t &p) { player = &p; }

    void displayProductionCards(vector<card_t> &hand,size_t annotateMask = 0) {
        for (cardIndex_t i=0; i<hand.size(); i++,annotateMask>>=1)
            active << i << ". " << (annotateMask&1?"*":"") << factoryNames[hand[i].prodType] << "/" << int(hand[i].value) << "\n";
    }

    void displayProductionCardsOnSingleLine(vector<card_t> &hand,size_t annotateMask = 0) {
        if (hand.size()) {
            active << "[";
            for (cardIndex_t i=0; i<hand.size(); i++,annotateMask>>=1)
                active << (annotateMask&1?" *":" ") << factoryNames[hand[i].prodType] << "/" << int(hand[i].value);
            active << " ]\n";
        }
        else
            active << "[ ** no production cards ** ]\n";
    }

    virtual amt_t wantMega(productionEnum_t which,amt_t maxMega) = 0;
    virtual cardIndex_t pickDiscard(vector<card_t> &hand) = 0;
    virtual void yourTurn() { }
    virtual cardIndex_t pickCardToAuction(vector<card_t> &hand,vector<upgradeEnum_t> &upgradeMarket,money_t &bid) = 0;
    virtual money_t raiseOrPass(vector<card_t> &hand,upgradeEnum_t upgrade,money_t bid) = 0;
    virtual money_t payFor(money_t cost,vector<card_t> &hand,bank_t &bank,amt_t minimumResearchCards); // returns actual amount paid which may be higher
    virtual amt_t purchaseFactories(const vector<byte_t> &maxByType,productionEnum_t &whichFactory) = 0;
    virtual amt_t purchaseColonists(money_t perColonist,amt_t maxAllowed) = 0;
    virtual amt_t purchaseRobots(money_t perRobot,amt_t maxAllowed,amt_t maxUsable) = 0;
    virtual void assignPersonnel();
    virtual void moveOperatorToNewFactory(productionEnum_t newFactory);
};

struct player_t {
    vector<card_t> hand;
    typedef vector<card_t>::iterator cardIt_t;
    byte_t colonists, colonistLimit, extraColonistLimit, robots, productionSize, productionLimit;
    money_t totalCredits, totalUpgradeCosts;
    vector<byte_t> factories;
    vector<byte_t> mannedByColonists;
    vector<byte_t> mannedByRobots;
    vector<byte_t> upgrades;
    brain_t *brain;

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
        if (discard.returnToDiscard)  // mega cards (and virtual cards) don't go into same deck
            bank[discard.prodType].discardCard(discard.value);
    }

    void drawProductionCards(bank_t &bank,bool firstTurn) {
        // have to decide whether to draw megaproduction cards first
        // this isn't strictly necessary according to the rules since we don't display any cards
        // until all have already been drawn, but it's more of a user interface issue where we
        // have to stop and ask in the middle of displaying status text.
        vector<byte_t> megaCount;
        megaCount.resize(PRODUCTION_COUNT);
        for (int i=ORE; i<PRODUCTION_COUNT; i++) {
            int maxMega = (mannedByColonists[i] + mannedByRobots[i]) / 4;
            if (bank[i].getMegaValue() && maxMega)
                megaCount[i] = brain->wantMega((productionEnum_t)i,maxMega);
        }
                         
        table << getName() << " draws ";
        bool firstCard = true;
        for (int i=ORE; i<PRODUCTION_COUNT; i++) {
            int toDraw = mannedByColonists[i] + mannedByRobots[i];
            // special cases: each scientist upgrade produces a research card without being populated.
            // same for microbiotics.
            if (i==RESEARCH)
                toDraw += upgrades[SCIENTISTS];
            else if (i==MICROBIOTICS)
                toDraw += upgrades[ORBITAL_LAB];
            if (firstTurn)
                toDraw *= 2;   // double production on first turn
            if (megaCount[i]) {
                card_t megaCard = { bank[i].getMegaValue(), i, 4, false };
                table << (firstCard?"":", ") << megaCount[i] << " " << factoryNames[i] << " Mega";
                firstCard = false;
                while (megaCount[i]) {
                    addCard(megaCard);
                    toDraw -= 4;
                    megaCount[i]--;
                }
            }
            if (toDraw) {
                table << (firstCard?"":", ") << toDraw << " " << factoryNames[i] << (toDraw>1?" cards":" card");
                firstCard = false;
            }
            while (toDraw) {
                addCard(bank[i]);
                --toDraw;
            }
        }
        if (firstCard)
            table << " no production cards!\n";
        else
            table << ".\n";
        sort(hand.begin(),hand.end());
    }
    
    void discardExcessProductionCards(bank_t &bank) {
        amt_t discarded = 0;
        while (productionSize > productionLimit) {
            discardCard(bank,brain->pickDiscard(hand));
            ++discarded;    // some cards take four slots
        }
        if (discarded)
            table << getName() << " discarded " << discarded << " production card" << (discarded>1?"s":"") << ".\n";
    }
    
    amt_t getRobotLimit() const { return upgrades[ROBOTICS] * (colonistLimit + extraColonistLimit); }
    
    amt_t getRobotsInUse() const { 
        amt_t result = 0;
        // robots cannot ever be in era 3 upgrades.
        for (int i=ORE; i<ORBITAL_MEDICINE; i++)
            result += mannedByRobots[i];
        return result;
    }
        
    void newFactoryFromUpgrade(productionEnum_t which) {
        factories[which]++;
        brain->moveOperatorToNewFactory(which);
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
        else if (upgrade == ROBOTICS) {
            robots++;
            mannedByRobots[UNUSED]++;
        }
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
        for (int i=DATA_LIBRARY; i<UPGRADE_COUNT; i++)
            vps += vpsForUpgrade[i] * upgrades[i];
        // now include victory points for factories which are manned
        // note that microbiotics is counted during upgrades and can never be manned.
        // scientists are counted during upgrades as well but you can also buy/man research factories so they're counted here.
        for (int i=ORE; i<PRODUCTION_COUNT; i++)
            vps += vpsForMannedFactory[i] * (mannedByColonists[i] + mannedByRobots[i]);
        return vps;
    }

    void getMaxFactories(vector<byte_t> &outFactories) {
        outFactories.clear();
        outFactories.resize(PRODUCTION_COUNT);
        outFactories[ORE] = totalCredits / factoryCosts[ORE];
        outFactories[WATER] = totalCredits / factoryCosts[WATER];
        if (upgrades[HEAVY_EQUIPMENT])
            outFactories[TITANIUM] = totalCredits / factoryCosts[TITANIUM];
        if (upgrades[LABORATORY])
            outFactories[RESEARCH] = totalCredits / factoryCosts[RESEARCH];
        // Each new chemicals factory must be paid for with at least one research card
        int numResearch = 0;
        for (cardIt_t i=hand.begin(); i!=hand.end(); i++)
            numResearch += (i->prodType == RESEARCH);
        outFactories[NEW_CHEMICALS] = totalCredits / factoryCosts[NEW_CHEMICALS];
        if (outFactories[NEW_CHEMICALS] > numResearch)
            outFactories[NEW_CHEMICALS] = numResearch;
        // MICROBIOTICS, ORBITAL_MEDICINE, RING_ORE, and MOON_ORE factories are never
        // directly purchased; they are part of upgrade purchases.
    }
    
    void payFor(money_t cost,bank_t &bank,int minResearchCards) {
        brain->payFor(cost,hand,bank,minResearchCards);
    }
    
    void yourTurn() {
        brain->yourTurn();
    }
    
    cardIndex_t pickCardToAuction(vector<upgradeEnum_t> &upgradeMarket,money_t &bid) {
        return brain->pickCardToAuction(hand,upgradeMarket,bid);
    }
    
    money_t raiseOrPass(upgradeEnum_t upgrade,money_t minBid) {
        return brain->raiseOrPass(hand,upgrade,minBid);
    }

    void purchaseFactories(bool firstTurn,bank_t &bank) {
        for (;;) {
            vector<byte_t> forPurchase;
            getMaxFactories(forPurchase);
            productionEnum_t whichFactory;
            // special case - on first turn we can trade in all cards for a water factory even if we couldn't normally afford one.
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
                // if it's the first turn water special case, pay what we have instead of its actual cost
                brain->payFor(firstTurn&&whichFactory==WATER? totalCredits : numToBuy * factoryCosts[whichFactory],hand,bank,whichFactory==NEW_CHEMICALS? numToBuy : 0);
                table << getName() << " bought " << numToBuy << " " << factoryNames[whichFactory] << " factor" << (numToBuy>1?"ies":"y") << ".\n";
                factories[whichFactory] += numToBuy;
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
                table << getName() << " bought " << purchased << " colonist" << (purchased>1?"s":"") << ".\n";
                colonists += purchased;
                mannedByColonists[UNUSED] += purchased;
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
                table << getName() << " bought " << purchased << " robot" << (purchased>1?"s":"") << ".\n";
                robots += purchased;
                mannedByRobots[UNUSED] += purchased;
                payFor(purchased * price,bank,0);
            }
        }        
        brain->assignPersonnel();
    }
    
    void displayHoldings() {
        if (totalUpgradeCosts) {
            table << getName() << "'s upgrades:";
            for (int i=DATA_LIBRARY; i<UPGRADE_COUNT; i++)
                if (upgrades[i])
                    table << " " << int(upgrades[i]) << "/" << upgradeNames[i] << ";";
            table << "\n";
        }
        table << getName() << "'s factories:";
        for (int i=ORE; i<PRODUCTION_COUNT; i++)
            if (factories[i])
                table << " " << int(factories[i]) << "/" << factoryNames[i] << "(" << int(mannedByColonists[i]) << "+" << int(mannedByRobots[i]) << ");";
        table << " Unused(" << int(mannedByColonists[UNUSED]) << "+" << int(mannedByRobots[UNUSED]) << ");\n";
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
    
    void getExpectedMoneyInHand(int &minPossible,int &maxPossible) const {
        static const byte_t minPerCard[PRODUCTION_COUNT] = { 1,4,7,9,14,14,20,30,40 };
        static const byte_t maxPerCard[PRODUCTION_COUNT] = { 5,10,13,17,20,26,40,50,60 };
        
        minPossible = 0;
        maxPossible = 0;
        for (vector<card_t>::const_iterator c=hand.begin(); c!=hand.end(); c++) {
            // if it's returned to discard we cannot know what it may be.
            // if it's not returned to discard it's an "average" card or mega card, either way we know its exact value
            minPossible += c->returnToDiscard? minPerCard[c->prodType] : c->value;
            maxPossible += c->returnToDiscard? maxPerCard[c->prodType] : c->value;
        }
    }
    
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
            int i;
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
            for (int i=DATA_LIBRARY; i<SPACE_STATION; i++)
                upgradeDrawPiles.push_back(upgrades_1_10[playerCount]);
            for (int i=SPACE_STATION; i<UPGRADE_COUNT; i++)
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
        playerOrder.reserve(players.size());
        for (playerIndex_t i=0; i<players.size(); i++) {
            playerPos_t p = { players[i].computeVictoryPoints(), players[i].getTotalUpgradeCosts(), rand(), i };
            playerOrder.push_back(p);
        }
        // sort in ascending order (default uses operator<)
        sort(playerOrder.begin(),playerOrder.end());
    }
    
    void displayPlayerOrder() {
        for (playerIndex_t pi=0; pi<playerOrder.size(); pi++) {
            player_t &p = players[playerOrder[pi].selfIndex];
            table << "#" << pi+1 << ". " << p.getName() << "; " << playerOrder[pi].vps << " VPs, upgrades:";
            bool anyUpgrades = false;
            for (int i=DATA_LIBRARY; i<UPGRADE_COUNT; i++)
                if (p.upgrades[i]) {
                    table << " " << int(p.upgrades[i]) << "/" << upgradeNames[i] << ";";
                    anyUpgrades = true;
                }
            if (!anyUpgrades)
                table << " [none];";
            table << " factories:";
            for (int i=ORE; i<PRODUCTION_COUNT; i++)
                if (p.factories[i])
                    table << " " << int(p.factories[i]) << "/" << factoryNames[i] << "(" << int(p.mannedByColonists[i]) << "+" << int(p.mannedByRobots[i]) << ");";
            table << " Unused(" << int(p.mannedByColonists[UNUSED]) << "+" << int(p.mannedByRobots[UNUSED]) << "); ";
            int minPos, maxPos;
            p.getExpectedMoneyInHand(minPos, maxPos);
            if (minPos == maxPos)
                table << "exactly " << minPos << "$ in hand.\n";
            else
                table << minPos << "-" << maxPos << "$ in hand.\n";
         }
            
    }

    void replaceUpgradeCards() {
        // figure out whether the market is totally empty or not
        bool marketEmpty = upgradeMarket.size() == 0;
        for (int i=DATA_LIBRARY; i<(era==1?SCIENTISTS:SPACE_STATION) && marketEmpty; i++)
            if (upgradeDrawPiles[i])
                marketEmpty = false;
        static const byte_t minVpsForEra3[] = { 0,0,40,35,40,30,35,40,30,35 };

        // figure out which era we're in now.
        if (era == 1 && (playerOrder[0].vps >= 10 || (marketEmpty && previousMarketEmpty))) {
            table << "*** Entering era 2!\n";
            era = 2;
        }
        else if (era == 2 && (playerOrder[0].vps >= minVpsForEra3[players.size()] || (marketEmpty && previousMarketEmpty))) {
            table << "*** Entering era 3!\n";
            era = 3;
        }
        previousMarketEmpty = marketEmpty;
        
        while (upgradeMarket.size() < players.size()) {
            // First check if any roll has a chance to succeeed
            int firstMarket = era==3? WAREHOUSE : DATA_LIBRARY;
            int marketSize = era==3? 12 : era==2? 10 : 4;
            bool anyValid = false;
            // note that we start at zero because even in Era 3 an unpurchased Data Library could still come up for auction.
            for (int i=DATA_LIBRARY; !anyValid && i<firstMarket+marketSize; i++)
                // if there is a card left of this type and we 
                if (upgradeDrawPiles[i] && currentMarketCounts[i] != marketLimit)
                    anyValid = true;
            
            if (!anyValid)
                break;
            
            int roll = (firstMarket + (rand() % marketSize));
            for(;;) {
                if (upgradeDrawPiles[roll] && currentMarketCounts[roll] != marketLimit)
                    break;
                else if (roll) // try next upgrade downward
                    --roll;
                else    // pick a new roll if we hit the bottom of the list
                    roll = (firstMarket + (rand() % marketSize));
            }
            
            table << upgradeNames[roll] << " added to market (" << upgradeHelp[roll] << ").\n";
            upgradeDrawPiles[roll]--;
            currentMarketCounts[roll]++;
            upgradeMarket.push_back((upgradeEnum_t)roll);
        }
        table << "Remaining upgrades:";
        bool anyUpgradesRemaining = false;
        for (int i=DATA_LIBRARY; i<UPGRADE_COUNT; i++)
            if (upgradeDrawPiles[i]) {
                table << " " << int(upgradeDrawPiles[i]) << "/" << upgradeNames[i] << ";";
                anyUpgradesRemaining = true;
            }
        if (!anyUpgradesRemaining)
            table << " [none]";
        table << "\n";
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
        /* if (upgradeMarket.size() == 1)
            table << "There is 1 card available for auction.\n";
        else
            table << "There are " << upgradeMarket.size() << " cards available for auction.\n"; */
        displayPlayerOrder();
        while (upgradeMarket.size() && (nextAuction = players[selfIndex].pickCardToAuction(upgradeMarket,bid)) != upgradeMarket.size()) {
            // remove the card from the market
            upgradeEnum_t upgrade = upgradeMarket[nextAuction];
            upgradeMarket.erase(upgradeMarket.begin() + nextAuction);
            currentMarketCounts[upgrade]--;
            table << players[selfIndex].getName() << " places " << upgradeNames[upgrade] << " up for auction with an opening bid of " << bid << ".\n";
            
            // run the auction until everybody else passes.
            unsigned numPassedInARow = 0;
            playerIndex_t highBidder = selfIndex;
            playerIndex_t nextBidder = selfIndex;
            for (;;) {
                if (++nextBidder == players.size())
                    nextBidder = 0;
                money_t newBid = players[nextBidder].raiseOrPass(upgrade,bid+1);
                // somebody wants to bid?
                if (newBid) {
                    highBidder = nextBidder;
                    bid = newBid;
                    numPassedInARow = 0;
                    table << players[highBidder].getName() << " raises the bid to " << bid << ".\n";
                }
                // everybody else has passed?
                else {
                    table << players[nextBidder].getName() << " passes.\n";
                    if (++numPassedInARow == players.size()-1)
                        break;
                }
            }
            
            table << players[highBidder].getName() << " wins the auction for " << upgradeNames[upgrade] << " with " << bid << " credits.\n";
            money_t discount = players[highBidder].computeDiscount(upgrade);
            if (bid > discount)
                players[highBidder].payFor(bid - discount,bank,0);
            players[highBidder].addUpgrade(upgrade);
            
            displayPlayerOrder();
        }
    }

    void performPlayerTurns(bool firstTurn) {
        for (playerOrderIt_t i=playerOrder.begin(); i!=playerOrder.end(); i++) {
            table << "\n=== " << players[i->selfIndex].getName() << "'s turn ===\n\n";
            players[i->selfIndex].yourTurn();
            auctionUpgradeCards(i->selfIndex);
            players[i->selfIndex].purchaseFactories(firstTurn,bank);
            players[i->selfIndex].purchaseAndAssignPersonnel(bank);
        }        
    }

    bool checkVictoryConditions() {
        computeVictoryPoints();
        if (playerOrder.front().vps < 75)
            return false;
        
        table << "\n\n=== GAME OVER ===\n\nFinal rankings:\n";
        displayPlayerOrder();
        return true;
    }
};

void brain_t::assignPersonnel() {
    amt_t robotLimit = player->getRobotLimit();
    
    // everybody outta the pool!
    for (int i=ORE; i<PRODUCTION_COUNT; i++) {
        player->mannedByColonists[UNUSED] += player->mannedByColonists[i];
        player->mannedByColonists[i] = 0;
        player->mannedByRobots[UNUSED] += player->mannedByRobots[i];
        player->mannedByRobots[i] = 0;
    }
    // assign to factories from the top down, favoring humans first
    for (int i=MOON_ORE; i>=ORE; i--) {
        while (player->mannedByColonists[i] < player->factories[i] && player->mannedByColonists[UNUSED]) {
            player->mannedByColonists[i]++;
            player->mannedByColonists[UNUSED]--;
        }
    }
    // fill in anything remaining with robots but only up to the limit
    for (int i=ORBITAL_MEDICINE; i>=ORE && robotLimit; i--) {
        while (robotLimit && (player->mannedByColonists[i] + player->mannedByRobots[i]) < player->factories[i] && player->mannedByRobots[UNUSED]) {
            player->mannedByRobots[i]++;
            player->mannedByRobots[UNUSED]--;
            --robotLimit;
        }
    }
}

money_t brain_t::payFor(money_t cost,vector<card_t> &hand,bank_t &bank,amt_t minResearchCards) {
    size_t best;

    if (debugLevel > 0) {
        cout << name << " needs to pay at least " << cost << " (of " << player->getTotalCredits() << ") from:\n\t";
        displayProductionCardsOnSingleLine(hand);
    }
    
    money_t paid = findBestCards(cost,hand,minResearchCards,&best);
    cardIndex_t base = 0;
    table << name << " needs to pay " << cost << " and discards:";
    while (best) {
        if (best & 1) {
            table << " " << factoryNames[hand[base].prodType] << "/" << int(hand[base].value);
            player->discardCard(bank, base);
        }
        else
            base++;
        best >>= 1;
    }
    table << ".\n";
    return paid;
}

money_t brain_t::findBestCards(money_t cost,vector<card_t> &hand,amt_t minResearchCards,size_t *bestOut) {
    // i doubt a hand size of more than 31 cards is really possible.
    // note this code could get pretty slow for bigger hands though since it's exhaustive.
    size_t width = hand.size();
    if (width > 25)
        width = 25;     // keep this from taking a very long time
    size_t handMax = 1U << width;
    size_t bestCards = width;     // best cards is the entire hand.
    size_t best = handMax - 1;    // best match is the entire hand.
    amt_t bestValue = player->getTotalCredits();  // best value is the entire hand.
    // don't waste time if it's an exact match
    if (bestValue > cost) {
        for (unsigned i=1; i<handMax; i++) {
            unsigned test = i;
            amt_t testValue = 0, testResearchCount = 0, testMinValue = 0;
            size_t testCards = 0;
            // determine the value of this permutation, and remember how many cards there were.
            for (unsigned j=0; j<width; j++,test>>=1) {
                if (test & 1) {
                    if (!testMinValue)
                        testMinValue = hand[j].value;
                    testValue += hand[j].value;
                    testResearchCount += hand[j].prodType == RESEARCH;
                    testCards += hand[j].handSize;
                }
            }
            // if this is better than our previous best guess, remember it.
            // also attempt to maximize the number of cards we'd be discarding.
            // but don't throw out cards just for the sake of tossing them
            if (testValue >= cost && testValue - testCards < bestValue - bestCards && testValue - testMinValue < cost && testResearchCount >= minResearchCards) {
                best = i;
                bestValue = testValue;
                bestCards = testCards;
            }
        }
    }
    if (bestOut)
        *bestOut = best;
    return bestValue;
}

void brain_t::moveOperatorToNewFactory(productionEnum_t dest) {
    bool robotCanOperate = (dest < ORBITAL_MEDICINE);
    // always choose an unused colonist first
    if (player->mannedByColonists[UNUSED]) {
        table << name << " moves an unused colonist to operate the new " << factoryNames[dest] << ".\n";
        player->mannedByColonists[UNUSED]--;
        player->mannedByColonists[dest]++;
    }
    // next choose an unused robot, but only if we're not at the limit yet and the robot can work there.
    else if (player->mannedByRobots[UNUSED] && player->getRobotsInUse() < player->getRobotLimit() && robotCanOperate) {
        table << name << " moves an unused robot to operate the new " << factoryNames[dest] << ".\n";
        player->mannedByRobots[UNUSED]--;
        player->mannedByRobots[dest]++;
    }
    else {
        // find the first available colonist or robot at any factory "worse" than this one
        for (int i=ORE; i<dest; i++) {
            if (player->mannedByColonists[i]) {
                table << name << " moves a colonist from " << factoryNames[i] << " to operate the new " << factoryNames[dest] << ".\n";
                player->mannedByColonists[i]--;
                player->mannedByColonists[dest]++;
                return;
            }
            else if (robotCanOperate && player->mannedByRobots[i]) {
                table << name << " moves a robot from " << factoryNames[i] << " to operate the new " << factoryNames[dest] << ".\n";
                player->mannedByRobots[i]--;
                player->mannedByRobots[dest]++;
                return;
            }
        }
        table << name << " didn't find a suitable operator for the new " << factoryNames[dest] << ".\n";
    }
      
}

/*
    Design thoughts for better AI:
    - AI shouldn't cheat; it should use only public information.
    - However, it can have perfect memory.
    - You know the average amount somebody will have based on their hand.
    - You can also know the least and most they may have.
    - Mega cards and "average" virtual cards are public information as well.
    - Discards are public information; that combined with known distributions and what you're holding can give you tighter bounds on your opponents.
 
    - The long-term goal is to make lots of money and score lots of victory points.
    - Era 1 purchases help dictate your mid-game strategy.
    - Data Library makes Research more viable.
    - Warehouse makes buying cheaper factories or saving up for bigger purchases earlier more viable.
    - Heavy Equipment should be purchased early for maximum benefit.
    - Nodule makes buying cheaper factories more viable.
    - If you have Heavy Equipment, you don't need Scientists or Orbital Lab as badly.
    - Scientists is always worth face value since Research factories must be manned and cost 30$.
    - But Laboratory includes a free factory (so effectively 50$) and is worth 5VP on its own.
    - Ecoplants is pretty cheap VP
    - Outpost can easily have discount of 10-25$, and includes Titanium factory (30$); critical if you
      didn't get Robotics
    - Era 3 technologies are all good, but don't let somebody else get them too cheaply.
 
    - Every upgrade has an expected value based on victory points and factory potential include; this
      expected value is somewhat dependent on what other technologies you own.
 
    - On last turn of game, any leftover cash should buy best factories possible assuming you can man them.
    - If you have 20$, buy Ore + Operator (only 15$ if Ecoplants), or Water and shift operator from Ore
    - If you have 40$, buy Titanium + Operator
    - If you have 70$, buy New Chem + Operator
 */
class computerBrain_t: public brain_t {
    const game_t &game;
    int lastFactory;
public:
    computerBrain_t(string name,const game_t &theGame) : brain_t(name), game(theGame) { } 
    amt_t wantMega(productionEnum_t,amt_t maxMega) { 
        // this decision is hard -- need to factor in what cards we think are left in the deck,
        // and whether we're making any big purchases this turn, and whether we'd be over our
        // hand limit and have to discard.
        // for now, just do it once in a while.
        return (rand() & 3) == 0; 
    }
    cardIndex_t pickDiscard(vector<card_t> &hand) {
        // Just pick the cheapest card we have.  (cards are always in sorted order)
        // Note that we could end up discarding a research card we needed to buy research factory!
        return 0;
    }
    void yourTurn() {
        // reset variable remembering highest factory we purchased
        lastFactory = NEW_CHEMICALS+1;
    }
    cardIndex_t pickCardToAuction(vector<card_t> &hand,vector<upgradeEnum_t> &upgradeMarket,money_t &bid) {
        // figure out which things we can actually afford.
        vector<byte_t> order;
        for (unsigned i=0; i<upgradeMarket.size(); i++) {
            upgradeEnum_t upgrade = upgradeMarket[i];
            if (player->getTotalCredits() + player->computeDiscount(upgrade) >= upgradeCosts[upgrade])
                order.push_back(i);
        }
        if (!order.size())
            return upgradeMarket.size();
        cardIndex_t which = order[rand() % order.size()];
        // TODO: if we suspect we know what people can actually afford, we should raise our bid slightly
        // if we're already near our limit.
        bid = findBestCards(upgradeCosts[upgradeMarket[which]],hand,0,0);
        return which;
    }
    money_t raiseOrPass(vector<card_t> &hand,upgradeEnum_t upgrade,money_t minBid) {
        // if we can't afford a higher bid, bail out now.
        if (player->getTotalCredits() < minBid)
            return 0;
        // random chance we pass anyway, moreso if we already have one or more
        if ((rand() & 3) + player->upgrades[upgrade] > 2)
            return 0;
        vector<card_t> handCopy(hand);
        amt_t discount = player->computeDiscount(upgrade);
        // handle the (unlikely) case that it's free
        if (discount >= minBid)
            return discount;
        else    // find the closest match
            return findBestCards(minBid - discount,hand,0,0) + discount;
    }
    amt_t adjustAmountIfBigMoney(money_t each,amt_t maxAllowed,amt_t actualWanted,amt_t minResearchCards = 0) {
        money_t expectedSpent = each * actualWanted;
        money_t actualSpent = findBestCards(expectedSpent,player->hand,minResearchCards,NULL);
        // If we're wasting enough money that we would have gotten something free, try adjusting the amount.
        while (actualSpent - expectedSpent >= each && actualWanted < maxAllowed) {
            expectedSpent += each;
            ++actualWanted;
            if (debugLevel > 0)
                debug << name << " can afford an extra thing based on cash in hand.\n";
        }
        // Finally, don't buy anything if we're wasting more than half the purchase price
        if (actualSpent - expectedSpent > (actualSpent>>1)) {
            if (debugLevel > 0)
                debug << name << " decides not to buy any after all based on cash in hand.\n";
            actualWanted = 0;
        }
        return actualWanted;
    }
    amt_t purchaseFactories(const vector<byte_t> &maxByType,productionEnum_t &whichFactory) {
        // need more smarts here... shouldn't keep buying factories that we have no hope of staffing.
        while (lastFactory != ORE) {
            --lastFactory;
            if (maxByType[lastFactory] && player->factories[lastFactory] < player->mannedByColonists[lastFactory] + player->mannedByRobots[lastFactory] + 2) {
                whichFactory = (productionEnum_t)lastFactory;
                return adjustAmountIfBigMoney(factoryCosts[lastFactory],maxByType[lastFactory],(maxByType[lastFactory] + 1) / 2,lastFactory==NEW_CHEMICALS?maxByType[lastFactory]:0);
            }
        }
        return 0;
    }
    amt_t purchaseColonists(money_t perColonist,amt_t maxAllowed) {
        // don't buy colonists if we already have some we haven't used yet.
        return (player->mannedByColonists[UNUSED])? 0 : adjustAmountIfBigMoney(perColonist,maxAllowed,(maxAllowed+1)/2);
    }
    amt_t purchaseRobots(money_t perRobot,amt_t maxAllowed,amt_t maxUsable) {
        // don't buy robots if we already have some we haven't used yet.
        return (player->mannedByRobots[UNUSED])? 0 : adjustAmountIfBigMoney(perRobot,maxAllowed,(maxAllowed+1)/2);
    }
};

class playerBrain_t: public brain_t {
public:
    playerBrain_t(string name) : brain_t(name) { }
    amt_t wantMega(productionEnum_t t,amt_t maxMega) {
        for (;;) {
            active << name << ", how many megaproduction cards for " << factoryNames[t] << " do you want (empty for none, at most " << maxMega << ")? ";
            amt_t answer = readUnsigned();
            if (answer == EMPTY)
                return 0;
            else if (answer <= maxMega)
                return answer;
            else
                active << "That is too many megaproduction cards.\n";
        }
    }
   cardIndex_t pickDiscard(vector<card_t> &hand) {
        active << name << ", you are over your hand limit.\n";
        displayProductionCards(hand);
        cardIndex_t which = 0;
        do {
            active << name << ", which card to you want to discard? ";
            which = readUnsigned();
        } while (which >= hand.size());
        return which;
    }
    cardIndex_t pickCardToAuction(vector<card_t> &hand,vector<upgradeEnum_t> &upgradeMarket,money_t &bid) {
        for (;;) {
            for (cardIndex_t i=0; i<upgradeMarket.size(); i++) {
                active << i << ". " << upgradeNames[upgradeMarket[i]] << " (min bid is " << int(upgradeCosts[upgradeMarket[i]]);
                int discount = player->computeDiscount(upgradeMarket[i]);
                if (discount)
                    active << ", your discount is " << discount;
                active << ")\n";
            }
            displayProductionCardsOnSingleLine(hand);
            active << name << ", pick a card to auction or empty line for none? (you have " << player->getTotalCredits() << ") ";
            for (;;) {
                cardIndex_t which = readUnsigned();
                if (which == EMPTY)
                    return upgradeMarket.size();
                else if (which < upgradeMarket.size()) {
                    bid = raiseOrPass(hand,upgradeMarket[which],upgradeCosts[upgradeMarket[which]]);
                    if (!bid) {      // couldn't make valid opening bid, bounce them to selection menu
                        active << "You cannot afford that.\n";
                        break;
                    }
                    else    // otherwise pass valid selection and bid to caller
                        return which;
                }
                else
                    active << "That is not a valid choice.  Enter an empty line if you don't wan't to auction anything: ";
            }
        }
    }
    money_t raiseOrPass(vector<card_t> &hand,upgradeEnum_t upgrade,money_t minBid) {
        money_t discount = player->computeDiscount(upgrade);
        // don't bother asking if we cannot afford one higher than current bid
        if (player->getTotalCredits() < minBid - discount)
            return 0;

        size_t best;
        money_t recommendedBid = findBestCards(minBid-discount,hand,0,&best) + discount;
        displayProductionCardsOnSingleLine(hand,best);
        active << name << ", you have " << player->getTotalCredits() << " and a discount of " << discount << " on this upgrade.\n";
        if (minBid == upgradeCosts[upgrade])
            active << name << ", please pick an opening bid for " << upgradeNames[upgrade] << " of at least " << minBid << " (you can pay exactly " << recommendedBid << ") or empty line or 0 to pass: ";
        else
            active << name << ", the minimum bid for " << upgradeNames[upgrade] << " is now at " << minBid << " (you can pay exactly " << recommendedBid << "), or empty line or 0 to pass: ";
        for (;;) {
            money_t newBid = readUnsigned();
            if (newBid == 0 || newBid == EMPTY)
                return 0;
            else if (newBid < minBid)
                active << "The bid must either be 0 to pass or something at least " << minBid << ".  Your bid? (default or 0 is pass) ";
            else if (newBid > player->getTotalCredits() + discount)
                active << "You only have " << player->getTotalCredits() << " (with a discount of " << discount << ") and cannot afford that bid.  Your bid? (0 to pass) ";
            else
                return newBid;
        }
    }
    money_t payFor(money_t cost,vector<card_t> &hand,bank_t &bank,amt_t minimumResearchCards) {
        money_t paid = 0;
        
        // if our total money minus our cheapest card is not enough to pay, toss everything
        // note that all other prerequisites would have been met before we got here.
        if (player->getTotalCredits() - hand[0].value < cost) {
            active << name << ", that cost all of your poduction cards.\n";
            paid = player->getTotalCredits();
            while (hand.size())
                player->discardCard(bank,0);
        }
        else while (paid < cost || minimumResearchCards) {
            if (cost >= 0) {
                active << name << ", you need to discard " << cost << " credits worth of cards";
                if (minimumResearchCards)
                    active << " (at least " << minimumResearchCards << " more research cards)\n";
                else
                    active << "\n";
            }
            else
                active << name << ", you still need to discard " << minimumResearchCards << " more research cards!\n";
            size_t best;
            findBestCards(cost-paid,hand,minimumResearchCards,&best);
            displayProductionCards(hand,best);
            active << "Enter a card, by number, to discard: (or nothing to pick defaults) ";
            cardIndex_t which = readUnsigned();
            if (which < hand.size()) {
                cost -= hand[which].value;
                if (hand[which].prodType == RESEARCH && minimumResearchCards)
                    --minimumResearchCards;
                player->discardCard(bank,which);
            }
            else if (which == EMPTY) {
                paid += brain_t::payFor(cost-paid,hand,bank,minimumResearchCards);
                break;
            }
            else
                active << "That was not a valid card choice.\n";
        }
        return paid;
    }
    amt_t purchaseFactories(const vector<byte_t> &maxByType,productionEnum_t &whichFactory) {
        player->displayHoldings();
        for (;;) {
            for (int i=ORE; i<=NEW_CHEMICALS; i++)
                if (maxByType[i])
                    active << i << ". " << factoryNames[i] << " (at most " << int(maxByType[i]) << ", you have " << int(player->factories[i]) << ")" << "\n";
            displayProductionCardsOnSingleLine(player->hand);
            active << name << ", which factory would you like to purchase? (default is none) ";
            whichFactory = (productionEnum_t) readUnsigned();
            if (whichFactory == EMPTY)
                return 0;
            if (whichFactory > NEW_CHEMICALS || !maxByType[whichFactory]) {
                active << "You cannot buy factories of that type.\n";
                continue;
            }
            active << "How many factories would you like to buy?  (default is " << int(maxByType[whichFactory]) << ") ";
            amt_t numToBuy = readUnsigned();
            if (numToBuy == EMPTY)
                return maxByType[whichFactory];
            else if (numToBuy > maxByType[whichFactory]) {
                active << "That's more than you can buy of that type.\n";
                continue;
            }
            return numToBuy;
        }
    }
    amt_t purchaseColonists(money_t perColonist,amt_t maxAllowed) {
        player->displayHoldings();
        for (;;) {
            displayProductionCardsOnSingleLine(player->hand);
            active << name << ", how many colonists do you want to buy at " << perColonist << " each? (at most " << maxAllowed << ", default is none) ";
            amt_t amt = readUnsigned();
            if (amt == EMPTY)
                return 0;
            else if (amt <= maxAllowed)
                return amt;
            else
                active << "That's more than you can buy.\n";
        }
    }
    amt_t purchaseRobots(money_t perRobot,amt_t maxAllowed,amt_t maxUsable) {
        player->displayHoldings();
        for (;;) {
            displayProductionCardsOnSingleLine(player->hand);
            active << name << ", how many robots do you want to buy at " << perRobot << " each? (at most " << maxAllowed << ", of which " << maxUsable << " can currently be used, default is none) ";
            amt_t amt = readUnsigned();
            if (amt == EMPTY)
                return 0;
            else if (amt <= maxAllowed)
                return amt;
            else
                active << "That's more than you can buy.\n";
        }
    }
    void assignPersonnel() {
        // automatically assign personnel first
        brain_t::assignPersonnel();
        amt_t robotLimit = player->getRobotLimit();
        for (;;) {
            active << name << ", here are your current allocations:\n";
            amt_t robotsInUse = player->getRobotsInUse();
            for (int i=ORE; i<PRODUCTION_COUNT; i++) {
                if (player->factories[i])
                    active << i << ". " << factoryNames[i] << ": " << int(player->factories[i]) << " factories manned by " << int(player->mannedByColonists[i]) << " colonists and " << int(player->mannedByRobots[i]) << " robots.\n";
            }
            active << UNUSED << ". Unallocated: " << int(player->mannedByColonists[UNUSED]) << " colonists, " << int(player->mannedByRobots[UNUSED]) << " robots (max allocated is " << robotLimit << ").\n";
            active << "Transfer colonist (c), robot (r), or anything else to finish? ";
            char cmd = readLetter();
            if (cmd != 'C' && cmd != 'R')
                return;
            vector<byte_t> &manned = (cmd == 'C')? player->mannedByColonists : player->mannedByRobots;
            active << "Transfer source? ";
            productionEnum_t src = (productionEnum_t)readUnsigned();
            if (src > UNUSED || !manned[src]) {
                active << "Sorry, that is an invalid or empty transfer source.\n";
                continue;
            }
            active << "Number to transfer? ";
            amt_t xferAmt = readUnsigned();
            if (xferAmt > manned[src]) {
                active << "Sorry, only " << int(manned[src]) << " personnel at that location.\n";
                continue;
            }
            active << "Transfer destination? ";
            productionEnum_t dst = (productionEnum_t)readUnsigned();
            if (dst > UNUSED || (dst != UNUSED && player->factories[dst] < player->mannedByColonists[dst] + player->mannedByRobots[dst] + xferAmt)) {
                active << "Sorry, that is an invalid transfer destination or there isn't enough room there.\n";
                continue;
            }
            else if (dst != UNUSED && src == UNUSED && cmd == 'R' && robotsInUse + xferAmt > robotLimit) {
                active << "Sorry, that would place you over your robot limit of " << robotLimit << ".\n";
                continue;
            }
            else if (dst >= ORBITAL_MEDICINE && dst <= MOON_ORE && cmd == 'R') {
                active << "Sorry, you cannot staff robots at those Special Factories.\n";
                continue;
            }
            else if (src >= ORBITAL_MEDICINE && dst < ORBITAL_MEDICINE && player->colonists > player->colonistLimit) {
                active << "Sorry, you cannot transfer from an era 3 upgrade to a lower upgrade when over your colonist limit.\n";
                continue;
            }
            // otherwise, perform the transfer
            manned[src] -= xferAmt;
            manned[dst] += xferAmt;
        }
    }
};


int main(int argc,char **argv) {
    unsigned playerCount;

    // seed the RNG, but let it be overridden from user input
    unsigned seed = (unsigned) time(NULL);

    if (argc > 1 && !strncmp(argv[1],"-d",2))
        debugLevel = atoi(argv[1]+2);
    
    // display rules if no parameters on command line
    if (argc == 1) {
        table << basicRules;

        table.setLeftMargin(4);
        table << "Upgrade Summary:\n";
        for (int i=0; i<UPGRADE_COUNT; i++) {
            table << upgradeNames[i] << ": Min bid " << upgradeCosts[i] << ", ";
            if (vpsForUpgrade[i])
                table << vpsForUpgrade[i] << "VPs; ";
            table << upgradeHelp[i] << ".\n";
        }
        table << "\nFactory Summary:\n";
        for (int i=0; i<PRODUCTION_COUNT; i++) {
            table << factoryNames[i] << ": ";
            if (factoryCosts[i])
                table << "Cost " << factoryCosts[i] << ", " ;
            table << vpsForMannedFactory[i] << " VPs when operated; " << factoryHelp[i] << ".\n";
        }
        table << "\n";
        table.setLeftMargin(0);
    }
        
    for (;;) {
        table << "Number of players?  (2-9) ";
        playerCount = readUnsigned();
        if (playerCount < 2 || playerCount > 9)
            seed = playerCount;
        else
            break;
    }

    table << "(using " << seed << " as RNG seed)" << "\n";
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
    table << "If you enter an empty string for a name, that and all future players will be run by computer.  ";
    table << "Players should be entered in seating order (aka auction bidding order).\n";
    table.setLeftMargin(4);
    
    bool anyHumans = true;
    for (int i=0; i<playerCount; i++) {
        string name;
        if (anyHumans) {
            table << "Player " << i+1 << " name? ";
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
        else {
            thisBrain = new playerBrain_t(name);
            anyHumansInGame = true;
        }
            
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
        ++round;
        table << "\n\n";
        table << "        =======================\n";
        table << "        ===  R O U N D  " << ((round<10)?" ":"") << round << "  ===\n";
        table << "        =======================\n\n";
        game.displayPlayerOrder();
        game.replaceUpgradeCards();
        game.drawProductionCards();
        game.discardExcessProductionCards();
        game.performPlayerTurns(false);
    } while (!game.checkVictoryConditions());
}
