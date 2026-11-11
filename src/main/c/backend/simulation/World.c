#include "World.h"
#include "Evaluator.h"
#include "SimulationLimits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PRIVATE TYPES */

/** How many instances a unit declaration has in the battle, and how many of them already have a name. */
typedef struct {
	const Declaration * declaration;
	size_t total;
	size_t named;
} NameCounter;

/* PRIVATE FUNCTIONS */

static bool _evaluateInt(Runtime * runtime, const Expression * expression, int64_t * value);
static int _addTeam(World * world, const char * name);
static Unit * _addUnit(World * world, const int team, const Declaration * declaration);
static int64_t _ceilSqrt(const int64_t value);
static bool _rollAttributes(Runtime * runtime, const UnitDeclaration * declaration, Unit * unit);
static bool _instantiateMember(Runtime * runtime, const SemanticModel * semanticModel, const int team, const Member * member);
static NameCounter * _findCounter(NameCounter * counters, const size_t count, const Declaration * declaration);
static bool _nameUnits(Runtime * runtime);

/**
 * Evaluates an expression that has to give an int (an attribute, a quantity, a
 * coordinate), with no unit to look at. Returns false if the simulation fails.
 */
static bool _evaluateInt(Runtime * runtime, const Expression * expression, int64_t * value) {
	Value result = evaluate(runtime, expression, -1, NULL);
	if (!runtime->failed && result.kind != VALUE_INT) {
		runtimeFail(runtime, "line %d: expected an int, got %s", expression->line, valueKindName(result.kind));
	}
	const bool succeeded = !runtime->failed;
	if (succeeded) {
		*value = result.number;
	}
	releaseValue(&result);
	return succeeded;
}

/**
 * Adds a team to the world, and returns its index, or -1 when it runs out of
 * memory.
 */
static int _addTeam(World * world, const char * name) {
	if (world->teamCount == world->teamCapacity) {
		const size_t capacity = (world->teamCapacity == 0) ? 4 : (2 * world->teamCapacity);
		Team * teams = realloc(world->teams, capacity * sizeof(Team));
		if (teams == NULL) {
			return -1;
		}
		world->teams = teams;
		world->teamCapacity = capacity;
	}
	Team * team = &world->teams[world->teamCount];
	team->name = name;
	team->total = 0;
	team->alive = 0;
	++world->teamCount;
	return (int) (world->teamCount - 1);
}

/**
 * Adds a unit to the end of the world, alive and with nothing else set, and
 * returns it (until the next one is added), or NULL when it runs out of memory.
 */
static Unit * _addUnit(World * world, const int team, const Declaration * declaration) {
	if (world->unitCount == world->unitCapacity) {
		const size_t capacity = (world->unitCapacity == 0) ? 16 : (2 * world->unitCapacity);
		Unit * units = realloc(world->units, capacity * sizeof(Unit));
		if (units == NULL) {
			return NULL;
		}
		world->units = units;
		world->unitCapacity = capacity;
	}
	Unit * unit = &world->units[world->unitCount];
	memset(unit, 0, sizeof(Unit));
	unit->id = (int) world->unitCount;
	unit->team = team;
	unit->declaration = declaration;
	unit->alive = true;
	++world->unitCount;
	++world->teams[team].total;
	++world->teams[team].alive;
	return unit;
}

/**
 * The smallest number whose square is not less than the value.
 */
static int64_t _ceilSqrt(const int64_t value) {
	int64_t root = 0;
	while (root * root < value) {
		++root;
	}
	return root;
}

/**
 * Throws the attributes of an instance, in the order they are written in its
 * declaration. None is less than 0, and the health is at least 1: that is the
 * most it can have.
 */
static bool _rollAttributes(Runtime * runtime, const UnitDeclaration * declaration, Unit * unit) {
	for (const AttributeList * item = declaration->attributes; item != NULL; item = item->next) {
		int64_t value = 0;
		if (!_evaluateInt(runtime, item->attribute->value, &value)) {
			return false;
		}
		if (value < 0) {
			value = 0;
		}
		const char * name = item->attribute->name;
		if (strcmp(name, "health") == 0) {
			unit->health = (value < 1) ? 1 : value;
		}
		else if (strcmp(name, "attack") == 0) {
			unit->attack = value;
		}
		else if (strcmp(name, "defense") == 0) {
			unit->defense = value;
		}
		else if (strcmp(name, "speed") == 0) {
			unit->speed = value;
		}
	}
	unit->maxHealth = unit->health;
	return true;
}

/**
 * Creates the instances of a member of a team. The quantity is evaluated once
 * (no quantity is one instance, and less than 1 is none). The position is the
 * member's, or else the unit's, or else the center of the arena (or the origin
 * without one): exact for a single instance, and the center of a cloud for
 * several, whose instances are scattered with an offset on each axis of at
 * most the square root of their number.
 */
