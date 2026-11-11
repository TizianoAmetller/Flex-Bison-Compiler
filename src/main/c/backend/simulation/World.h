#ifndef WORLD_HEADER
#define WORLD_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../semantic-analysis/SemanticModel.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Runtime Runtime;

/** An effect that is acting on a unit, and the turns it has left. */
typedef struct {
	/** Borrowed from the AST. */
	const EffectDeclaration * effect;
	int64_t remaining;
} ActiveEffect;

/**
 * A unit of the battle: one instance of a unit declaration. Its id is its
 * position in the world, which is the order of creation.
 */
typedef struct {
	int id;
	/** "Hero" if its declaration has a single instance in the battle, and "Archer#2" if not. Owned. */
	char * name;
	/** The index of its team in the world. */
	int team;
	/** The unit declaration, borrowed from the AST. */
	const Declaration * declaration;
	int64_t x;
	int64_t y;
	int64_t health;
	int64_t maxHealth;
	int64_t attack;
	int64_t defense;
	int64_t speed;
	bool alive;
	/** In the order they were applied. */
	ActiveEffect * effects;
	size_t effectCount;
	size_t effectCapacity;
} Unit;

typedef struct {
	/** Borrowed from the AST. */
	const char * name;
	/** The units it started with, and the ones that are still alive. */
	size_t total;
	size_t alive;
} Team;

/**
 * The state of a battle. The teams are in the order of the "battle"
 * declaration, and the units in the order they are created: by team, then by
 * member, then by instance.
 */
typedef struct {
	Unit * units;
	size_t unitCount;
	size_t unitCapacity;
	Team * teams;
	size_t teamCount;
	size_t teamCapacity;
	bool hasArena;
	int64_t width;
	int64_t height;
} World;

/** Creates an empty world, or returns NULL if it runs out of memory. */
World * createWorld(void);

/** Destroys a world and everything it owns (not the AST it points to). NULL is fine. */
void destroyWorld(World * world);

/**
 * Creates the arena, the teams and the units of the battle declaration of the
 * model, throwing the dice that they ask for, in a fixed order: for each member
 * its quantity, and for each of its instances the attributes in the order they
 * are written and then, when there is a quantity, the two offsets of its
 * position. On failure it returns false, with the reason in the runtime.
 */
bool buildWorld(Runtime * runtime, const SemanticModel * semanticModel);

/** The unit with that id. The id must be valid. */
Unit * getUnit(const World * world, const int id);

/** Whether the unit with id "other" is of the same team as the one with id "unit", and is another unit. */
bool isAlly(const World * world, const int unit, const int other);

/** Whether the unit with id "other" is of another team than the one with id "unit". */
bool isEnemy(const World * world, const int unit, const int other);

/** The square of the distance between two units, or the largest int64 if it doesn't fit. */
int64_t squaredDistance(const Unit * from, const Unit * to);

/** Brings a position inside the arena, if there is one. */
void clampToArena(const World * world, int64_t * x, int64_t * y);

/** Whether the value is one of the attributes of a unit, and if so, its value. */
bool getUnitAttribute(const Unit * unit, const char * name, int64_t * value);

/** The place of an effect among those that are acting on a unit, or -1 if it is not. */
int findActiveEffect(const Unit * unit, const EffectDeclaration * effect);

/** Adds an effect to the end of those of a unit. Returns false only when it runs out of memory. */
bool addActiveEffect(Unit * unit, const EffectDeclaration * effect, const int64_t remaining);

/** Removes the effect at that place, keeping the order of the others. */
void removeActiveEffect(Unit * unit, const size_t index);

#endif
