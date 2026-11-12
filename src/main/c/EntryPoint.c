#include "backend/code-generation/Generator.h"
#include "backend/code-generation/HtmlGenerator.h"
#include "backend/semantic-analysis/SemanticAnalyzer.h"
#include "backend/semantic-analysis/SemanticModel.h"
#include "backend/simulation/SimulationLimits.h"
#include "backend/simulation/Simulator.h"
#include "backend/simulation/Trace.h"
#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/configuration/Environment.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* PRIVATE FUNCTIONS */

static bool _parseUnsigned(const char * text, uint64_t * value);
static bool _parseArguments(const int length, const char ** arguments, CompilerState * compilerState, const Logger * logger);
static CompilationStatus _simulate(CompilerState * compilerState, const Logger * logger);

/**
 * Reads a number made only of digits, from 0 to the largest 64-bit unsigned
 * value. Returns false for anything else, including a sign or a space.
 */
static bool _parseUnsigned(const char * text, uint64_t * value) {
	if (text[0] < '0' || '9' < text[0]) {
		return false;
	}
	char * end = NULL;
	errno = 0;
	const unsigned long long parsed = strtoull(text, &end, 10);
	if (errno != 0 || *end != '\0') {
		return false;
	}
	*value = (uint64_t) parsed;
	return true;
}

/**
 * Parses the command-line arguments (the first one is the program itself) into
 * the compiler state: "--syntax-only", "--check", "--seed N", "--max-rounds N"
 * and "--format text|html". If there is no seed, it is the SEED environment variable (when it is not
 * empty) or else a random one. Returns false, after reporting it, when there is
 * an unknown option or an invalid value.
 */
static bool _parseArguments(const int length, const char ** arguments, CompilerState * compilerState, const Logger * logger) {
	static const char * usage = "Usage: Flex-Bison-Compiler [--syntax-only] [--check] [--seed N] [--max-rounds N] [--format text|html] < <program>";
	bool hasSeed = false;
	compilerState->maxRounds = DEFAULT_MAX_ROUNDS;
	for (int k = 1; k < length; ++k) {
		uint64_t value = 0;
		if (strcmp(arguments[k], "--syntax-only") == 0) {
			compilerState->syntaxOnly = true;
		}
		else if (strcmp(arguments[k], "--check") == 0) {
			compilerState->check = true;
		}
		else if (strcmp(arguments[k], "--seed") == 0) {
			if (length <= k + 1 || !_parseUnsigned(arguments[k + 1], &value)) {
				logError(logger, "Invalid value for --seed: %s", (length <= k + 1) ? "(missing)" : arguments[k + 1]);
				logError(logger, "%s", usage);
				return false;
			}
			compilerState->seed = value;
			hasSeed = true;
			++k;
		}
		else if (strcmp(arguments[k], "--max-rounds") == 0) {
			if (length <= k + 1 || !_parseUnsigned(arguments[k + 1], &value) || value < 1 || MAX_ROUNDS_OPTION < value) {
				logError(logger, "Invalid value for --max-rounds: %s (it must be a number from 1 to %d)", (length <= k + 1) ? "(missing)" : arguments[k + 1], MAX_ROUNDS_OPTION);
				logError(logger, "%s", usage);
				return false;
			}
			compilerState->maxRounds = (int) value;
			++k;
		}
		else if (strcmp(arguments[k], "--format") == 0) {
			if (length <= k + 1 || (strcmp(arguments[k + 1], "text") != 0 && strcmp(arguments[k + 1], "html") != 0)) {
				logError(logger, "Invalid value for --format: %s (it must be text or html)", (length <= k + 1) ? "(missing)" : arguments[k + 1]);
				logError(logger, "%s", usage);
				return false;
			}
			compilerState->format = (strcmp(arguments[k + 1], "html") == 0) ? FORMAT_HTML : FORMAT_TEXT;
			++k;
		}
		else {
			logError(logger, "Unknown option: %s", arguments[k]);
			logError(logger, "%s", usage);
			return false;
		}
	}
	if (!hasSeed) {
		const char * environment = getStringOrDefault("SEED", NULL);
		if (environment != NULL && environment[0] != '\0') {
			if (!_parseUnsigned(environment, &compilerState->seed)) {
				logError(logger, "Invalid value for the SEED environment variable: %s", environment);
				logError(logger, "%s", usage);
				return false;
			}
		}
		else {
			/** The time down to the nanosecond, because the process ids of two containers can be the same. */
			struct timespec now;
			clock_gettime(CLOCK_REALTIME, &now);
			compilerState->seed = ((uint64_t) now.tv_sec * UINT64_C(1000000000) + (uint64_t) now.tv_nsec) ^ (uint64_t) getpid();
		}
	}
	return true;
}

