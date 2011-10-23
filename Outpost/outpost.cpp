#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

enum productionEnum_t { ORE, WATER, TITANIUM, RESEARCH, MICROBIOTICS, NEW_CHEMICALS, ORBITAL_MEDICINE, RING_ORE, MOON_ORE, PRODUCTION_COUNT };

const char *factoryNames[PRODUCTION_COUNT] = { "Ore", "Water", "Titanium", "Research", "Microbiotics", "NewChemicals", "OrbitalMedicine", "RingOre", "MoonOre" };

enum upgradeEnum_t { 
	DATA_LIBRARY, 
	WAREHOUSE, 
	HEAVY_EQUIPMENT, 
	NODULE, 	// Era 1
	SCIENTISTS, 
	ORBIAL_LAB, 
	ROBOTS, 
	LABORATORY, 
	ECOPLANTS, 
	OUTPOST, 	// Era 2
	SPACE_STATION, 
	PLANETARY_CRUISER, 
	MOON_BASE,
	UPGRADE_COUNT
};

const char *upgradeNames[UPGRADE_COUNT] = { "DataLibrary", "Warehouse", "HeavyEquipment", "Nodule", "Scientists", "OrbitalLab", "Robots",
	"Laboratory", "Ecoplants", "Outpost", "SpaceStation", "PlanetaryCruiser", "MoonBase" };


const uint8_t upgradeCosts[UPGRADE_COUNT] = { 15,25,30,25,40, 50,50,80,30,100, 120,160,200 };

#define NELEM(x)	(sizeof(x)/sizeof(x[0]))


inline int d4() { return rand() % 4; }
inline int d10() { return rand() % 10; }
inline int d12() { return rand() % 12; }


struct card_t {
	uint8_t value, 		// biggest card in game is 88, so 7 bits would be enough.
		prodType:4, 	// productionEnum_t
		handSize:3;	// 0 for Research and Microbiotics.  4 for mega water, titanium, or new chem.  1 otherwise.
};

struct cardDistribution_t {
	uint8_t value, count;
};

