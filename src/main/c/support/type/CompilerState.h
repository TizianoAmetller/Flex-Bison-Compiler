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

	/**
	 * What the semantic analysis learns about the program. It holds pointers
	 * into the AST, so it must be destroyed before it. NULL until then.
	 */
	struct SemanticModel * semanticModel;
} CompilerState;

#endif
