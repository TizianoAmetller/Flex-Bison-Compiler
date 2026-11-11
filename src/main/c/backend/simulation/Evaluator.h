#ifndef EVALUATOR_HEADER
#define EVALUATOR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "Rng.h"
#include "World.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** The size of the text that describes the error of a simulation. */
#define RUNTIME_ERROR_SIZE 256

/**
 * What the evaluation of expressions needs, and where it leaves its error. The
 * first error is the one that stays: once the simulation has failed, whatever
 * is being evaluated gives up.
 */
struct Runtime {
	World * world;
	Rng * rng;
	bool failed;
	bool outOfMemory;
	int expressionDepth;
	/** Only meaningful when "failed" is set. */
	char error[RUNTIME_ERROR_SIZE];
};

/** Makes the simulation fail with this error, unless it had already failed. */
void runtimeFail(Runtime * runtime, const char * format, ...) __attribute__((format(printf, 2, 3)));

/** Makes the simulation fail because it ran out of memory. */
void runtimeOutOfMemory(Runtime * runtime);

typedef enum {
	/** The value of what could not be found, such as the nearest enemy when there are none. It spreads to everything that uses it. */
	VALUE_NONE,
	VALUE_INT,
	VALUE_BOOL,
	VALUE_STRING,
	/** A unit, by its id, in "number". */
	VALUE_UNIT,
	/** Some units, by their ids, in "units". The value owns that array. */
	VALUE_UNITS
} ValueKind;

/** The value of an expression. A bool is 0 or 1 in "number". */
typedef struct {
	ValueKind kind;
	int64_t number;
	/** Borrowed from the AST. */
	const char * string;
	int * units;
	size_t count;
} Value;

/**
 * A name that a body can use while it runs, and the unit it stands for: "self",
 * the parameter of an ability or effect, and the iterators of the "for" that are
 * running. Scopes live in the stack and link to the ones around them.
 */
typedef struct RuntimeScope RuntimeScope;

struct RuntimeScope {
	const RuntimeScope * parent;
	const char * name;
	int unit;
};

/** A value of NONE. */
Value noneValue(void);

/** Frees what a value owns. A value must be released once. */
void releaseValue(Value * value);

/** The name of a kind of value, for the errors. */
const char * valueKindName(const ValueKind kind);

/**
 * Evaluates an expression from the point of view of a unit (the "perspective",
 * a unit id, or -1 when there is none), which is what "allies", "enemies" and
 * "nearest" are relative to. When the simulation fails, the value is NONE, and
 * the reason is in the runtime.
 */
Value evaluate(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope);

/** The value of a name: a unit of the scope, or "allies" or "enemies". */
Value resolveName(Runtime * runtime, const char * name, const int perspective, const RuntimeScope * scope, const int line);

#endif
