/**
 * The HTML generator: the page of the replay, with the battle in it.
 *
 * The page is the template (see "ReplayTemplate.h") with its marker replaced by
 * the data of the battle, which is JSON, written between the lines of
 * <script id="battle-data" type="application/json"> and its </script>. The
 * script of the page reads it and plays the battle back: it never runs it. The
 * data are:
 *
 *   {
 *   "seed":"1",                      the seed, as a string (it can be up to 2^64 - 1)
 *   "maxRounds":1000,
 *   "arena":[40,20],                 the size of the arena, or null if it is unbounded
 *   "teams":["Heroes","Goblins"],    in the order of the battle
 *   "units":[                        as the battle starts; the position is the id
 *   [0,"Hero",0,"Hero",0,10,30,30,8,2,6],
 *   ...                              id, name, team, declaration, x, y, health, maxHealth,
 *   ],                               attack, defense, speed
 *   "strings":["Slash","Poison"],    the names of the abilities and effects, the reasons of the
 *                                    skips and the messages of the logs, with no repeats
 *   "events":[                       in order, one per line
 *   [1,1,0,0,1,0,0,0,0,0,0,0],
 *   ...                              type, round, depth, actor, target, amount, health,
 *   ],                               fromX, fromY, x, y, string (an index of "strings", or -1)
 *   "result":{"kind":"winner","team":0,"rounds":2},    or null if the battle did not end
 *   "error":null                     or why the simulation failed
 *   }
 *
 * The type of an event is its place in "EventType" (0 for ROUND_START, to 12
 * for BATTLE_END). What the rest of the fields mean is in "Trace.h", with two
 * differences: a skip has the ability in "string" (or -1) and the index of its
 * reason in "amount", and a log has the index of its message in "string".
 *
 * Nothing the program of the user wrote can get out of its string: the text is
 * escaped so that it can't close the <script>, start a tag or break the JSON.
 * The characters "<", ">" and "&", U+2028 and U+2029 and the control characters
 * are written as \uXXXX, and every byte that is not valid UTF-8 as U+FFFD, so
 * the data are always valid UTF-8 (in fact, ASCII, apart from the valid
 * characters that the logs have).
 */

#include "HtmlGenerator.h"
#include "ReplayTemplate.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* PRIVATE TYPES AND CONSTANTS */

/** Where the data of the battle go in the template. */
#define DATA_MARKER "@@BATTLE_DATA@@"

/** A set of strings, each one with a number in the order they were added. They are borrowed. */
typedef struct {
	const char ** strings;
	size_t count;
	size_t capacity;
	/** A hash table with the numbers of the strings (open addressing), -1 where there is none. */
	int * slots;
	size_t slotCount;
} StringTable;

/* PRIVATE FUNCTIONS */

static size_t _hash(const char * text);
static bool _createTable(StringTable * table, const size_t capacity);
static void _destroyTable(StringTable * table);
static int _findString(const StringTable * table, const char * text);
static bool _addString(StringTable * table, const char * text);
static const char * _eventString(const TraceEvent * event);
static void _writeString(FILE * out, const char * text);
static void _writeEvents(const Trace * trace, const StringTable * table, FILE * out);
static void _writeData(const Trace * trace, const StringTable * table, FILE * out);

/**
 * FNV-1a, which is all a table of names needs.
 */
static size_t _hash(const char * text) {
	uint64_t hash = UINT64_C(14695981039346656037);
	for (const unsigned char * c = (const unsigned char *) text; *c != '\0'; ++c) {
		hash ^= *c;
		hash *= UINT64_C(1099511628211);
	}
	return (size_t) hash;
}

/**
 * Makes room for at most "capacity" strings.
 */
static bool _createTable(StringTable * table, const size_t capacity) {
	size_t slotCount = 16;
	while (slotCount < 2 * capacity) {
		slotCount *= 2;
	}
	table->count = 0;
	table->capacity = capacity;
	table->slotCount = slotCount;
	table->strings = malloc(capacity * sizeof(const char *));
	table->slots = malloc(slotCount * sizeof(int));
	if (table->strings == NULL || table->slots == NULL) {
		_destroyTable(table);
		return false;
	}
	for (size_t k = 0; k < slotCount; ++k) {
		table->slots[k] = -1;
	}
	return true;
}