static bool _instantiateMember(Runtime * runtime, const SemanticModel * semanticModel, const int team, const Member * member) {
	World * world = runtime->world;
	const Declaration * declaration = findUnit(semanticModel, member->unitName);
	if (declaration == NULL) {
		runtimeFail(runtime, "line %d: unit '%s' is not declared", member->line, member->unitName);
		return false;
	}
	const UnitDeclaration * unit = declaration->unitDeclaration;
	const bool grouped = (member->quantity != NULL);
	int64_t quantity = 1;
	if (grouped && !_evaluateInt(runtime, member->quantity, &quantity)) {
		return false;
	}
	if (quantity <= 0) {
		return true;
	}
	if (quantity > (int64_t) (MAX_UNITS - world->unitCount)) {
		runtimeFail(runtime, "line %d: too many units: a battle can have at most %d", member->line, MAX_UNITS);
		return false;
	}
	int64_t centerX = 0;
	int64_t centerY = 0;
	const Position * position = (member->position != NULL) ? member->position : unit->position;
	if (position != NULL) {
		if (!_evaluateInt(runtime, position->x, &centerX) || !_evaluateInt(runtime, position->y, &centerY)) {
			return false;
		}
	}
	else if (world->hasArena) {
		centerX = world->width / 2;
		centerY = world->height / 2;
	}
	const int64_t radius = grouped ? _ceilSqrt(quantity) : 0;
	for (int64_t k = 0; k < quantity; ++k) {
		Unit * instance = _addUnit(world, team, declaration);
		if (instance == NULL) {
			runtimeOutOfMemory(runtime);
			return false;
		}
		if (!_rollAttributes(runtime, unit, instance)) {
			return false;
		}
		int64_t x = centerX;
		int64_t y = centerY;
		if (grouped) {
			const int64_t offsetX = randomBetween(runtime->rng, -radius, radius);
			const int64_t offsetY = randomBetween(runtime->rng, -radius, radius);
			if (__builtin_add_overflow(centerX, offsetX, &x) || __builtin_add_overflow(centerY, offsetY, &y)) {
				runtimeFail(runtime, "line %d: the position of '%s' overflows", member->line, member->unitName);
				return false;
			}
		}
		clampToArena(world, &x, &y);
		instance->x = x;
		instance->y = y;
	}
	return true;
}

static NameCounter * _findCounter(NameCounter * counters, const size_t count, const Declaration * declaration) {
	for (size_t k = 0; k < count; ++k) {
		if (counters[k].declaration == declaration) {
			return &counters[k];
		}
	}
	return NULL;
}

/**
 * Gives every unit its name: the one of its declaration if that has a single
 * instance in the whole battle, and else that name followed by "#" and the
 * order of the instance (from 1) among those of its declaration.
 */
static bool _nameUnits(Runtime * runtime) {
	World * world = runtime->world;
	NameCounter * counters = NULL;
	size_t counterCount = 0;
	bool succeeded = true;
	for (size_t k = 0; k < world->unitCount && succeeded; ++k) {
		NameCounter * counter = _findCounter(counters, counterCount, world->units[k].declaration);
		if (counter == NULL) {
			NameCounter * grown = realloc(counters, (counterCount + 1) * sizeof(NameCounter));
			if (grown == NULL) {
				succeeded = false;
				break;
			}
			counters = grown;
			counter = &counters[counterCount];
			counter->declaration = world->units[k].declaration;
			counter->total = 0;
			counter->named = 0;
			++counterCount;
		}
		++counter->total;
	}
	for (size_t k = 0; k < world->unitCount && succeeded; ++k) {
		Unit * unit = &world->units[k];
		NameCounter * counter = _findCounter(counters, counterCount, unit->declaration);
		const char * base = unit->declaration->unitDeclaration->name;
		const size_t size = strlen(base) + 24;
		unit->name = malloc(size);
		if (unit->name == NULL) {
			succeeded = false;
			break;
		}
		++counter->named;
		if (counter->total == 1) {
			snprintf(unit->name, size, "%s", base);
		}
		else {
			snprintf(unit->name, size, "%s#%zu", base, counter->named);
		}
	}
	free(counters);
	if (!succeeded) {
		runtimeOutOfMemory(runtime);
	}
	return succeeded;
}

/* PUBLIC FUNCTIONS */

World * createWorld(void) {
	return calloc(1, sizeof(World));
}

