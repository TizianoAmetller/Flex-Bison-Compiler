#ifndef RNG_HEADER
#define RNG_HEADER

#include <stdint.h>

/**
 * A pseudo-random number generator: SplitMix64. It is written here (and not
 * taken from the C library) because it has to give the same numbers on every
 * platform, so that a seed reproduces a battle anywhere.
 */
typedef struct {
	uint64_t state;
} Rng;

/** Starts the generator from a seed. The same seed always gives the same numbers. */
void seedRng(Rng * rng, const uint64_t seed);

/** The next number, uniform over all the 64-bit values. */
uint64_t nextRandom(Rng * rng);

/**
 * A uniform number between "low" and "high", both included ("low" must not be
 * greater than "high"). It has no modulo bias: the numbers that would cause it
 * are thrown away.
 */
int64_t randomBetween(Rng * rng, const int64_t low, const int64_t high);

#endif
