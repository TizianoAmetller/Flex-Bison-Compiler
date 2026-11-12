#ifndef TRACE_HEADER
#define TRACE_HEADER

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * The trace of a battle: how it started, and everything that happened in it,
 * as structured events. The simulation writes it and the generators read it
 * (the text one now, the HTML one later), so nothing else is needed to show or
 * replay a battle. The events carry the RESULTING values (the health after a
 * hit, the position after a move), so a replay doesn't have to recompute them.
 *
 * The names of the teams, abilities and effects are borrowed from the AST, so
 * the trace must be destroyed before it. The names of the units and the
 * messages of the logs belong to the trace.
 */

typedef enum {
	EVENT_ROUND_START,
	EVENT_USE,
	EVENT_DAMAGE,
	EVENT_HEAL,
	EVENT_DEATH,
	EVENT_EFFECT_APPLIED,
	EVENT_EFFECT_REFRESHED,
	EVENT_EFFECT_TICK,
	EVENT_EFFECT_END,
	EVENT_MOVE,
	EVENT_LOG,
	EVENT_SKIP,
	EVENT_BATTLE_END
} EventType;

/** How a battle ends, in "amount" of the EVENT_BATTLE_END event. */
typedef enum {
	RESULT_WINNER,
	RESULT_DRAW_NOBODY,
	RESULT_DRAW_LIMIT
} BattleResult;

/** A unit as the battle starts. */
typedef struct {
	int id;
	char * name;
	int team;
	/** The name of its declaration, borrowed. */
	const char * declaration;
	int64_t x;
	int64_t y;
	int64_t health;
	int64_t maxHealth;
	int64_t attack;
	int64_t defense;
	int64_t speed;
} TraceUnit;

typedef struct {
	/** Borrowed. */
	const char * name;
} TraceTeam;

/**
 * Something that happened. Which fields mean something depends on the type
 * (the rest are -1 or 0 or NULL):
 *
 *   ROUND_START                the round
 *   USE                        actor uses the ability "name" on target
 *   DAMAGE, HEAL               target takes or gets "amount"; "health" is its health after
 *   DEATH                      target dies
 *   EFFECT_APPLIED, _REFRESHED the effect "name" is on target for "amount" turns
 *   EFFECT_TICK                the effect "name" acts on target; "amount" is its turns left after the tick
 *   EFFECT_END                 the effect "name" wears off from target
 *   MOVE                       actor goes from (fromX, fromY) to (x, y), "toward" or "away from" (name) target;
 *                              it always changes the position (a move that would leave it as it is is no event)
 *   LOG                        actor says "text"
 *   SKIP                       actor skips a statement, because of "reason"; with a "name" it was a "use" on target
 *   BATTLE_END                 the result is in "amount" (see BattleResult), the winner is "team"
 *
 * The "depth" says how many actions the event is inside of (an ability used
 * by a unit, an effect that acts), to show it as a consequence of them.
 */
typedef struct {
	EventType type;
	int round;
	int depth;
	int actor;
	int target;
	int team;
	int64_t amount;
	int64_t health;
	int64_t fromX;
	int64_t fromY;
	int64_t x;
	int64_t y;
	/** Borrowed. */
	const char * name;
	/** A fixed string. */
	const char * reason;
	/** Owned by the trace. */
	char * text;
} TraceEvent;

typedef struct Trace Trace;

struct Trace {
	uint64_t seed;
	int maxRounds;
	bool hasArena;
	int64_t arenaWidth;
	int64_t arenaHeight;
	TraceTeam * teams;
	size_t teamCount;
	size_t teamCapacity;
	TraceUnit * units;
	size_t unitCount;
	size_t unitCapacity;
	TraceEvent * events;
	size_t eventCount;
	size_t eventCapacity;
	/** Why the simulation failed, or NULL if it didn't. Owned by the trace. */
	char * error;
};

/** Creates an empty trace, or returns NULL if it runs out of memory. */
Trace * createTrace(void);

/** Destroys a trace and everything it owns. NULL is fine. */
void destroyTrace(Trace * trace);

/** Each of these returns false only when it runs out of memory. */
bool addTraceTeam(Trace * trace, const char * name);
bool addTraceUnit(Trace * trace, const TraceUnit * unit);

/**
 * Appends an event of a type, in a round and at a depth, with the rest of its
 * fields cleared (the units and the team at -1). Returns it so the caller can
 * fill it, but only until the next event is added. It returns NULL when it runs
 * out of memory.
 */
TraceEvent * addTraceEvent(Trace * trace, const EventType type, const int round, const int depth);

#endif
