#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

enum productionEnum_t { ORE, WATER, TITANIUM, RESEARCH, MICROBIOTICS, NEW_CHEMICALS, ORBITAL_MEDICINE, RING_ORE, MOON_ORE, PRODUCTION_COUNT };

const char *factoryNames[] = { "Ore", "Water", "Titanium", "Research", "Microbiotics", "NewChemicals", "OrbitalMedicine", "RingOre", "MoonOre" };

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

const char *upgradeNames[] = { "DataLibrary", "Warehouse", "HeavyEquipment", "Nodule", "Scientists", "OrbitalLab", "Robots",
	"Laboratory", "Ecoplants", "Outpost", "SpaceStation", "PlanetaryCruiser", "MoonBase" };

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
	typedef vector<uint8_t>::iterator deckIterator;
	vector<uint8_t> discards;
	typedef vector<uint8_t>::iterator discardsIterator;
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
		for (deckIterator i=deck.begin(); i!=deck.end(); i++) {
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

	virtual bool wantMega(productionEnum_t) = 0;
};

class player_t {
	vector<card_t> productionCards;
	typedef vector<card_t>::iterator cardIterator;
	uint8_t colonists, unusedColonists, colonistLimit, productionSize, productionLimit;
	uint16_t totalCredits;
	vector<uint8_t> factories;
	vector<uint8_t> mannedByColonists;
	vector<uint8_t> mannedByRobots;
	brain_t *brain;
public:
	player_t() {
		colonists = 0;
		unusedColonists = 0;
		colonistLimit = 5;
        productionLimit = 10;
		productionSize = 0;
		factories.resize(PRODUCTION_COUNT);
		mannedByColonists.resize(PRODUCTION_COUNT);
		mannedByRobots.resize(PRODUCTION_COUNT);
		totalCredits = 0;
		brain = 0;
	}

	~player_t() {
		delete brain;
	}

	void setBrain(brain_t &b) {
		if (brain)
			delete brain;
		brain = &b;
	}

	void addColonist() {
		++colonists;
		++unusedColonists;
	}

	void addFactory(productionEnum_t type) {
		factories[type]++;
		// automatically man a factory if unused one is available.
		if (unusedColonists) {
			--unusedColonists;
			mannedByColonists[type]++;
		}
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
			while (toDraw >= 4 && decks[i].getMegaValue() && brain->wantMega(i)) {
				card_t megaCard = { decks[i].getMegaValue(), i, 4 };
				addCard(megaCard);
				toDraw -= 4;
			}
		}
	}
    
    void discardExcessProductionCards(decks_t &decks) {
        while (productionSize > productionLimit) {
            /* int which = brain->pickDiscard();
            discardCard(which); */
        }
    }

	void dump() {
		cout << "player @" << this << ", " << int(colonists) << " colonists, " << int(productionSize) << " hand size\n\tfactories: ";
		for (int i=0; i<PRODUCTION_COUNT; i++)
			if (factories[i])
				cout << factoryNames[i] << ":" << int(factories[i]) << "(" << int(mannedByColonists[i]) << 
					"/" << int(mannedByRobots[i]) << ") ";
		cout << "\n\tcards: ";
		for (cardIterator i=productionCards.begin(); i!=productionCards.end(); i++)
			cout << factoryNames[i->prodType] << "/" << int(i->value) << "$ ";
		cout << "(" << totalCredits << "$ total)\n";
	}
};