static void _destroyTable(StringTable * table) {
	free((void *) table->strings);
	free(table->slots);
	table->strings = NULL;
	table->slots = NULL;
}

/**
 * The number of a string in the table, or -1 if it is not there.
 */
static int _findString(const StringTable * table, const char * text) {
	size_t slot = _hash(text) & (table->slotCount - 1);
	while (0 <= table->slots[slot]) {
		if (strcmp(table->strings[table->slots[slot]], text) == 0) {
			return table->slots[slot];
		}
		slot = (slot + 1) & (table->slotCount - 1);
	}
	return -1;
}

/**
 * Adds a string, unless it is already there.
 */
static bool _addString(StringTable * table, const char * text) {
	size_t slot = _hash(text) & (table->slotCount - 1);
	while (0 <= table->slots[slot]) {
		if (strcmp(table->strings[table->slots[slot]], text) == 0) {
			return true;
		}
		slot = (slot + 1) & (table->slotCount - 1);
	}
	if (table->capacity <= table->count) {
		return false;
	}
	table->strings[table->count] = text;
	table->slots[slot] = (int) table->count;
	++table->count;
	return true;
}

/**
 * The string that an event refers to, or NULL if it has none: the name of an
 * ability or effect, the direction of a move or the message of a log.
 */
static const char * _eventString(const TraceEvent * event) {
	return (event->type == EVENT_LOG) ? event->text : event->name;
}

/**
 * Writes a string between quotes, as JSON that is safe to put in a <script>.
 */
static void _writeString(FILE * out, const char * text) {
	static const char hex[] = "0123456789abcdef";
	const unsigned char * bytes = (const unsigned char *) text;
	fputc('"', out);
	for (size_t k = 0; bytes[k] != '\0';) {
		const unsigned char c = bytes[k];
		if (c == '"' || c == '\\') {
			fputc('\\', out);
			fputc(c, out);
			++k;
		}
		else if (c < 0x20 || c == 0x7F || c == '<' || c == '>' || c == '&') {
			fprintf(out, "\\u00%c%c", hex[c >> 4], hex[c & 15]);
			++k;
		}
		else if (c < 0x80) {
			fputc(c, out);
			++k;
		}
		else {
			/** How many bytes the sequence has, if it is valid UTF-8 (no overlong form, no surrogate, nothing past U+10FFFF). */
			size_t length = 0;
			const unsigned char b1 = bytes[k + 1];
			if (0xC2 <= c && c <= 0xDF) {
				length = (0x80 <= b1 && b1 <= 0xBF) ? 2 : 0;
			}
			else if (0xE0 <= c && c <= 0xEF) {
				const unsigned char low = (c == 0xE0) ? 0xA0 : 0x80;
				const unsigned char high = (c == 0xED) ? 0x9F : 0xBF;
				length = (low <= b1 && b1 <= high && 0x80 <= bytes[k + 2] && bytes[k + 2] <= 0xBF) ? 3 : 0;
			}
			else if (0xF0 <= c && c <= 0xF4) {
				const unsigned char low = (c == 0xF0) ? 0x90 : 0x80;
				const unsigned char high = (c == 0xF4) ? 0x8F : 0xBF;
				length = (low <= b1 && b1 <= high && 0x80 <= bytes[k + 2] && bytes[k + 2] <= 0xBF && 0x80 <= bytes[k + 3] && bytes[k + 3] <= 0xBF) ? 4 : 0;
			}
			if (length == 0) {
				fputs("\\ufffd", out);
				++k;
			}
			else if (c == 0xE2 && b1 == 0x80 && (bytes[k + 2] == 0xA8 || bytes[k + 2] == 0xA9)) {
				fputs((bytes[k + 2] == 0xA8) ? "\\u2028" : "\\u2029", out);
				k += length;
			}
			else {
				fwrite(bytes + k, 1, length, out);
				k += length;
			}
		}
	}
	fputc('"', out);
}

/**
 * Writes the events, one per line. The strings go by their number in the table.
 */
