#ifndef SIMULATOR_HEADER
#define SIMULATOR_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"

/** Initialize module's internal state. */
ModuleDestructor initializeSimulatorModule(void);

/**
 * Simulates the battle of a program that passed the semantic analysis: it
 * creates the units, runs the rounds until a team wins or the rounds run out,
 * and records everything in the trace that it leaves in "compilerState->trace"
 * (which the caller must destroy, before the AST). The trace is left even when
 * the simulation fails, with what happened until then.
 *
 * It uses the seed and the maximum of rounds of the compiler state, and the
 * caller must make sure there is a battle.
 *
 * @return
 *	SUCCEEDED when the battle ends (a draw is a way to end), SIMULATION_ERROR
 *	when it fails (it reports why through this module's logger), and
 *	OUT_OF_MEMORY when it runs out of memory.
 */
CompilationStatus executeSimulation(CompilerState * compilerState);

#endif
