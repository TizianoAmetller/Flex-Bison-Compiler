#ifndef SIMULATION_LIMITS_HEADER
#define SIMULATION_LIMITS_HEADER

#include <stdint.h>

/**
 * The limits of a simulation, all in one place. Going over any of them (except
 * the rounds, which end the battle in a draw) is an error of the simulation.
 */

/** The most units that a battle can have, all the teams together. */
#define MAX_UNITS 10000

/** The most dice that a single roll (for example "100000d6") can throw. */
#define MAX_DICE 100000

/** The most events that the trace of a battle can have. */
#define MAX_EVENTS 2000000

/** The rounds that a battle lasts at most, unless "--max-rounds" says otherwise. */
#define DEFAULT_MAX_ROUNDS 1000

/** The most that "--max-rounds" can ask for. */
#define MAX_ROUNDS_OPTION 1000000

/** The iterations that a "while" can do each time it runs. */
#define MAX_WHILE_ITERATIONS 10000

/** The "use" statements that can be inside one another. */
#define MAX_CALL_DEPTH 32

/** How deep an expression can be nested (for example, a very long sum) when it is evaluated. */
#define MAX_EXPRESSION_DEPTH 2000

/** The radius of a "radius" expression is capped at 2^31, so its square fits in 64 bits. */
#define MAX_RADIUS ((int64_t) 2147483648)

#endif
