#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <string.h>

/* PRIVATE FUNCTIONS */

static bool _parseArguments(const int length, const char ** arguments, CompilerState * compilerState, const Logger * logger);

/**
 * Parses the command-line arguments (the first one is the program itself) into
 * the compiler state. The only supported option is "--syntax-only". Returns
 * false, after reporting it, when there is an unknown option.
 */
static bool _parseArguments(const int length, const char ** arguments, CompilerState * compilerState, const Logger * logger) {
	for (int k = 1; k < length; ++k) {
		if (strcmp(arguments[k], "--syntax-only") == 0) {
			compilerState->syntaxOnly = true;
		}
		else {
			logError(logger, "Unknown option: %s", arguments[k]);
			logError(logger, "Usage: Flex-Bison-Compiler [--syntax-only] < <program>");
			return false;
		}
	}
	return true;
}

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
 *
 * @note
 *	Stage II only covers the frontend (lexical + syntactic analysis): the
 *	semantic-analysis and backend (code-generation) phases are out of scope
 *	for this delivery per the QRF's own feedback, and are deferred to Stage
 *	III, which will add the RPG combat simulator's own backend from scratch
 *	(the original template's "Calculator"/"Generator" backend, belonging to
 *	a different, arithmetic-only domain, has been removed).
 */
int main(const int length, const char ** arguments) {
	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	CompilerState compilerState = {
		.abstractSyntaxTree = NULL,
		.syntaxOnly = false
	};
	if (!_parseArguments(length, arguments, &compilerState, logger)) {
		destroyLogger(logger);
		destroyLexicalAnalyzer(lexicalAnalyzer);
		return INVALID_USAGE;
	}
	ModuleDestructor moduleDestructors[] = {
		initializeAbstractSyntaxTreeModule(),
		initializeFlexActionsModule(lexicalAnalyzer),
		initializeBisonActionsModule(&compilerState),
		initializeFrontendModule(lexicalAnalyzer)
	};
	CompilationStatus compilationStatus = executeSyntacticAnalysis();
	Program * program = compilerState.abstractSyntaxTree;
	if (compilationStatus == SUCCEEDED) {
		logDebugging(logger, "The syntactic-analysis phase accepts the input program.");
		// The semantic-analysis phase goes here, and it is skipped when compilerState.syntaxOnly is set.
	}
	else {
		logError(logger, "The syntactic-analysis phase rejects the input program.");
		compilationStatus = FAILED;
	}
	logDebugging(logger, "Releasing AST resources...");
	destroyProgram(program);
	for (int k = (sizeof(moduleDestructors)/sizeof(ModuleDestructor)) - 1; 0 <= k; --k) {
		moduleDestructors[k]();
	}
	logDebugging(logger, "Compilation is done.");
	destroyLogger(logger);
	destroyLexicalAnalyzer(lexicalAnalyzer);
	return compilationStatus;
}