class productionDeck_t {
	string name;
	vector<uint8_t> deck;
	typedef vector<uint8_t>::iterator deckIt_t;
	vector<uint8_t> discards;
	typedef vector<uint8_t>::iterator discardsIt_t;
	uint8_t prodType;
	uint8_t average;
	uint8_t megaSize;
	uint8_t countsInHandSize;
public:
	void init(productionEnum_t n,const cardDistribution_t *dist,size_t count,uint8_t avg,uint8_t mega,uint8_t isBig) {
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

	uint8_t getMegaValue() const {
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
		if (deck.size() == 0)
			newCard.value = average;
		else {
			// otherwise take the top of the draw deck and consume it, return it to caller
			newCard.value = deck.back();
			deck.pop_back();
		}
		return newCard;
	}
    
    void discardCard(uint8_t value) {
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

typedef vector<productionDeck_t> decks_t;

class brain_t {
protected:
	string name;
public:
	brain_t(string n) : name(n) { }
	virtual ~brain_t() { }
    const string& getName() const { return name; }

	virtual bool wantMega(productionEnum_t) = 0;
    virtual int pickDiscard(vector<card_t> &productionCards) = 0;
};

class player_t {
	vector<card_t> productionCards;
	typedef vector<card_t>::iterator cardIt_t;
	uint8_t colonists, colonistLimit, robots, productionSize, productionLimit;
	uint16_t totalCredits, totalUpgradeCosts;
    uint32_t scaledVictoryPoints;   // victory points times a thousand, plus printed cost of purchased upgrades.
	vector<uint8_t> factories;
	vector<uint8_t> mannedByColonists;
	vector<uint8_t> mannedByRobots;
    vector<uint8_t> upgrades;
	brain_t *brain;
public:
	player_t() {
		colonists = 3;
		colonistLimit = 5;
        robots = 0; // active robots limit is colonistLimit times number of robotics upgrades.
        productionLimit = 10;
		productionSize = 0;
		factories.resize(PRODUCTION_COUNT);
		mannedByColonists.resize(PRODUCTION_COUNT);
		mannedByRobots.resize(PRODUCTION_COUNT);
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
		productionCards.push_back(newCard);
		productionSize += newCard.handSize;
		totalCredits += newCard.value;
	}
    
    void discardCard(decks_t &decks,int which) {
        card_t discard = productionCards[which];
        // erase the card from hand
        productionCards.erase(productionCards.begin() + which);
        productionSize -= discard.handSize;
        totalCredits -= discard.value;
        if (discard.handSize != 4)  // mega cards don't go into same deck
            decks[discard.prodType].discardCard(discard.value);
    }

	void drawProductionCards(decks_t &decks) {
		for (productionEnum_t i=ORE; i<PRODUCTION_COUNT; i++) {
			int toDraw = mannedByColonists[i] + mannedByRobots[i];
            // special case: each scientist upgrade produces a research card without being populated.
            if (i==RESEARCH)
                toDraw += upgrades[SCIENTISTS];
            else if (i==MICROBIOTICS)
                toDraw += upgrades[ORBIAL_LAB];
			while (toDraw >= 4 && decks[i].getMegaValue() && brain->wantMega(i)) {
				card_t megaCard = { decks[i].getMegaValue(), i, 4 };
				addCard(megaCard);
				toDraw -= 4;
			}
            while (toDraw) {
                addCard(decks[i]);
                --toDraw;
            }
		}
	}
    
    void discardExcessProductionCards(decks_t &decks) {
        while (productionSize > productionLimit) {
            discardCard(decks,brain->pickDiscard(productionCards));
        }
    }
    
    void addUpgrade(uint8_t upgrade) {
        upgrades[upgrade]++;
        // this is used for breaking ties on victory points
        totalUpgradeCosts += upgradeCosts[upgrade];
        
        // implement purchase bonuses
        if (upgrade == WAREHOUSE)
            productionLimit += 5;
        else if (upgrade == NODULE)
            colonistLimit += 3;
        else if (upgrade == ROBOTS)
            robots++;
        else if (upgrade == LABORATORY)
            factories[RESEARCH]++;
        else if (upgrade == OUTPOST) {
            colonistLimit += 5;
            productionLimit += 5;
            factories[TITANIUM]++;
        }
    }
    
    unsigned computeScaledVictoryPoints() {
        unsigned vps = 0;
        // compute victory points for static upgrades
        for (upgradeEnum_t i=DATA_LIBRARY; i<UPGRADE_COUNT; i++) {
            static const uint8_t vpsForUpgrade[UPGRADE_COUNT] = { 1,1,1,2,2, 3,3,5,5,5, 0,0,0 };
            vps += vpsForUpgrade[i];
        }
        // now include victory points for factories which are manned
        for (productionEnum_t i=ORE; i<PRODUCTION_COUNT; i++) {
            static const uint8_t vpsForMannedFactory[UPGRADE_COUNT] = { 1,1,2,2,0, 0,3,10,15,20 };
            vps += vpsForMannedFactory[i] * (mannedByColonists[i] + mannedByRobots[i]);
        }
        // leave room in the 4 lsb's to store the player index
        // and room in the next 4 lsb's for some random noise for tiebreaking
        return (vps << 20) + (totalUpgradeCosts<<8);
    }
    
    const string& getName() const { return brain->getName(); }

	void dump() {
		cout << "player " << brain->getName() << ", " << int(colonists) << "/" << int(colonistLimit) << " colonists, " << int(productionSize) << "/" << int(productionLimit) << " cards\n\tfactories: ";
		for (int i=0; i<PRODUCTION_COUNT; i++)
			if (factories[i])
				cout << factoryNames[i] << ":" << int(factories[i]) << "(" << int(mannedByColonists[i]) << 
					"/" << int(mannedByRobots[i]) << ") ";
		cout << "\n\tcards: ";
		for (cardIt_t i=productionCards.begin(); i!=productionCards.end(); i++)
			cout << factoryNames[i->prodType] << "/" << int(i->value) << "$ ";
		cout << "(" << totalCredits << "$ total)\n";
	}
};


class game_t {
	vector<productionDeck_t> productionDecks;
	vector<uint8_t> upgradeDrawPiles;
	vector<uint8_t> upgradeMarket;
	vector<uint8_t> currentMarketCounts;
	vector<player_t> players;
	typedef vector<player_t>::iterator playerIt_t;
	vector<uint8_t> playerOrder;
	typedef vector<uint8_t>::iterator playerOrderIt_t;
	uint8_t era, marketLimit;
	uint16_t highestVp;
	bool previousMarketEmpty;
public:
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

		productionDecks.clear();
		productionDecks.resize(PRODUCTION_COUNT);
		productionDecks[ORE].init(ORE,OreDeck,NELEM(OreDeck),3,0,true);
		productionDecks[WATER].init(WATER,WaterDeck,NELEM(WaterDeck),7,30,true);
		productionDecks[TITANIUM].init(TITANIUM,TitaniumDeck,NELEM(TitaniumDeck),10,44,true);
		productionDecks[RESEARCH].init(RESEARCH,ResearchDeck,NELEM(ResearchDeck),13,0,false);
		productionDecks[MICROBIOTICS].init(MICROBIOTICS,MicrobioticsDeck,NELEM(MicrobioticsDeck),17,0,false);
		productionDecks[NEW_CHEMICALS].init(NEW_CHEMICALS,NewChemicalsDeck,NELEM(NewChemicalsDeck),20,88,true);
		productionDecks[ORBITAL_MEDICINE].init(ORBITAL_MEDICINE,OrbitalMedicineDeck,NELEM(OrbitalMedicineDeck),30,0,true);
		productionDecks[RING_ORE].init(RING_ORE,RingOreDeck,NELEM(RingOreDeck),40,0,true);
		productionDecks[MOON_ORE].init(MOON_ORE,MoonOreDeck,NELEM(MoonOreDeck),50,0,true);
	}

	void setupUpgradeDecks(int playerCount) {
		upgradeDrawPiles.clear();
		upgradeDrawPiles.reserve(PRODUCTION_COUNT);
		if (playerCount == 2) {
			int even = 0, odd = 0, i;
			for (i=0; i<UPGRADE_COUNT; i++) {
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
			static const uint8_t upgrades_1_10[10] = { 0,0,0, 2,3,3,4,5,5,6 };
			static const uint8_t upgrades_11_13[10] = { 0,0,0, 2,3,4,4,5,6,6 };
			for (int i=0; i<10; i++)
				upgradeDrawPiles.push_back(upgrades_1_10[playerCount]);
			for (int i=10; i<UPGRADE_COUNT; i++)
				upgradeDrawPiles.push_back(upgrades_11_13[playerCount]);
		}
	}

	void chooseInitialUpgrades(int playerCount) {
		upgradeMarket.clear();
		currentMarketCounts.clear();
		currentMarketCounts.resize(UPGRADE_COUNT);
        marketLimit = playerCount >> 1;
        replaceUpgradeCards();
	}

	void setInitialPlayerState(int playerCount) {
		players.clear();
		players.resize(playerCount);
		for (playerIt_t i=players.begin(); i!= players.end(); i++) {
            // production is doubled on first turn.
            i->drawProductionCards(productionDecks);
            i->drawProductionCards(productionDecks);
        }
        
		playerOrder.clear();
		playerOrder.resize(playerCount);
		for (int i=0; i<playerOrder.size(); i++)
			playerOrder[i] = i;
		random_shuffle(playerOrder.begin(), playerOrder.end());
	}


	void setupGame(int playerCount) {
		era = 1;
		highestVp = 3;
		previousMarketEmpty = false;
		setupProductionDecks();
		setupUpgradeDecks(playerCount);
		chooseInitialUpgrades(playerCount);
		setInitialPlayerState(playerCount);
	}

	void setPlayerBrain(int index,brain_t &brain) {
		players[index].setBrain(brain);
        players[index].dump();
	}
    
	void determinePlayerOrder() {
		// player order is determined by number of victory points
		// ties are broken by total cost of purchased upgrades (and special factories)
		// beyond that, ties are broken randomly by assigning some random bits to the LSB's
        // and finally beyond that, ties are broken by ordinal value
        vector<unsigned> playerVps;
        for (unsigned i=0; i<players.size(); i++)
            playerVps.push_back(players[i].computeScaledVictoryPoints() | (rand() & 0xF0) | i);
        // sort in ascending order (default uses operator<)
        sort(playerVps.begin(),playerVps.end());
        playerOrder.clear();
        // and reverse the result to determine player order
        while (playerVps.size()) {
            playerOrder.push_back(playerVps.back() & 0xF);
            playerVps.pop_back();
        }
	}
    
    void displayPlayerOrder() {
        for (unsigned i=0; i<playerOrder.size(); i++)
            cout << players[playerOrder[i]].getName() << " is Player " << i+1 << " this round.\n";
    }

	void replaceUpgradeCards() {
		// figure out whether the market is totally empty or not
		bool marketEmpty = upgradeMarket.size() == 0;
		for (int i=0; i<(era==1?4:10) && marketEmpty; i++)
			if (upgradeDrawPiles[i])
				marketEmpty = false;
		static const uint8_t minVpsForEra3[] = { 0,0,40,35,40,30,35,40,30,35 };

		// figure out which era we're in now.
		if (era == 1 && (highestVp >= 10 || (marketEmpty && previousMarketEmpty)))
			era = 2;
		else if (era == 2 && (highestVp >= minVpsForEra3[players.size()] || (marketEmpty && previousMarketEmpty)))
			era = 3;
		previousMarketEmpty = marketEmpty;
        
        while (upgradeMarket.size() < players.size()) {
            // First check if any roll has a chance to succeeed
            int firstMarket = era==3? 1 : 0;
            int marketSize = era==3?12 : era==2? 10 : 4;
            bool anyValid = false;
            // note that we start at zero because even in Era 3 an unpurchased Data Library could still come up for auction.
            for (int i=0; !anyValid && i<firstMarket+marketSize; i++)
                // if there is a card left of this type and we 
                if (upgradeDrawPiles[i] && currentMarketCounts[i] != marketLimit)
                    anyValid = true;
            
            if (!anyValid)
                break;
            
            int roll = firstMarket + (rand() % marketSize);
            for(;;) {
                if (upgradeDrawPiles[roll] && currentMarketCounts[roll] != marketLimit)
                    break;
                else if (roll) // try next upgrade downward
                    --roll;
                else    // pick a new roll if we hit the bottom of the list
                    roll = firstMarket + (rand() % marketSize);
            }
            
            upgradeDrawPiles[roll]--;
            currentMarketCounts[roll]++;
            upgradeMarket.push_back(roll);
        }
	}

	void drawProductionCards() {
		for (playerOrderIt_t i=playerOrder.begin(); i!=playerOrder.end(); i++) {
			players[*i].drawProductionCards(productionDecks);
		}
	}

	void discardExcessProductionCards() {
		for (playerOrderIt_t i=playerOrder.begin(); i!=playerOrder.end(); i++) {
			players[*i].discardExcessProductionCards(productionDecks);
		}
	}

	void performPlayerTurns(bool firstTurn) {
	}

	bool checkVictoryConditions() {
        unsigned winnerIndex = 0;
        unsigned winnerScore = 0;
        for (unsigned i=0; i<players.size(); i++) {
            unsigned vps = players[i].computeScaledVictoryPoints();
            if (vps >= (75 << 20)) {
                if (vps > winnerScore) {
                    winnerIndex = i;
                    winnerScore = vps;
                }
            }
        }
        if (winnerScore == 0)
            return false;   // nobody reached 75 points yet
        cout << "With a winning score of " << (winnerScore >> 20) << " and " << ((winnerScore >> 8) & 0xFFF) << " tiebreaking points:\n";
        // there may be multiple tying players.
        for (unsigned i=0; i<players.size(); i++) {
            if (players[i].computeScaledVictoryPoints() == winnerScore)
                cout << players[i].getName() << " wins!\n";
        }        
        return true;
	}
    
    uint8_t getNumberOfUpgradesInMarket(upgradeEnum_t e) const {
        return currentMarketCounts[e];
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
    int cardAdjustedValue(card_t &card) {
        int value = card.value;
        if (card.prodType == RESEARCH)
            value += 5; // a guess... should also see if we have any chance of affording a New Chem factory, and whether we even want one.
        return value;
    }
    int pickDiscard(vector<card_t> &productionCards) {
        // Just pick the cheapest card we have.
        int guessValue = cardAdjustedValue(productionCards[0]);
        int guessIndex = 0;
        for (int i=1; i<productionCards.size(); i++) {
            int thisValue = cardAdjustedValue(productionCards[i]);
            if (thisValue < guessValue) {
                guessValue = thisValue;
                guessIndex = i;
            }
        }
        return guessIndex;
    }

};

class playerBrain_t: public brain_t {
public:
	playerBrain_t(std::string name) : brain_t(name) { }
    bool wantMega(productionEnum_t t) {
        cout << name << ", do you want a megaproduction card for " << factoryNames[t] << "? ";
        std::string answer;
        getline(cin,answer);
        return toupper(answer[0]) == 'Y';
    }
    int pickDiscard(vector<card_t> &productionCards) {
        cout << name << ", you are over your hand limit.\n";
        for (int i=0; i<productionCards.size(); i++)
            cout << i << ". " << factoryNames[productionCards[i].prodType] << "/" << productionCards[i].value << "$" << endl;
        
        int which = 0;
        do {
            cout << name << ", which card to you want to discard? ";
            std::string answer;
            getline(cin,answer);
            which = atoi(answer.c_str());
        } while (which < 0 || which >= productionCards.size());
        return which;
    }
                    
};


int main() {
	game_t game;
	unsigned playerCount;

    // seed the RNG, but let it be overridden from user input
    unsigned seed = (unsigned) time(NULL);
    
	for (;;) {
		cout << "Number of players?  (2-9) ";
        std::string answer;
        getline(cin,answer);
        playerCount = atoi(answer.c_str());
        if (playerCount < 2 || playerCount > 9)
            seed = playerCount;
        else
            break;
	}

    cout << "(using " << seed << " as RNG seed)" << endl;
    srand(seed);
    
    // set up the play area, deal hands, etc
	game.setupGame(playerCount);

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

    // do the first turn of the game (several phases are skipped, victory is impossible)
    game.displayPlayerOrder();
	game.performPlayerTurns(true);
    
    // now enter the normal turn progression
	do {
		game.determinePlayerOrder();
        game.displayPlayerOrder();
		game.replaceUpgradeCards();
		game.drawProductionCards();
		game.discardExcessProductionCards();
		game.performPlayerTurns(false);
	} while (!game.checkVictoryConditions());
}
