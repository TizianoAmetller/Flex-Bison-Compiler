# Minimum required version fo CMake. Can be modified at your own risk.
# @see https://cmake.org/download/
cmake_minimum_required(VERSION 3.28.3)

# Name of the project and language to use (C, in this case).
project(Flex-Bison-Compiler C)

# Selects the best strategy according to the available compiler in the system.
# @see https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	message(NOTICE "The C compiler is GCC.")

	# Compiles the parser with Bison, generating counter-examples in case S/R or R/R conflicts exists.
	add_custom_command(
		OUTPUT ../src/main/c/frontend/syntactic-analysis/BisonParser.c ../src/main/c/frontend/syntactic-analysis/BisonParser.h
		COMMAND ../src/main/bash/bison.sh
		DEPENDS ${CMAKE_SOURCE_DIR}/src/main/c/frontend/syntactic-analysis/BisonGrammar.y
	)

	# Options for GCC.
	add_compile_options(-fsanitize=address)
	add_compile_options(-fsanitize=undefined)
	add_compile_options(-O3)
	add_compile_options(-static-libgcc)
	add_compile_options(-std=gnu99)
	add_compile_options(-Wall)
	add_compile_options(-Wextra)
	add_link_options(-fsanitize=address)
	add_link_options(-fsanitize=undefined)

	# Compiles the scanner with Flex.
	add_custom_command(
		OUTPUT ../src/main/c/frontend/lexical-analysis/FlexScanner.c ../src/main/c/frontend/lexical-analysis/FlexScanner.h
		COMMAND ../src/main/bash/flex.sh
		DEPENDS ../src/main/c/frontend/syntactic-analysis/BisonParser.c ../src/main/c/frontend/syntactic-analysis/BisonParser.h ${CMAKE_SOURCE_DIR}/src/main/c/frontend/lexical-analysis/FlexPatterns.l
	)

	# Turns the HTML template of the replay into a C file with its bytes, so that it
	# goes inside the binary. It is made again when the template (or the script) changes.
	set(REPLAY_TEMPLATE_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/ReplayTemplate.c)
	add_custom_command(
		OUTPUT ${REPLAY_TEMPLATE_SOURCE}
		COMMAND ${CMAKE_COMMAND} -DINPUT=${CMAKE_SOURCE_DIR}/src/main/resources/replay-template.html -DOUTPUT=${REPLAY_TEMPLATE_SOURCE} -P ${CMAKE_SOURCE_DIR}/src/main/cmake/EmbedFile.cmake
		DEPENDS ${CMAKE_SOURCE_DIR}/src/main/resources/replay-template.html ${CMAKE_SOURCE_DIR}/src/main/cmake/EmbedFile.cmake
	)

	# Defines the entry-point of the application, and the source-codes (*.c extension).
	# The header files (*.h extension), are automatically included from the source-codes.
	add_executable(Flex-Bison-Compiler
		# The frontend (Flex, Bison and the AST) reads the program, and the
		# backend checks it (semantic-analysis), runs the battle (simulation)
		# and writes it as text or as an HTML replay (code-generation). The
		# arithmetic backend of the template (Calculator.c) was removed along
		# with the AST it worked on.
		src/main/c/EntryPoint.c
		src/main/c/backend/code-generation/Generator.c
		src/main/c/backend/code-generation/HtmlGenerator.c
		src/main/c/backend/semantic-analysis/LogTemplate.c
		src/main/c/backend/semantic-analysis/SemanticAnalyzer.c
		src/main/c/backend/semantic-analysis/SemanticModel.c
		src/main/c/backend/simulation/Evaluator.c
		src/main/c/backend/simulation/Rng.c
		src/main/c/backend/simulation/Simulator.c
		src/main/c/backend/simulation/Trace.c
		src/main/c/backend/simulation/World.c
		src/main/c/frontend/Frontend.c
		src/main/c/frontend/lexical-analysis/FlexActions.c
		src/main/c/frontend/lexical-analysis/FlexScanner.c
		src/main/c/frontend/syntactic-analysis/AbstractSyntaxTree.c
		src/main/c/frontend/syntactic-analysis/BisonActions.c
		src/main/c/frontend/syntactic-analysis/BisonParser.c
		src/main/c/support/configuration/Environment.c
		src/main/c/support/language/String.c
		src/main/c/support/logging/Logger.c
		${REPLAY_TEMPLATE_SOURCE}
		# Add more *.c files if needed (otherwise, they won't be compiled).
		# ...
	)

	# Flex emits a few helpers that this scanner never calls (yyunput, input and
	# yy_top_state), which the warnings above would report on every build.
	set_source_files_properties(
		src/main/c/frontend/lexical-analysis/FlexScanner.c
		PROPERTIES COMPILE_OPTIONS -Wno-unused-function
	)

	# Link final project and libraries.
	target_link_libraries(Flex-Bison-Compiler)
else ()
	message(NOTICE "The C compiler is unknown.")
endif ()
