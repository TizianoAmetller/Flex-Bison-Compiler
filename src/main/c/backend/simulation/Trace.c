#include "Trace.h"
#include <stdlib.h>
#include <string.h>

/* PRIVATE FUNCTIONS */

static bool _grow(void ** items, size_t * capacity, const size_t count, const size_t size);

/**
 * Makes room for one more item in an array that holds "count" of them,
 * doubling its capacity when it is full. Returns false when it runs out of
 * memory, and leaves the array as it was.
 */
static bool _grow(void ** items, size_t * capacity, const size_t count, const size_t size) {
	if (count < *capacity) {
		return true;
	}
	const size_t newCapacity = (*capacity == 0) ? 16 : (2 * *capacity);
	void * grown = realloc(*items, newCapacity * size);
	if (grown == NULL) {
		return false;
	}
	*items = grown;
	*capacity = newCapacity;
	return true;
}

/* PUBLIC FUNCTIONS */

Trace * createTrace(void) {
	return calloc(1, sizeof(Trace));
}

void destroyTrace(Trace * trace) {
	if (trace != NULL) {
		for (size_t k = 0; k < trace->unitCount; ++k) {
			free(trace->units[k].name);
		}
		for (size_t k = 0; k < trace->eventCount; ++k) {
			free(trace->events[k].text);
		}
		free(trace->teams);
		free(trace->units);
		free(trace->events);
		free(trace->error);
		free(trace);
	}
}

bool addTraceTeam(Trace * trace, const char * name) {
	if (!_grow((void **) &trace->teams, &trace->teamCapacity, trace->teamCount, sizeof(TraceTeam))) {
		return false;
	}
	trace->teams[trace->teamCount].name = name;
	++trace->teamCount;
	return true;
}

bool addTraceUnit(Trace * trace, const TraceUnit * unit) {
	if (!_grow((void **) &trace->units, &trace->unitCapacity, trace->unitCount, sizeof(TraceUnit))) {
		return false;
	}
	const size_t length = strlen(unit->name);
	char * name = malloc(length + 1);
	if (name == NULL) {
		return false;
	}
	memcpy(name, unit->name, length + 1);
	trace->units[trace->unitCount] = *unit;
	trace->units[trace->unitCount].name = name;
	++trace->unitCount;
	return true;
}

TraceEvent * addTraceEvent(Trace * trace, const EventType type, const int round, const int depth) {
	if (!_grow((void **) &trace->events, &trace->eventCapacity, trace->eventCount, sizeof(TraceEvent))) {
		return NULL;
	}
	TraceEvent * event = &trace->events[trace->eventCount];
	memset(event, 0, sizeof(TraceEvent));
	event->type = type;
	event->round = round;
	event->depth = depth;
	event->actor = -1;
	event->target = -1;
	event->team = -1;
	++trace->eventCount;
	return event;
}