static void _writeEvents(const Trace * trace, const StringTable * table, FILE * out) {
	fputs("\"events\":[\n", out);
	for (size_t k = 0; k < trace->eventCount; ++k) {
		const TraceEvent * event = &trace->events[k];
		const char * text = _eventString(event);
		int64_t amount = event->amount;
		if (event->type == EVENT_SKIP) {
			amount = (event->reason != NULL) ? _findString(table, event->reason) : -1;
		}
		fprintf(out, "[%d,%d,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%d]%s\n",
			(int) event->type, event->round, event->depth, event->actor, event->target,
			(long long) amount, (long long) event->health,
			(long long) event->fromX, (long long) event->fromY, (long long) event->x, (long long) event->y,
			(text != NULL) ? _findString(table, text) : -1,
			(k + 1 < trace->eventCount) ? "," : "");
	}
	fputs("],\n", out);
}

/**
 * Writes the data of the battle: a JSON object, which can go in many lines.
 */
static void _writeData(const Trace * trace, const StringTable * table, FILE * out) {
	fputs("{\n", out);
	fprintf(out, "\"seed\":\"%llu\",\n", (unsigned long long) trace->seed);
	fprintf(out, "\"maxRounds\":%d,\n", trace->maxRounds);
	if (trace->hasArena) {
		fprintf(out, "\"arena\":[%lld,%lld],\n", (long long) trace->arenaWidth, (long long) trace->arenaHeight);
	}
	else {
		fputs("\"arena\":null,\n", out);
	}
	fputs("\"teams\":[", out);
	for (size_t k = 0; k < trace->teamCount; ++k) {
		if (0 < k) {
			fputc(',', out);
		}
		_writeString(out, trace->teams[k].name);
	}
	fputs("],\n\"units\":[\n", out);
	for (size_t k = 0; k < trace->unitCount; ++k) {
		const TraceUnit * unit = &trace->units[k];
		fprintf(out, "[%d,", unit->id);
		_writeString(out, unit->name);
		fprintf(out, ",%d,", unit->team);
		_writeString(out, unit->declaration);
		fprintf(out, ",%lld,%lld,%lld,%lld,%lld,%lld,%lld]%s\n",
			(long long) unit->x, (long long) unit->y, (long long) unit->health, (long long) unit->maxHealth,
			(long long) unit->attack, (long long) unit->defense, (long long) unit->speed,
			(k + 1 < trace->unitCount) ? "," : "");
	}
	fputs("],\n\"strings\":[\n", out);
	for (size_t k = 0; k < table->count; ++k) {
		_writeString(out, table->strings[k]);
		fputs((k + 1 < table->count) ? ",\n" : "\n", out);
	}
	fputs("],\n", out);
	_writeEvents(trace, table, out);
	const TraceEvent * last = (0 < trace->eventCount) ? &trace->events[trace->eventCount - 1] : NULL;
	if (last != NULL && last->type == EVENT_BATTLE_END) {
		const char * kind = (last->amount == RESULT_WINNER) ? "winner" : ((last->amount == RESULT_DRAW_NOBODY) ? "draw-nobody" : "draw-limit");
		fprintf(out, "\"result\":{\"kind\":\"%s\",\"team\":%d,\"rounds\":%d},\n", kind, last->team, last->round);
	}
	else {
		fputs("\"result\":null,\n", out);
	}
	fputs("\"error\":", out);
	if (trace->error != NULL) {
		_writeString(out, trace->error);
	}
	else {
		fputs("null", out);
	}
	fputs("\n}", out);
}

/* PUBLIC FUNCTIONS */

bool generateHtml(const Trace * trace, FILE * out) {
	const char * page = (const char *) replayTemplate;
	const char * marker = strstr(page, DATA_MARKER);
	if (marker == NULL) {
		return false;
	}
	StringTable table;
	if (!_createTable(&table, 2 * trace->eventCount + 16)) {
		return false;
	}
	for (size_t k = 0; k < trace->eventCount; ++k) {
		const TraceEvent * event = &trace->events[k];
		const char * text = _eventString(event);
		if ((text != NULL && !_addString(&table, text)) || (event->type == EVENT_SKIP && event->reason != NULL && !_addString(&table, event->reason))) {
			_destroyTable(&table);
			return false;
		}
	}
	fwrite(page, 1, (size_t) (marker - page), out);
	_writeData(trace, &table, out);
	fputs(marker + strlen(DATA_MARKER), out);
	_destroyTable(&table);
	return true;
}
