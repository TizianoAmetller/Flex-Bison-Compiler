#include "Simulator.h"
#include "../semantic-analysis/LogTemplate.h"
#include "Evaluator.h"
#include "Rng.h"
#include "SimulationLimits.h"
#include "Trace.h"
#include "World.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
static void _shutdownSimulatorModule(void) {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Simulator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSimulatorModule(void) {
	_logger = createLogger("Simulator");
	return _shutdownSimulatorModule;
}

/* PRIVATE TYPES AND CONSTANTS */

/** Everything that a simulation is made of while it runs. */
typedef struct {
	Runtime runtime;
	const SemanticModel * semanticModel;
	Trace * trace;
	Rng rng;
	int round;
	int maxRounds;
	/** How many actions the events being recorded are inside of (to show them as consequences). */
	int depth;
	/** How many "use" are inside one another. */
	int callDepth;
} Simulation;

/** What happened when the targets of a statement were looked up. */
typedef enum {
	TARGETS_FOUND,
	TARGETS_SKIPPED,
	TARGETS_FAILED
} TargetsStatus;

/** A text that grows. */
typedef struct {
	char * data;
	size_t length;
	size_t capacity;
} Buffer;

/** Why a statement is skipped. */
#define REASON_NO_UNIT "no unit found"
#define REASON_NOT_IN_ABILITIES "not in its abilities"
#define REASON_INVALID_TARGET_TYPE "invalid target type"
#define REASON_TARGET_DEAD "target is dead"

/** The world that "_compareBySpeed" looks at, because "qsort" can't be told about it. */
static const World * _sortingWorld = NULL;

/* PRIVATE FUNCTIONS */

static TraceEvent * _emit(Simulation * simulation, const EventType type);
static void _emitSkip(Simulation * simulation, const int actor, const char * reason, const char * ability, const int target);
static void _hurt(Simulation * simulation, const int actor, const int targetId, const int64_t amount);
static void _mend(Simulation * simulation, const int actor, const int targetId, const int64_t amount);
static TargetsStatus _findTargets(Simulation * simulation, const Value * value, const int perspective, const int line, const int ** ids, size_t * count, int * single);
static void _executeHurtOrMend(Simulation * simulation, const bool hurt, const Expression * amount, const Expression * target, const int perspective, const RuntimeScope * scope, const int line);
static bool _listsAbility(const Unit * unit, const char * name);
static bool _hasTag(const Unit * unit, const char * tag);
static bool _acceptsTarget(const World * world, const AbilityDeclaration * ability, const int perspective, const int target);
static void _executeUse(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _executeApply(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static int _sign(const int64_t value);
static bool _moveAlongAxis(Runtime * runtime, const int64_t position, const int64_t sign, const int64_t steps, int64_t * result, const int line);
static void _executeMove(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static bool _append(Runtime * runtime, Buffer * buffer, const char * text, const size_t length);
static char * _renderLog(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _executeLog(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _executeFor(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _executeWhile(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _executeIf(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _executeStatement(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope);
static void _execute(Simulation * simulation, const StatementList * statements, const int perspective, const RuntimeScope * scope);
static void _runEffects(Simulation * simulation, const int unitId);
static void _runTurn(Simulation * simulation, const int unitId);
static int _compareBySpeed(const void * left, const void * right);
static bool _isOver(const Simulation * simulation, int * winner);
static void _emitEnd(Simulation * simulation, const BattleResult result, const int winner);
static void _runBattle(Simulation * simulation);
static void _recordStart(Simulation * simulation);

/**
 * Adds an event to the trace, in the current round and depth. It returns NULL,
 * and fails the simulation, if the trace is full or memory runs out. The event
 * is good until the next one is added.
 */
static TraceEvent * _emit(Simulation * simulation, const EventType type) {
	if (simulation->runtime.failed) {
		return NULL;
	}
	if (MAX_EVENTS <= simulation->trace->eventCount) {
		runtimeFail(&simulation->runtime, "the trace has too many events (the limit is %d)", MAX_EVENTS);
		return NULL;
	}
	TraceEvent * event = addTraceEvent(simulation->trace, type, simulation->round, simulation->depth);
	if (event == NULL) {
		runtimeOutOfMemory(&simulation->runtime);
	}
	return event;
}

/**
 * Records that a statement of a unit was skipped. A "use" has the ability and
 * its target; the other statements have neither (ability NULL).
 */
static void _emitSkip(Simulation * simulation, const int actor, const char * reason, const char * ability, const int target) {
	TraceEvent * event = _emit(simulation, EVENT_SKIP);
	if (event != NULL) {
		event->actor = actor;
		event->target = target;
		event->name = ability;
		event->reason = reason;
	}
}

/**
 * Takes health off a unit (never below 0), and kills it if none is left. The
 * amount is what the statement said, even if the unit had less to lose.
 */
static void _hurt(Simulation * simulation, const int actor, const int targetId, const int64_t amount) {
	World * world = simulation->runtime.world;
	Unit * target = getUnit(world, targetId);
	target->health = (amount < target->health) ? (target->health - amount) : 0;
	TraceEvent * event = _emit(simulation, EVENT_DAMAGE);
	if (event != NULL) {
		event->actor = actor;
		event->target = targetId;
		event->amount = amount;
		event->health = target->health;
	}
	if (target->health == 0) {
		target->alive = false;
		--world->teams[target->team].alive;
		event = _emit(simulation, EVENT_DEATH);
		if (event != NULL) {
			event->actor = actor;
			event->target = targetId;
		}
	}
}

/**
 * Gives health to a unit, up to the most it can have. The amount is what the
 * statement said, even if the unit had less room.
 */
static void _mend(Simulation * simulation, const int actor, const int targetId, const int64_t amount) {
	Unit * target = getUnit(simulation->runtime.world, targetId);
	const int64_t room = target->maxHealth - target->health;
	target->health = (room < amount) ? target->maxHealth : (target->health + amount);
	TraceEvent * event = _emit(simulation, EVENT_HEAL);
	if (event != NULL) {
		event->actor = actor;
		event->target = targetId;
		event->amount = amount;
		event->health = target->health;
	}
}

/**
 * The units that a statement works on: the one the value is, or those it
 * lists. Nothing was found for NONE, and a unit that is dead is not a target:
 * in both cases the statement is skipped, with its reason.
 */
static TargetsStatus _findTargets(Simulation * simulation, const Value * value, const int perspective, const int line, const int ** ids, size_t * count, int * single) {
	switch (value->kind) {
		case VALUE_NONE:
			_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
			return TARGETS_SKIPPED;
		case VALUE_UNIT:
			*single = (int) value->number;
			if (!getUnit(simulation->runtime.world, *single)->alive) {
				_emitSkip(simulation, perspective, REASON_TARGET_DEAD, NULL, -1);
				return TARGETS_SKIPPED;
			}
			*ids = single;
			*count = 1;
			return TARGETS_FOUND;
		case VALUE_UNITS:
			*ids = value->units;
			*count = value->count;
			return TARGETS_FOUND;
		default:
			runtimeFail(&simulation->runtime, "line %d: expected a unit or units, got %s", line, valueKindName(value->kind));
			return TARGETS_FAILED;
	}
}

/**
 * "deal" or "heal": the amount is evaluated once for each target, and a
 * negative one is 0. A target that dies on the way is left out.
 */
static void _executeHurtOrMend(Simulation * simulation, const bool hurt, const Expression * amount, const Expression * target, const int perspective, const RuntimeScope * scope, const int line) {
	Value targets = evaluate(&simulation->runtime, target, perspective, scope);
	const int * ids = NULL;
	size_t count = 0;
	int single = -1;
	if (!simulation->runtime.failed && _findTargets(simulation, &targets, perspective, line, &ids, &count, &single) == TARGETS_FOUND) {
		for (size_t k = 0; k < count && !simulation->runtime.failed; ++k) {
			if (!getUnit(simulation->runtime.world, ids[k])->alive) {
				continue;
			}
			Value value = evaluate(&simulation->runtime, amount, perspective, scope);
			if (!simulation->runtime.failed) {
				if (value.kind == VALUE_NONE) {
					_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
					releaseValue(&value);
					break;
				}
				if (value.kind != VALUE_INT) {
					runtimeFail(&simulation->runtime, "line %d: expected an int, got %s", line, valueKindName(value.kind));
				}
				else if (hurt) {
					_hurt(simulation, perspective, ids[k], (0 < value.number) ? value.number : 0);
				}
				else {
					_mend(simulation, perspective, ids[k], (0 < value.number) ? value.number : 0);
				}
			}
			releaseValue(&value);
		}
	}
	releaseValue(&targets);
}

static bool _listsAbility(const Unit * unit, const char * name) {
	for (const IdentifierList * item = unit->declaration->unitDeclaration->abilities; item != NULL; item = item->next) {
		if (strcmp(item->identifier, name) == 0) {
			return true;
		}
	}
	return false;
}

static bool _hasTag(const Unit * unit, const char * tag) {
	for (const IdentifierList * item = unit->declaration->unitDeclaration->tags; item != NULL; item = item->next) {
		if (strcmp(item->identifier, tag) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * Whether an ability can be used by a unit on another: its target types, if
 * it has them, must include one that fits: self (the unit itself), ally (of
 * its team but another unit), enemy (of another team) or a tag of the target.
 * An ability with no target types takes any target.
 */
static bool _acceptsTarget(const World * world, const AbilityDeclaration * ability, const int perspective, const int target) {
	if (ability->targetTypes == NULL) {
		return true;
	}
	for (const IdentifierList * item = ability->targetTypes; item != NULL; item = item->next) {
		const char * type = item->identifier;
		if (strcmp(type, "self") == 0) {
			if (perspective == target) {
				return true;
			}
		}
		else if (strcmp(type, "ally") == 0) {
			if (isAlly(world, perspective, target)) {
				return true;
			}
		}
		else if (strcmp(type, "enemy") == 0) {
			if (isEnemy(world, perspective, target)) {
				return true;
			}
		}
		else if (_hasTag(getUnit(world, target), type)) {
			return true;
		}
	}
	return false;
}

/**
 * "use <ability> on <unit>": skipped (and said so) if the unit doesn't have the
 * ability, if the target doesn't fit its types or if it is dead; else the body
 * of the ability runs, with "self" and the parameter.
 */
static void _executeUse(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	const UseStatement * use = statement->useStatement;
	Value target = evaluate(&simulation->runtime, use->target, perspective, scope);
	if (!simulation->runtime.failed) {
		if (target.kind == VALUE_NONE) {
			_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
		}
		else if (target.kind != VALUE_UNIT) {
			runtimeFail(&simulation->runtime, "line %d: expected a unit, got %s", statement->line, valueKindName(target.kind));
		}
		else {
			const World * world = simulation->runtime.world;
			const int targetId = (int) target.number;
			const Declaration * ability = findAbility(simulation->semanticModel, use->abilityName);
			if (ability == NULL) {
				runtimeFail(&simulation->runtime, "line %d: ability '%s' is not declared", statement->line, use->abilityName);
			}
			else {
				const char * reason = NULL;
				if (!_listsAbility(getUnit(world, perspective), use->abilityName)) {
					reason = REASON_NOT_IN_ABILITIES;
				}
				else if (!_acceptsTarget(world, ability->abilityDeclaration, perspective, targetId)) {
					reason = REASON_INVALID_TARGET_TYPE;
				}
				else if (!getUnit(world, targetId)->alive) {
					reason = REASON_TARGET_DEAD;
				}
				if (reason != NULL) {
					_emitSkip(simulation, perspective, reason, use->abilityName, targetId);
				}
				else if (MAX_CALL_DEPTH <= simulation->callDepth) {
					runtimeFail(&simulation->runtime, "line %d: too many nested uses (the limit is %d)", statement->line, MAX_CALL_DEPTH);
				}
				else {
					TraceEvent * event = _emit(simulation, EVENT_USE);
					if (event != NULL) {
						event->actor = perspective;
						event->target = targetId;
						event->name = use->abilityName;
					}
					const RuntimeScope self = { .parent = NULL, .name = "self", .unit = perspective };
					const RuntimeScope parameter = { .parent = &self, .name = ability->abilityDeclaration->targetParameter, .unit = targetId };
					++simulation->callDepth;
					++simulation->depth;
					_execute(simulation, ability->abilityDeclaration->body, perspective, &parameter);
					--simulation->depth;
					--simulation->callDepth;
				}
			}
		}
	}
	releaseValue(&target);
}

/**
 * "apply <effect> to <unit or units> for <turns>": the effect starts to act on
 * each target that is alive, or, if it was already acting, its turns are set
 * again (they never add up). Nothing happens for 0 turns or less.
 */
static void _executeApply(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	const ApplyStatement * apply = statement->applyStatement;
	Value targets = evaluate(&simulation->runtime, apply->target, perspective, scope);
	Value turns = evaluate(&simulation->runtime, apply->duration, perspective, scope);
	const int * ids = NULL;
	size_t count = 0;
	int single = -1;
	if (!simulation->runtime.failed) {
		if (turns.kind == VALUE_NONE) {
			_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
		}
		else if (turns.kind != VALUE_INT) {
			runtimeFail(&simulation->runtime, "line %d: expected an int, got %s", statement->line, valueKindName(turns.kind));
		}
		else if (_findTargets(simulation, &targets, perspective, statement->line, &ids, &count, &single) == TARGETS_FOUND) {
			const Declaration * effect = findEffect(simulation->semanticModel, apply->effectName);
			if (effect == NULL) {
				runtimeFail(&simulation->runtime, "line %d: effect '%s' is not declared", statement->line, apply->effectName);
			}
			for (size_t k = 0; effect != NULL && 0 < turns.number && k < count && !simulation->runtime.failed; ++k) {
				Unit * unit = getUnit(simulation->runtime.world, ids[k]);
				if (!unit->alive) {
					continue;
				}
				const int index = findActiveEffect(unit, effect->effectDeclaration);
				TraceEvent * event = NULL;
				if (0 <= index) {
					unit->effects[index].remaining = turns.number;
					event = _emit(simulation, EVENT_EFFECT_REFRESHED);
				}
				else if (addActiveEffect(unit, effect->effectDeclaration, turns.number)) {
					event = _emit(simulation, EVENT_EFFECT_APPLIED);
				}
				else {
					runtimeOutOfMemory(&simulation->runtime);
				}
				if (event != NULL) {
					event->actor = perspective;
					event->target = ids[k];
					event->name = apply->effectName;
					event->amount = turns.number;
				}
			}
		}
	}
	releaseValue(&targets);
	releaseValue(&turns);
}

static int _sign(const int64_t value) {
	return (0 < value) - (value < 0);
}

/**
 * Moves a coordinate "steps" cells in the direction of the sign. It fails the
 * simulation if the result doesn't fit in 64 bits.
 */
static bool _moveAlongAxis(Runtime * runtime, const int64_t position, const int64_t sign, const int64_t steps, int64_t * result, const int line) {
	int64_t distance = 0;
	if (__builtin_mul_overflow(sign, steps, &distance) || __builtin_add_overflow(position, distance, result)) {
		runtimeFail(runtime, "line %d: a position overflows", line);
		return false;
	}
	return true;
}

/**
 * "move toward <unit>": the unit goes as many cells as its speed along each axis
 * (one step at a time, but in a closed formula), without going past the cell
 * next to the target. "move away from <unit>" goes as many cells as its speed
 * the other way, and from the same cell as the target it goes toward +x. The
 * unit stays inside the arena.
 */
static void _executeMove(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	const MoveStatement * move = statement->moveStatement;
	Value target = evaluate(&simulation->runtime, move->target, perspective, scope);
	if (!simulation->runtime.failed) {
		const World * world = simulation->runtime.world;
		if (target.kind == VALUE_NONE) {
			_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
		}
		else if (target.kind != VALUE_UNIT) {
			runtimeFail(&simulation->runtime, "line %d: expected a unit, got %s", statement->line, valueKindName(target.kind));
		}
		else if (!getUnit(world, (int) target.number)->alive) {
			_emitSkip(simulation, perspective, REASON_TARGET_DEAD, NULL, -1);
		}
		else {
			Unit * unit = getUnit(world, perspective);
			const Unit * destination = getUnit(world, (int) target.number);
			const int64_t fromX = unit->x;
			const int64_t fromY = unit->y;
			int64_t dx = 0;
			int64_t dy = 0;
			int64_t x = fromX;
			int64_t y = fromY;
			bool moved = !(__builtin_sub_overflow(destination->x, fromX, &dx) || __builtin_sub_overflow(destination->y, fromY, &dy) || dx == INT64_MIN || dy == INT64_MIN);
			if (!moved) {
				runtimeFail(&simulation->runtime, "line %d: a position overflows", statement->line);
			}
			else if (move->direction == TOWARD_MOVE) {
				const int64_t distanceX = (dx < 0) ? -dx : dx;
				const int64_t distanceY = (dy < 0) ? -dy : dy;
				const int64_t farthest = (distanceX < distanceY) ? distanceY : distanceX;
				int64_t steps = (farthest <= 1) ? 0 : (farthest - 1);
				if (unit->speed < steps) {
					steps = unit->speed;
				}
				moved = _moveAlongAxis(&simulation->runtime, fromX, _sign(dx), (steps < distanceX) ? steps : distanceX, &x, statement->line)
					&& _moveAlongAxis(&simulation->runtime, fromY, _sign(dy), (steps < distanceY) ? steps : distanceY, &y, statement->line);
			}
			else {
				const int signX = (dx == 0 && dy == 0) ? -1 : _sign(dx);
				moved = _moveAlongAxis(&simulation->runtime, fromX, -signX, unit->speed, &x, statement->line)
					&& _moveAlongAxis(&simulation->runtime, fromY, -_sign(dy), unit->speed, &y, statement->line);
			}
			if (moved) {
				clampToArena(world, &x, &y);
				unit->x = x;
				unit->y = y;
				TraceEvent * event = _emit(simulation, EVENT_MOVE);
				if (event != NULL) {
					event->actor = perspective;
					event->target = (int) target.number;
					event->name = (move->direction == TOWARD_MOVE) ? "toward" : "away from";
					event->fromX = fromX;
					event->fromY = fromY;
					event->x = x;
					event->y = y;
				}
			}
		}
	}
	releaseValue(&target);
}

/**
 * Adds text to the end of a buffer, which always has room for a final '\0'.
 */
static bool _append(Runtime * runtime, Buffer * buffer, const char * text, const size_t length) {
	if (buffer->capacity < buffer->length + length + 1) {
		size_t capacity = (buffer->capacity == 0) ? 64 : buffer->capacity;
		while (capacity < buffer->length + length + 1) {
			capacity *= 2;
		}
		char * data = realloc(buffer->data, capacity);
		if (data == NULL) {
			runtimeOutOfMemory(runtime);
			return false;
		}
		buffer->data = data;
		buffer->capacity = capacity;
	}
	memcpy(buffer->data + buffer->length, text, length);
	buffer->length += length;
	buffer->data[buffer->length] = '\0';
	return true;
}

/**
 * Fills in the template of a log: a unit is written by its name, an int in
 * decimal, a bool as true or false and a string as it is. The text is owned by
 * the caller. It returns NULL, with the simulation failed, if anything goes wrong.
 */
static char * _renderLog(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	Runtime * runtime = &simulation->runtime;
	LogTemplate * logTemplate = parseLogTemplate(statement->logStatement->message);
	if (logTemplate == NULL) {
		runtimeOutOfMemory(runtime);
		return NULL;
	}
	if (logTemplate->error != LOG_TEMPLATE_VALID) {
		runtimeFail(runtime, "line %d: the message of the log is not valid", statement->line);
	}
	Buffer buffer = { .data = NULL, .length = 0, .capacity = 0 };
	for (size_t k = 0; k < logTemplate->count && !runtime->failed; ++k) {
		const LogSegment * segment = &logTemplate->segments[k];
		if (segment->type == LOG_SEGMENT_LITERAL) {
			_append(runtime, &buffer, segment->text, strlen(segment->text));
			continue;
		}
		Value value = resolveName(runtime, segment->text, perspective, scope, statement->line);
		char number[32];
		const char * text = NULL;
		if (runtime->failed) {
			text = NULL;
		}
		else if (segment->attribute != NULL) {
			int64_t attribute = 0;
			if (value.kind == VALUE_UNIT && getUnitAttribute(getUnit(runtime->world, (int) value.number), segment->attribute, &attribute)) {
				snprintf(number, sizeof(number), "%lld", (long long) attribute);
				text = number;
			}
			else {
				runtimeFail(runtime, "line %d: '%s.%s' can't be printed", statement->line, segment->text, segment->attribute);
			}
		}
		else if (value.kind == VALUE_UNIT) {
			text = getUnit(runtime->world, (int) value.number)->name;
		}
		else if (value.kind == VALUE_INT) {
			snprintf(number, sizeof(number), "%lld", (long long) value.number);
			text = number;
		}
		else if (value.kind == VALUE_BOOL) {
			text = (value.number != 0) ? "true" : "false";
		}
		else if (value.kind == VALUE_STRING) {
			text = value.string;
		}
		else {
			runtimeFail(runtime, "line %d: '%s' can't be printed", statement->line, segment->text);
		}
		if (text != NULL) {
			_append(runtime, &buffer, text, strlen(text));
		}
		releaseValue(&value);
	}
	destroyLogTemplate(logTemplate);
	if (!runtime->failed && buffer.data == NULL) {
		_append(runtime, &buffer, "", 0);
	}
	if (runtime->failed) {
		free(buffer.data);
		return NULL;
	}
	return buffer.data;
}

/**
 * "log": the message, with its placeholders filled in, as an event of the unit.
 */
static void _executeLog(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	char * message = _renderLog(simulation, statement, perspective, scope);
	if (message == NULL) {
		return;
	}
	TraceEvent * event = _emit(simulation, EVENT_LOG);
	if (event == NULL) {
		free(message);
		return;
	}
	event->actor = perspective;
	event->text = message;
}

/**
 * "for": it goes over the units of a copy of the collection, leaving out those
 * that die on the way, with the iterator as a unit in a scope of its own.
 */
static void _executeFor(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	const ForStatement * loop = statement->forStatement;
	Value collection = evaluate(&simulation->runtime, loop->collection, perspective, scope);
	if (!simulation->runtime.failed) {
		if (collection.kind == VALUE_NONE) {
			_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
		}
		else if (collection.kind != VALUE_UNITS) {
			runtimeFail(&simulation->runtime, "line %d: expected units, got %s", statement->line, valueKindName(collection.kind));
		}
		else {
			for (size_t k = 0; k < collection.count && !simulation->runtime.failed; ++k) {
				if (getUnit(simulation->runtime.world, collection.units[k])->alive) {
					const RuntimeScope iterator = { .parent = scope, .name = loop->iterator, .unit = collection.units[k] };
					_execute(simulation, loop->body, perspective, &iterator);
				}
			}
		}
	}
	releaseValue(&collection);
}

/**
 * "while": it repeats while the condition holds, up to a limit of iterations.
 */
static void _executeWhile(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	const WhileStatement * loop = statement->whileStatement;
	int iterations = 0;
	while (!simulation->runtime.failed) {
		Value condition = evaluate(&simulation->runtime, loop->condition, perspective, scope);
		bool again = false;
		if (!simulation->runtime.failed) {
			if (condition.kind == VALUE_NONE) {
				_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
			}
			else if (condition.kind != VALUE_BOOL) {
				runtimeFail(&simulation->runtime, "line %d: expected a bool, got %s", statement->line, valueKindName(condition.kind));
			}
			else if (condition.number != 0) {
				if (MAX_WHILE_ITERATIONS <= iterations) {
					runtimeFail(&simulation->runtime, "line %d: a while loop ran more than %d iterations", statement->line, MAX_WHILE_ITERATIONS);
				}
				else {
					again = true;
				}
			}
		}
		releaseValue(&condition);
		if (!again) {
			return;
		}
		++iterations;
		_execute(simulation, loop->body, perspective, scope);
	}
}

/**
 * "if": the branch that the condition chooses. If there is no value to decide
 * with (NONE), neither runs.
 */
static void _executeIf(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	const IfStatement * conditional = statement->ifStatement;
	Value condition = evaluate(&simulation->runtime, conditional->condition, perspective, scope);
	if (!simulation->runtime.failed) {
		if (condition.kind == VALUE_NONE) {
			_emitSkip(simulation, perspective, REASON_NO_UNIT, NULL, -1);
		}
		else if (condition.kind != VALUE_BOOL) {
			runtimeFail(&simulation->runtime, "line %d: expected a bool, got %s", statement->line, valueKindName(condition.kind));
		}
		else {
			_execute(simulation, (condition.number != 0) ? conditional->thenBranch : conditional->elseBranch, perspective, scope);
		}
	}
	releaseValue(&condition);
}

static void _executeStatement(Simulation * simulation, const Statement * statement, const int perspective, const RuntimeScope * scope) {
	switch (statement->type) {
		case APPLY_STATEMENT:
			_executeApply(simulation, statement, perspective, scope);
			break;
		case DEAL_STATEMENT:
			_executeHurtOrMend(simulation, true, statement->dealStatement->amount, statement->dealStatement->target, perspective, scope, statement->line);
			break;
		case FOR_STATEMENT:
			_executeFor(simulation, statement, perspective, scope);
			break;
		case HEAL_STATEMENT:
			_executeHurtOrMend(simulation, false, statement->healStatement->amount, statement->healStatement->target, perspective, scope, statement->line);
			break;
		case IF_STATEMENT:
			_executeIf(simulation, statement, perspective, scope);
			break;
		case LOG_STATEMENT:
			_executeLog(simulation, statement, perspective, scope);
			break;
		case MOVE_STATEMENT:
			_executeMove(simulation, statement, perspective, scope);
			break;
		case USE_STATEMENT:
			_executeUse(simulation, statement, perspective, scope);
			break;
		case WHILE_STATEMENT:
			_executeWhile(simulation, statement, perspective, scope);
			break;
	}
}

/**
 * Runs the statements of a body one after the other, each one seeing what the
 * previous one left. The perspective is the unit that "allies", "enemies" and
 * "nearest" are relative to.
 */
static void _execute(Simulation * simulation, const StatementList * statements, const int perspective, const RuntimeScope * scope) {
	for (const StatementList * item = statements; item != NULL && !simulation->runtime.failed; item = item->next) {
		_executeStatement(simulation, item->statement, perspective, scope);
	}
}

/**
 * The effects that are acting on a unit, in the order they were applied: the
 * body of each one runs with its parameter as the unit, and then its turns go
 * down by one, and it wears off at 0. Those applied while this goes on wait for
 * the next turn. If the unit dies, nothing else of this runs.
 */
static void _runEffects(Simulation * simulation, const int unitId) {
	World * world = simulation->runtime.world;
	Unit * unit = getUnit(world, unitId);
	const size_t count = unit->effectCount;
	if (count == 0) {
		return;
	}
	const EffectDeclaration ** effects = malloc(count * sizeof(const EffectDeclaration *));
	if (effects == NULL) {
		runtimeOutOfMemory(&simulation->runtime);
		return;
	}
	for (size_t k = 0; k < count; ++k) {
		effects[k] = unit->effects[k].effect;
	}
	for (size_t k = 0; k < count && !simulation->runtime.failed; ++k) {
		if (findActiveEffect(unit, effects[k]) < 0) {
			continue;
		}
		TraceEvent * event = _emit(simulation, EVENT_EFFECT_TICK);
		if (event != NULL) {
			event->actor = unitId;
			event->target = unitId;
			event->name = effects[k]->name;
		}
		const RuntimeScope parameter = { .parent = NULL, .name = effects[k]->targetParameter, .unit = unitId };
		++simulation->depth;
		_execute(simulation, effects[k]->body, unitId, &parameter);
		--simulation->depth;
		if (simulation->runtime.failed || !unit->alive) {
			break;
		}
		const int index = findActiveEffect(unit, effects[k]);
		if (0 <= index) {
			--unit->effects[index].remaining;
			if (unit->effects[index].remaining <= 0) {
				removeActiveEffect(unit, (size_t) index);
				event = _emit(simulation, EVENT_EFFECT_END);
				if (event != NULL) {
					event->actor = unitId;
					event->target = unitId;
					event->name = effects[k]->name;
				}
			}
		}
	}
	free(effects);
}

/**
 * The "on turn" of a unit, if its declaration has one, with "self" as the unit.
 */
static void _runTurn(Simulation * simulation, const int unitId) {
	const Unit * unit = getUnit(simulation->runtime.world, unitId);
	const Declaration * turn = findTurn(simulation->semanticModel, unit->declaration->unitDeclaration->name);
	if (turn != NULL) {
		const RuntimeScope self = { .parent = NULL, .name = "self", .unit = unitId };
		_execute(simulation, turn->turnDeclaration->body, unitId, &self);
	}
}

/**
 * The order of the turns: the fastest first, and of those equally fast, the
 * first one created.
 */
static int _compareBySpeed(const void * left, const void * right) {
	const Unit * first = getUnit(_sortingWorld, *(const int *) left);
	const Unit * second = getUnit(_sortingWorld, *(const int *) right);
	if (first->speed != second->speed) {
		return (second->speed < first->speed) ? -1 : 1;
	}
	return (first->id < second->id) ? -1 : ((second->id < first->id) ? 1 : 0);
}

/**
 * Whether the battle is over: at most one team has units alive. Then the winner
 * is that team, or -1 if no one is left.
 */
static bool _isOver(const Simulation * simulation, int * winner) {
	const World * world = simulation->runtime.world;
	int alive = 0;
	*winner = -1;
	for (size_t k = 0; k < world->teamCount; ++k) {
		if (0 < world->teams[k].alive) {
			++alive;
			*winner = (int) k;
		}
	}
	if (1 < alive) {
		*winner = -1;
	}
	return alive <= 1;
}

static void _emitEnd(Simulation * simulation, const BattleResult result, const int winner) {
	TraceEvent * event = _emit(simulation, EVENT_BATTLE_END);
	if (event != NULL) {
		event->amount = result;
		event->team = winner;
	}
}

/**
 * Plays the rounds. In each, the units alive go in order of speed; each one that
 * is still alive on its turn first has its effects act and then, if it
 * survives, runs its "on turn". After every turn, the battle ends if at most
 * one team has units alive. If the rounds run out, it is a draw.
 */
static void _runBattle(Simulation * simulation) {
	const World * world = simulation->runtime.world;
	int * order = malloc(world->unitCount * sizeof(int));
	if (order == NULL) {
		runtimeOutOfMemory(&simulation->runtime);
		return;
	}
	_sortingWorld = world;
	for (simulation->round = 1; simulation->round <= simulation->maxRounds && !simulation->runtime.failed; ++simulation->round) {
		TraceEvent * event = _emit(simulation, EVENT_ROUND_START);
		(void) event;
		size_t count = 0;
		for (size_t k = 0; k < world->unitCount; ++k) {
			if (world->units[k].alive) {
				order[count] = (int) k;
				++count;
			}
		}
		qsort(order, count, sizeof(int), _compareBySpeed);
		for (size_t k = 0; k < count && !simulation->runtime.failed; ++k) {
			if (!getUnit(world, order[k])->alive) {
				continue;
			}
			_runEffects(simulation, order[k]);
			if (!simulation->runtime.failed && getUnit(world, order[k])->alive) {
				_runTurn(simulation, order[k]);
			}
			int winner = -1;
			if (!simulation->runtime.failed && _isOver(simulation, &winner)) {
				_emitEnd(simulation, (0 <= winner) ? RESULT_WINNER : RESULT_DRAW_NOBODY, winner);
				free(order);
				return;
			}
		}
	}
	free(order);
	if (!simulation->runtime.failed) {
		simulation->round = simulation->maxRounds;
		_emitEnd(simulation, RESULT_DRAW_LIMIT, -1);
	}
}

/**
 * Records in the trace how the battle starts: the arena, the teams and the
 * units. If the world could not be built there are no names for its units, and
 * only the arena is recorded.
 */
static void _recordStart(Simulation * simulation) {
	const World * world = simulation->runtime.world;
	Trace * trace = simulation->trace;
	trace->hasArena = world->hasArena;
	trace->arenaWidth = world->width;
	trace->arenaHeight = world->height;
	if (simulation->runtime.failed) {
		return;
	}
	for (size_t k = 0; k < world->teamCount; ++k) {
		if (!addTraceTeam(trace, world->teams[k].name)) {
			runtimeOutOfMemory(&simulation->runtime);
			return;
		}
	}
	for (size_t k = 0; k < world->unitCount; ++k) {
		const Unit * unit = &world->units[k];
		TraceUnit start = {
			.id = unit->id,
			.name = unit->name,
			.team = unit->team,
			.declaration = unit->declaration->unitDeclaration->name,
			.x = unit->x,
			.y = unit->y,
			.health = unit->health,
			.maxHealth = unit->maxHealth,
			.attack = unit->attack,
			.defense = unit->defense,
			.speed = unit->speed
		};
		if (!addTraceUnit(trace, &start)) {
			runtimeOutOfMemory(&simulation->runtime);
			return;
		}
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSimulation(CompilerState * compilerState) {
	Simulation simulation;
	memset(&simulation, 0, sizeof(Simulation));
	simulation.semanticModel = compilerState->semanticModel;
	simulation.maxRounds = compilerState->maxRounds;
	seedRng(&simulation.rng, compilerState->seed);
	World * world = createWorld();
	Trace * trace = createTrace();
	compilerState->trace = trace;
	if (world == NULL || trace == NULL) {
		logError(_logger, "simulation error: out of memory");
		destroyWorld(world);
		return OUT_OF_MEMORY;
	}
	simulation.trace = trace;
	simulation.runtime.world = world;
	simulation.runtime.rng = &simulation.rng;
	trace->seed = compilerState->seed;
	trace->maxRounds = compilerState->maxRounds;
	logDebugging(_logger, "Simulating with seed %llu...", (unsigned long long) compilerState->seed);
	buildWorld(&simulation.runtime, simulation.semanticModel);
	_recordStart(&simulation);
	if (!simulation.runtime.failed) {
		_runBattle(&simulation);
	}
	CompilationStatus status = SUCCEEDED;
	if (simulation.runtime.failed) {
		logError(_logger, "simulation error: %s", simulation.runtime.error);
		status = simulation.runtime.outOfMemory ? OUT_OF_MEMORY : SIMULATION_ERROR;
	}
	destroyWorld(world);
	return status;
}