/**
 * Simulates the battle and writes it to the standard output, as text or as an
 * HTML page, even if the simulation fails halfway: what happened until then is
 * shown. If everything went well but the output could not be written (for
 * example, the disk is full), that is an error too: UNKNOWN_ERROR, with its reason.
 */
static CompilationStatus _simulate(CompilerState * compilerState, const Logger * logger) {
	CompilationStatus status = executeSimulation(compilerState);
	errno = 0;
	if (compilerState->trace != NULL) {
		const bool written = (compilerState->format == FORMAT_HTML)
			? generateHtml(compilerState->trace, stdout)
			: generateText(compilerState->trace, stdout);
		if (!written) {
			status = OUT_OF_MEMORY;
		}
	}
	const bool flushed = (fflush(stdout) == 0);
	const int writeError = errno;
	if (status != SUCCEEDED) {
		logError(logger, "The simulation was aborted by an error.");
	}
	else if (!flushed || ferror(stdout)) {
		logError(logger, "The output could not be written: %s", (writeError != 0) ? strerror(writeError) : "unknown error");
		status = UNKNOWN_ERROR;
	}
	return status;
}

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
 *
 * @note
 *	Once the program is accepted (syntactic analysis) and validated (semantic
 *	analysis), the battle it declares is simulated, and what tells it (a text,
 *	or an HTML page that replays it) goes to the standard output. The exit status is 0 if the battle ends, 1 if
 *	the program is rejected, 3 if the output cannot be written, 100 if the
 *	command line is invalid and 101 if the simulation fails.
 */
int main(const int length, const char ** arguments) {
	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	CompilerState compilerState = {
		.abstractSyntaxTree = NULL,
		.syntaxOnly = false,
		.check = false,
		.seed = 0,
		.maxRounds = DEFAULT_MAX_ROUNDS,
		.format = FORMAT_TEXT,
		.semanticModel = NULL,
		.trace = NULL
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
		initializeFrontendModule(lexicalAnalyzer),
		initializeSemanticAnalyzerModule(),
		initializeSimulatorModule()
	};
	CompilationStatus compilationStatus = executeSyntacticAnalysis();
	Program * program = compilerState.abstractSyntaxTree;
	if (compilationStatus == SUCCEEDED) {
		logDebugging(logger, "The syntactic-analysis phase accepts the input program.");
		if (!compilerState.syntaxOnly) {
			compilationStatus = executeSemanticAnalysis(&compilerState);
			if (compilationStatus != SUCCEEDED) {
				logError(logger, "Semantic analysis rejected the input program.");
			}
			else if (!compilerState.check && compilerState.semanticModel->battle != NULL) {
				compilationStatus = _simulate(&compilerState, logger);
			}
		}
	}
	else {
		logError(logger, "The syntactic-analysis phase rejects the input program.");
		compilationStatus = FAILED;
	}
	logDebugging(logger, "Releasing the trace, the semantic model and AST resources...");
	destroyTrace(compilerState.trace);
	destroySemanticModel(compilerState.semanticModel);
	destroyProgram(program);
	for (int k = (sizeof(moduleDestructors)/sizeof(ModuleDestructor)) - 1; 0 <= k; --k) {
		moduleDestructors[k]();
	}
	logDebugging(logger, "Compilation is done.");
	destroyLogger(logger);
	destroyLexicalAnalyzer(lexicalAnalyzer);
	return compilationStatus;
}