void destroyWorld(World * world) {
	if (world != NULL) {
		for (size_t k = 0; k < world->unitCount; ++k) {
			free(world->units[k].name);
			free(world->units[k].effects);
		}
		free(world->units);
		free(world->teams);
		free(world);
	}
}

bool buildWorld(Runtime * runtime, const SemanticModel * semanticModel) {
	World * world = runtime->world;
	if (semanticModel->arena != NULL) {
		const ArenaDeclaration * arena = semanticModel->arena->arenaDeclaration;
		int64_t width = 0;
		int64_t height = 0;
		if (!_evaluateInt(runtime, arena->width, &width) || !_evaluateInt(runtime, arena->height, &height)) {
			return false;
		}
		if (width < 1 || height < 1) {
			runtimeFail(runtime, "line %d: the arena must be at least 1x1", semanticModel->arena->line);
			return false;
		}
		world->hasArena = true;
		world->width = width;
		world->height = height;
	}
	const BattleDeclaration * battle = semanticModel->battle->battleDeclaration;
	for (const IdentifierList * item = battle->teams; item != NULL; item = item->next) {
		const Declaration * team = findTeam(semanticModel, item->identifier);
		if (team == NULL) {
			runtimeFail(runtime, "line %d: team '%s' is not declared", item->line, item->identifier);
			return false;
		}
		const int index = _addTeam(world, team->teamDeclaration->name);
		if (index < 0) {
			runtimeOutOfMemory(runtime);
			return false;
		}
		for (const MemberList * member = team->teamDeclaration->members; member != NULL; member = member->next) {
			if (!_instantiateMember(runtime, semanticModel, index, member->member)) {
				return false;
			}
		}
		if (world->teams[index].total == 0) {
			runtimeFail(runtime, "line %d: team '%s' has no units at the start", item->line, item->identifier);
			return false;
		}
	}
	return _nameUnits(runtime);
}

Unit * getUnit(const World * world, const int id) {
	return &world->units[id];
}

bool isAlly(const World * world, const int unit, const int other) {
	return unit != other && world->units[unit].team == world->units[other].team;
}

bool isEnemy(const World * world, const int unit, const int other) {
	return world->units[unit].team != world->units[other].team;
}

int64_t squaredDistance(const Unit * from, const Unit * to) {
	int64_t dx = 0;
	int64_t dy = 0;
	int64_t squareX = 0;
	int64_t squareY = 0;
	int64_t sum = 0;
	if (__builtin_sub_overflow(to->x, from->x, &dx) || __builtin_sub_overflow(to->y, from->y, &dy)
		|| __builtin_mul_overflow(dx, dx, &squareX) || __builtin_mul_overflow(dy, dy, &squareY)
		|| __builtin_add_overflow(squareX, squareY, &sum)) {
		return INT64_MAX;
	}
	return sum;
}

void clampToArena(const World * world, int64_t * x, int64_t * y) {
	if (world->hasArena) {
		if (*x < 0) {
			*x = 0;
		}
		else if (world->width <= *x) {
			*x = world->width - 1;
		}
		if (*y < 0) {
			*y = 0;
		}
		else if (world->height <= *y) {
			*y = world->height - 1;
		}
	}
}

bool getUnitAttribute(const Unit * unit, const char * name, int64_t * value) {
	if (strcmp(name, "health") == 0) {
		*value = unit->health;
	}
	else if (strcmp(name, "attack") == 0) {
		*value = unit->attack;
	}
	else if (strcmp(name, "defense") == 0) {
		*value = unit->defense;
	}
	else if (strcmp(name, "speed") == 0) {
		*value = unit->speed;
	}
	else {
		return false;
	}
	return true;
}

int findActiveEffect(const Unit * unit, const EffectDeclaration * effect) {
	for (size_t k = 0; k < unit->effectCount; ++k) {
		if (unit->effects[k].effect == effect) {
			return (int) k;
		}
	}
	return -1;
}

bool addActiveEffect(Unit * unit, const EffectDeclaration * effect, const int64_t remaining) {
	if (unit->effectCount == unit->effectCapacity) {
		const size_t capacity = (unit->effectCapacity == 0) ? 4 : (2 * unit->effectCapacity);
		ActiveEffect * effects = realloc(unit->effects, capacity * sizeof(ActiveEffect));
		if (effects == NULL) {
			return false;
		}
		unit->effects = effects;
		unit->effectCapacity = capacity;
	}
	unit->effects[unit->effectCount].effect = effect;
	unit->effects[unit->effectCount].remaining = remaining;
	++unit->effectCount;
	return true;
}

void removeActiveEffect(Unit * unit, const size_t index) {
	for (size_t k = index + 1; k < unit->effectCount; ++k) {
		unit->effects[k - 1] = unit->effects[k];
	}
	--unit->effectCount;
}
