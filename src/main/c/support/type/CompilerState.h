#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER

#include <stdbool.h>
#include <stdint.h>

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
	 * When true, the compilation stops after the semantic analysis, with no
	 * simulation (see the "--check" command-line option).
	 */
	bool check;

	/**
	 * The seed of the random numbers of the simulation, which is always set
	 * ("--seed", else the SEED environment variable, else a random one).
	 */
	uint64_t seed;

	/**
	 * The most rounds that the battle lasts ("--max-rounds").
	 */
	int maxRounds;

	/**
	 * What the semantic analysis learns about the program. It holds pointers
	 * into the AST, so it must be destroyed before it. NULL until then.
	 */
	struct SemanticModel * semanticModel;

	/**
	 * What happened in the simulation. It holds names from the AST, so it must
	 * be destroyed before it. NULL until then.
	 */
	struct Trace * trace;
} CompilerState;

#endif
