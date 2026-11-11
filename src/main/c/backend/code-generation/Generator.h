#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER

#include "../simulation/Trace.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * Writes the trace of a battle as text, one event per line: first how the
 * battle starts (the seed, the arena and the teams), then each round, then how
 * it ends. A trace that stops halfway (a failed simulation) is written as far
 * as it goes. It only reads the trace.
 *
 * The consequences of an action (the damage of an ability, what an effect
 * does) are indented under it. Returns false only when it runs out of memory.
 */
bool generateText(const Trace * trace, FILE * out);

#endif
