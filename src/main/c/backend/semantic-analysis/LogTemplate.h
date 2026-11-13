#ifndef LOG_TEMPLATE_HEADER
#define LOG_TEMPLATE_HEADER

#include <stddef.h>

/**
 * The template of a "log" message: its raw string split into the literal text
 * and the placeholders that a simulation fills in. A placeholder is "{name}"
 * or "{name.attribute}", where both are written like an identifier of the
 * language. There is no way to write a literal brace: every "{" opens a
 * placeholder and every "}" closes one.
 *
 * The semantic analysis uses it to validate the placeholders, and the
 * simulation uses it to print the messages.
 */

typedef enum {
	LOG_SEGMENT_LITERAL,
	LOG_SEGMENT_PLACEHOLDER
} LogSegmentType;

typedef struct {
	LogSegmentType type;
	/** The text of a literal segment, or the name of a placeholder. */
	char * text;
	/** The attribute of a placeholder like "{name.attribute}", or NULL. Always NULL in a literal. */
	char * attribute;
} LogSegment;

/** What is wrong with a message, if anything. */
typedef enum {
	LOG_TEMPLATE_VALID,
	/** A "{" that no "}" closes (another "{" before it counts as not closing it). */
	LOG_TEMPLATE_UNCLOSED_BRACE,
	/** A "}" with no "{" before it. */
	LOG_TEMPLATE_UNMATCHED_BRACE,
	/** A "{}". */
	LOG_TEMPLATE_EMPTY_PLACEHOLDER,
	/** A "{...}" that is neither "{name}" nor "{name.attribute}". */
	LOG_TEMPLATE_INVALID_PLACEHOLDER
} LogTemplateError;

typedef struct {
	LogSegment * segments;
	size_t count;
	size_t capacity;
	/** The first error found. The segments before it are kept, and there are none after it. */
	LogTemplateError error;
	/** With LOG_TEMPLATE_INVALID_PLACEHOLDER, the offending placeholder, braces included. NULL otherwise. */
	char * invalid;
} LogTemplate;

/**
 * Splits a message into segments. It always returns a template, even if the
 * message has an error (see its "error" field), unless it runs out of memory,
 * in which case it returns NULL. The template owns its strings, so it doesn't
 * depend on the message after this call. Destroy it with "destroyLogTemplate".
 */
LogTemplate * parseLogTemplate(const char * message);

/** Destroys a template and everything it owns. NULL is fine. */
void destroyLogTemplate(LogTemplate * logTemplate);

#endif
