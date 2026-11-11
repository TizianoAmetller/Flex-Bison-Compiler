#include "Rng.h"

void seedRng(Rng * rng, const uint64_t seed) {
	rng->state = seed;
}

uint64_t nextRandom(Rng * rng) {
	rng->state += UINT64_C(0x9E3779B97F4A7C15);
	uint64_t z = rng->state;
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);
}

int64_t randomBetween(Rng * rng, const int64_t low, const int64_t high) {
	const uint64_t span = (uint64_t) high - (uint64_t) low + 1;
	if (span == 0) {
		return (int64_t) nextRandom(rng);
	}
	/** The numbers below this one are the ones that make some results likelier. */
	const uint64_t threshold = (0 - span) % span;
	uint64_t value = nextRandom(rng);
	while (value < threshold) {
		value = nextRandom(rng);
	}
	return (int64_t) ((uint64_t) low + value % span);
}
