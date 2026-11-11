#include "LogTemplate.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* PRIVATE FUNCTIONS */

static bool _addSegment(LogTemplate * logTemplate, const LogSegmentType type, const char * text, const size_t length, const char * attribute, const size_t attributeLength);
static char * _duplicate(const char * text, const size_t length);
static bool _isIdentifierStart(const char character);
static bool _isIdentifierPart(const char character);
static size_t _identifierLength(const char * text, const size_t length);

/**
 * Appends a segment, with its own copy of the strings (the attribute is
 * optional: a length of 0 means that there is none). Returns false when it
 * runs out of memory.
 */
static bool _addSegment(LogTemplate * logTemplate, const LogSegmentType type, const char * text, const size_t length, const char * attribute, const size_t attributeLength) {
	if (logTemplate->count == logTemplate->capacity) {
		const size_t capacity = (logTemplate->capacity == 0) ? 8 : (2 * logTemplate->capacity);
		LogSegment * segments = realloc(logTemplate->segments, capacity * sizeof(LogSegment));
		if (segments == NULL) {
			return false;
		}
		logTemplate->segments = segments;
		logTemplate->capacity = capacity;
	}
	LogSegment * segment = &logTemplate->segments[logTemplate->count];
	segment->type = type;
	segment->text = _duplicate(text, length);
	segment->attribute = NULL;
	if (segment->text == NULL) {
		return false;
	}
	if (0 < attributeLength) {
		segment->attribute = _duplicate(attribute, attributeLength);
		if (segment->attribute == NULL) {
			free(segment->text);
			return false;
		}
	}
	++logTemplate->count;
	return true;
}

/**
 * A copy of the first "length" characters of a text, as a string.
 */
static char * _duplicate(const char * text, const size_t length) {
	char * copy = malloc(length + 1);
	if (copy != NULL) {
		memcpy(copy, text, length);
		copy[length] = '\0';
	}
	return copy;
}

/** The characters of an identifier of the language: [A-Za-z_][A-Za-z0-9_]* */
static bool _isIdentifierStart(const char character) {
	return ('A' <= character && character <= 'Z') || ('a' <= character && character <= 'z') || character == '_';
}

static bool _isIdentifierPart(const char character) {
	return _isIdentifierStart(character) || ('0' <= character && character <= '9');
}

/**
 * The length of the identifier that the text starts with, or 0 if it doesn't
 * start with one.
 */
static size_t _identifierLength(const char * text, const size_t length) {
	if (length == 0 || !_isIdentifierStart(text[0])) {
		return 0;
	}
	size_t k = 1;
	while (k < length && _isIdentifierPart(text[k])) {
		++k;
	}
	return k;
}

/* PUBLIC FUNCTIONS */

LogTemplate * parseLogTemplate(const char * message) {
	LogTemplate * logTemplate = calloc(1, sizeof(LogTemplate));
	if (logTemplate == NULL) {
		return NULL;
	}
	const size_t length = strlen(message);
	size_t literalStart = 0;
	size_t k = 0;
	while (k < length) {
		if (message[k] == '}') {
			logTemplate->error = LOG_TEMPLATE_UNMATCHED_BRACE;
			return logTemplate;
		}
		if (message[k] != '{') {
			++k;
			continue;
		}
		size_t end = k + 1;
		while (end < length && message[end] != '}' && message[end] != '{') {
			++end;
		}
		if (end == length || message[end] == '{') {
			logTemplate->error = LOG_TEMPLATE_UNCLOSED_BRACE;
			return logTemplate;
		}
		const char * body = message + k + 1;
		const size_t bodyLength = end - k - 1;
		if (bodyLength == 0) {
			logTemplate->error = LOG_TEMPLATE_EMPTY_PLACEHOLDER;
			return logTemplate;
		}
		const size_t nameLength = _identifierLength(body, bodyLength);
		size_t attributeLength = 0;
		bool valid = (0 < nameLength);
		if (valid && nameLength < bodyLength) {
			valid = (body[nameLength] == '.');
			if (valid) {
				attributeLength = _identifierLength(body + nameLength + 1, bodyLength - nameLength - 1);
				valid = (0 < attributeLength) && (nameLength + 1 + attributeLength == bodyLength);
			}
		}
		if (!valid) {
			logTemplate->error = LOG_TEMPLATE_INVALID_PLACEHOLDER;
			logTemplate->invalid = _duplicate(message + k, end - k + 1);
			if (logTemplate->invalid == NULL) {
				destroyLogTemplate(logTemplate);
				return NULL;
			}
			return logTemplate;
		}
		if (literalStart < k && !_addSegment(logTemplate, LOG_SEGMENT_LITERAL, message + literalStart, k - literalStart, NULL, 0)) {
			destroyLogTemplate(logTemplate);
			return NULL;
		}
		if (!_addSegment(logTemplate, LOG_SEGMENT_PLACEHOLDER, body, nameLength, body + nameLength + 1, attributeLength)) {
			destroyLogTemplate(logTemplate);
			return NULL;
		}
		k = end + 1;
		literalStart = k;
	}
	if (literalStart < length && !_addSegment(logTemplate, LOG_SEGMENT_LITERAL, message + literalStart, length - literalStart, NULL, 0)) {
		destroyLogTemplate(logTemplate);
		return NULL;
	}
	return logTemplate;
}

void destroyLogTemplate(LogTemplate * logTemplate) {
	if (logTemplate != NULL) {
		for (size_t k = 0; k < logTemplate->count; ++k) {
			free(logTemplate->segments[k].text);
			free(logTemplate->segments[k].attribute);
		}
		free(logTemplate->segments);
		free(logTemplate->invalid);
		free(logTemplate);
	}
}
