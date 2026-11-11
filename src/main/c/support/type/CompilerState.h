#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER

#include <stdbool.h>

/**
 * The global state of the compiler. Should transport every data structure
 * needed across the different phases of a compilation.
 */
typedef struct {
	/**
	 * The root node of the AST.
	 */
	struct Program * abstractSyntaxTree;

	/**
	 * When true, the compilation stops after the syntactic analysis (see the
	 * "--syntax-only" command-line option).
	 */
	bool syntaxOnly;

	// Stage III adds the semantic model here.
} CompilerState;

#endif
