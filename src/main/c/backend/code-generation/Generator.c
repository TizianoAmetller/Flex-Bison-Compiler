#include "Generator.h"
#include <stdlib.h>

/* PRIVATE FUNCTIONS */

static const char * _plural(const int64_t count, const char * singular, const char * plural);
static const char * _unitName(const Trace * trace, const int id);
static void _printEvent(const Trace * trace, const TraceEvent * event, const size_t * totals, const size_t * deaths, FILE * out);

static const char * _plural(const int64_t count, const char * singular, const char * plural) {
	return (count == 1) ? singular : plural;
}

static const char * _unitName(const Trace * trace, const int id) {
	return (0 <= id && (size_t) id < trace->unitCount) ? trace->units[id].name : "?";
}

/**
 * Writes one event as a line, indented by the actions it is inside of.
 */
static void _printEvent(const Trace * trace, const TraceEvent * event, const size_t * totals, const size_t * deaths, FILE * out) {
	const char * actor = _unitName(trace, event->actor);
	const char * target = _unitName(trace, event->target);
	if (event->type == EVENT_ROUND_START) {
		fprintf(out, "\nRound %d\n", event->round);
		return;
	}
	if (event->type == EVENT_BATTLE_END) {
		fprintf(out, "\nBattle over after %d %s.\n", event->round, _plural(event->round, "round", "rounds"));
		if (event->amount == RESULT_WINNER) {
			fprintf(out, "Winner: %s\n", trace->teams[event->team].name);
		}
		else if (event->amount == RESULT_DRAW_NOBODY) {
			fprintf(out, "Draw: nobody is left alive\n");
		}
		else {
			fprintf(out, "Draw: round limit (%d) reached\n", trace->maxRounds);
		}
		for (size_t k = 0; k < trace->teamCount; ++k) {
			fprintf(out, "Team %s: %zu alive, %zu dead\n", trace->teams[k].name, totals[k] - deaths[k], deaths[k]);
		}
		return;
	}
	fprintf(out, "%*s", 2 * event->depth, "");
	switch (event->type) {
		case EVENT_USE:
			fprintf(out, "%s uses %s on %s\n", actor, event->name, target);
			break;
		case EVENT_DAMAGE:
			fprintf(out, "%s takes %lld damage (health %lld/%lld)\n", target, (long long) event->amount, (long long) event->health, (long long) trace->units[event->target].maxHealth);
			break;
		case EVENT_HEAL:
			fprintf(out, "%s heals %lld (health %lld/%lld)\n", target, (long long) event->amount, (long long) event->health, (long long) trace->units[event->target].maxHealth);
			break;
		case EVENT_DEATH:
			fprintf(out, "%s dies\n", target);
			break;
		case EVENT_EFFECT_APPLIED:
			fprintf(out, "%s is affected by %s for %lld %s\n", target, event->name, (long long) event->amount, _plural(event->amount, "turn", "turns"));
			break;
		case EVENT_EFFECT_REFRESHED:
			fprintf(out, "%s on %s is refreshed to %lld %s\n", event->name, target, (long long) event->amount, _plural(event->amount, "turn", "turns"));
			break;
		case EVENT_EFFECT_TICK:
			fprintf(out, "%s acts on %s\n", event->name, target);
			break;
		case EVENT_EFFECT_END:
			fprintf(out, "%s wears off from %s\n", event->name, target);
			break;
		case EVENT_MOVE:
			fprintf(out, "%s moves %s %s: (%lld, %lld) -> (%lld, %lld)\n", actor, event->name, target, (long long) event->fromX, (long long) event->fromY, (long long) event->x, (long long) event->y);
			break;
		case EVENT_LOG:
			fprintf(out, "> %s\n", event->text);
			break;
		case EVENT_SKIP:
			if (event->name != NULL) {
				fprintf(out, "%s cannot use %s on %s: %s\n", actor, event->name, target, event->reason);
			}
			else {
				fprintf(out, "%s skips a statement: %s\n", actor, event->reason);
			}
			break;
		case EVENT_ROUND_START:
		case EVENT_BATTLE_END:
			break;
	}
}

/* PUBLIC FUNCTIONS */

bool generateText(const Trace * trace, FILE * out) {
	const size_t teamCount = (0 < trace->teamCount) ? trace->teamCount : 1;
	size_t * totals = calloc(teamCount, sizeof(size_t));
	size_t * deaths = calloc(teamCount, sizeof(size_t));
	if (totals == NULL || deaths == NULL) {
		free(totals);
		free(deaths);
		return false;
	}
	for (size_t k = 0; k < trace->unitCount; ++k) {
		++totals[trace->units[k].team];
	}
	for (size_t k = 0; k < trace->eventCount; ++k) {
		if (trace->events[k].type == EVENT_DEATH) {
			++deaths[trace->units[trace->events[k].target].team];
		}
	}
	fprintf(out, "Seed: %llu\n", (unsigned long long) trace->seed);
	if (trace->hasArena) {
		fprintf(out, "Arena: %lldx%lld\n", (long long) trace->arenaWidth, (long long) trace->arenaHeight);
	}
	else {
		fprintf(out, "Arena: unbounded\n");
	}
	for (size_t k = 0; k < trace->teamCount; ++k) {
		fprintf(out, "Team %s: %zu %s\n", trace->teams[k].name, totals[k], _plural((int64_t) totals[k], "unit", "units"));
	}
	for (size_t k = 0; k < trace->eventCount; ++k) {
		_printEvent(trace, &trace->events[k], totals, deaths, out);
	}
	free(totals);
	free(deaths);
	return true;
}
