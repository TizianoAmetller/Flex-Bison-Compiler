#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "SemanticModel.h"

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalyzerModule(void);

/**
 * Validates the program in the AST: its declarations, the references between
 * them, the battle and the arena, the attributes of the units, the constant
 * expressions (attributes, positions, quantities and the arena), and the bodies
 * of the abilities, effects and "on turn" declarations (the names they use, the
 * types of their expressions and statements, and the messages of their logs).
 *
 * It leaves the model it builds in "compilerState->semanticModel", which the
 * caller must destroy (before the AST, whose nodes the model points to), and
 * it reports every error it finds, up to a limit, through this module's logger.
 *
 * @return
 *	SUCCEEDED when there are no errors, FAILED when there is at least one, and
 *	OUT_OF_MEMORY when it runs out of memory.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

#endif