class game_t {
	vector<productionDeck_t> productionDecks;
	vector<uint8_t> upgradesRemaining;
	vector<uint8_t> availableUpgrades;
	vector<uint8_t> currentUpgradeCounts;
	vector<player_t> players;
	typedef vector<player_t>::iterator playerIterator;
	vector<uint8_t> playerOrder;
	typedef vector<uint8_t>::iterator playerOrderIterator;
	uint8_t era;
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
		upgradesRemaining.clear();
		upgradesRemaining.reserve(PRODUCTION_COUNT);
		if (playerCount == 2) {
			int even = 0, odd = 0, i;
			for (i=0; i<UPGRADE_COUNT; i++) {
				if (rand() & 1) {
					upgradesRemaining.push_back(1);
					if (++odd == 10)
						break;
				}
				else {
					upgradesRemaining.push_back(2);
					if (++even == 10)
						break;
				}
			}
			// at most 10 upgrade types allowed with one count
			for (; i<UPGRADE_COUNT; i++) {
				upgradesRemaining.push_back((odd == 10)? 2 : 1);
			}
		}
		else {
			static const uint8_t upgrades_1_10[10] = { 0,0,0, 2,3,3,4,5,5,6 };
			static const uint8_t upgrades_11_13[10] = { 0,0,0, 2,3,4,4,5,6,6 };
			for (int i=0; i<10; i++)
				upgradesRemaining.push_back(upgrades_1_10[playerCount]);
			for (int i=10; i<UPGRADE_COUNT; i++)
				upgradesRemaining.push_back(upgrades_11_13[playerCount]);
		}
	}

	void chooseInitialUpgrades(int playerCount) {
		availableUpgrades.clear();
		currentUpgradeCounts.clear();
		currentUpgradeCounts.resize(UPGRADE_COUNT);
		for (int i=0; i<playerCount; i++) {
			// reroll any roll which would yield more upgrade cards of a given type
			// than half the number of players (rounded down)
			int roll;
			do { roll = d4();
			} while (currentUpgradeCounts[roll] == (playerCount >> 1));
			availableUpgrades.push_back(roll);
			upgradesRemaining[roll]--;
			currentUpgradeCounts[roll]++;
		}
	}

	void setInitialPlayerState(int playerCount) {
		players.clear();
		players.resize(playerCount);
		for (playerIterator i=players.begin(); i!= players.end(); i++) {
			for (int j=0; j<3; j++)
				i->addColonist();
			for (int j=0; j<2; j++)
				i->addFactory(ORE);
			for (int j=0; j<1; j++)
				i->addFactory(WATER);
			for (int j=0; j<4; j++)
				i->addCard(productionDecks[ORE]);
			for (int j=0; j<2; j++)
				i->addCard(productionDecks[WATER]);

			i->dump();
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
	}

	void determinePlayerOrder() {
		// player order is determined by number of victory points
		// ties are broken by total cost of purchased upgrades (and special factories)
		// beyond that, ties are broken randomly
	}

	void replaceUpgradeCards() {
		// figure out whether the market is totally empty or not
		bool marketEmpty = availableUpgrades.size() == 0;
		for (int i=0; i<(era==1?4:10) && marketEmpty; i++)
			if (upgradesRemaining[i])
				marketEmpty = false;
		static const uint8_t minVpsForEra3[] = { 0,0,40,35,40,30,35,40,30,35 };

		// figure out which era we're in now.
		if (era == 1 && (highestVp >= 10 || (marketEmpty && previousMarketEmpty)))
			era = 2;
		else if (era == 2 && (highestVp >= minVpsForEra3[players.size()] || (marketEmpty && previousMarketEmpty)))
			era = 3;
		previousMarketEmpty = marketEmpty;
	}

	void distributeProductionCards() {
		for (playerOrderIterator i=playerOrder.begin(); i!=playerOrder.end(); i++) {
			players[*i].drawProductionCards(productionDecks);
		}
	}

	void discardExcessProductionCards() {
		for (playerOrderIterator i=playerOrder.begin(); i!=playerOrder.end(); i++) {
			players[*i].discardExcessProductionCards(productionDecks);
		}
	}

	void performPlayerTurns(bool firstTurn) {
	}

	bool checkVictoryConditions() {
		return highestVp >= 75;
	}

	void test(int playerCount) {
		setupProductionDecks();
		for (vector<productionDeck_t>::iterator i=productionDecks.begin(); i!=productionDecks.end(); i++)
			i->dump();

		setupUpgradeDecks(playerCount);

		cout << "upgrades remaining: ";
		for (vector<uint8_t>::iterator i=upgradesRemaining.begin(); i!= upgradesRemaining.end(); i++)
			cout << int(*i) << " ";
		cout << endl;

		chooseInitialUpgrades(playerCount);

		cout << "new upgrades remaining: ";
		for (vector<uint8_t>::iterator i=upgradesRemaining.begin(); i!= upgradesRemaining.end(); i++)
			cout << int(*i) << " ";
		cout << endl;

		cout << "current upgrades per type: ";
		for (vector<uint8_t>::iterator i=currentUpgradeCounts.begin(); i!= currentUpgradeCounts.end(); i++)
			cout << int(*i) << " ";
		cout << endl;
		cout << "upgrades available: ";
		for (vector<uint8_t>::iterator i=availableUpgrades.begin(); i!= availableUpgrades.end(); i++)
			cout << upgradeNames[*i] << " ";
		cout << endl;

		setInitialPlayerState(playerCount);
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
};


int main() {
	game_t game;
	int playerCount;
	do {
		cout << "Number of players?  (2-9) ";
        std::string answer;
        getline(cin,answer);
        playerCount = atoi(answer.c_str());
	} while (playerCount < 2 || playerCount > 9);

    // set up the play area, deal hands, etc
	game.setupGame(playerCount);

    // attach brains to each player
	for (int i=0; i<playerCount; i++) {
		string name;
		do {
			cout << "Player " << i+1 << " name? ";
			getline(cin,name);
		} while (name.size() == 0);

		char type;
		do {
			cout << "Is player '" << name << "' Human or Computer? (H/C) ";
            std::string answer;
            getline(cin,answer);
			type = toupper(answer[0]);
		} while (type != 'H' && type != 'C');
		game.setPlayerBrain(i,type=='H'? *static_cast<brain_t*>(new playerBrain_t(name)) : *static_cast<brain_t*>(new computerBrain_t(name,game)));
	}

    // do the first turn of the game (several phases are skipped, victory is impossible)
	game.performPlayerTurns(true);
    
    // now enter the normal turn progression
	do {
		game.determinePlayerOrder();
		game.replaceUpgradeCards();
		game.distributeProductionCards();
		game.discardExcessProductionCards();
		game.performPlayerTurns(false);
		break;
	} while (!game.checkVictoryConditions());
	return 0;
}
