#! /bin/bash

set -u

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'

# By default, AddressSanitizer exits with status 1, which is also what a
# rejection looks like: these statuses keep a crash from passing as a rejection.
export ASAN_OPTIONS="exitcode=77"
export LSAN_OPTIONS="exitcode=78"

# The semantic tests count the lines of the standard error, so its content can't
# depend on the environment: at WARNING it holds the errors and the warnings of
# the compiler, and nothing below them.
export LOGGING_LEVEL="WARNING"

# The simulation tests use the seed in their command line, never this one.
unset SEED

# UndefinedBehaviorSanitizer reports as "<file>:<line>:<column>: runtime error:",
# and the anchor leaves out the errors of the compiled program that are worded alike.
SANITIZER_PATTERN='AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|[0-9]+:[0-9]+: runtime error:'

STDOUT_FILE="$(mktemp)"
STDERR_FILE="$(mktemp)"
PLAIN_FILE="$(mktemp)"
trap 'rm --force "$STDOUT_FILE" "$STDERR_FILE" "$PLAIN_FILE"' EXIT

PASSED=0
FAILED=0
RESULT=0

# Runs the compiler over a program, with the given options. Leaves its exit
# status in RESULT, its standard output in STDOUT_FILE, its standard error in
# STDERR_FILE, and that same text without colors in PLAIN_FILE.
run_compiler() {
	local program="$1"
	shift
	".build/Flex-Bison-Compiler" "$@" <"$program" >"$STDOUT_FILE" 2>"$STDERR_FILE"
	RESULT="$?"
	sed 's/\x1b\[[0-9;]*m//g' "$STDERR_FILE" >"$PLAIN_FILE"
}

has_sanitizer_report() {
	grep --quiet --extended-regexp "$SANITIZER_PATTERN" "$STDERR_FILE"
}

# Whether a file is not a program but belongs to one: the messages it must
# report (.stderr), the output it must give (.stdout) or its options (.args).
is_companion() {
	case "$1" in
		*.stderr | *.stdout | *.args) return 0 ;;
	esac
	return 1
}

# Prints the first non-empty line of the expected-messages file that is missing
# from the standard error of the last run, if there is one.
first_missing_message() {
	local expected="$1"
	local line
	while IFS= read -r line || [ -n "$line" ]; do
		if [ -n "${line//[[:space:]]/}" ] && ! grep --quiet --fixed-strings -- "$line" "$PLAIN_FILE"; then
			echo "$line"
			return
		fi
	done <"$expected"
}

pass() {
	PASSED=$((PASSED + 1))
	echo -e "    $1, ${GREEN}and it does${OFF} (status $RESULT)"
}

fail() {
	FAILED=$((FAILED + 1))
	echo -e "    $1, ${RED}but $2${OFF} (status $RESULT)"
}

# Runs the compiler (syntax only) over every program in a folder, and expects
# its exit status to be exactly the given one and its standard error to hold
# no sanitizer report.
#
# Arguments: folder, expected status, and the status of the opposite outcome
# with the words that describe it.
check_syntax() {
	local folder="$1"
	local expected="$2"
	local opposite="$3"
	local oppositeMessage="$4"
	local file
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		run_compiler "$file" --syntax-only
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" == "$expected" ]; then
			pass "$(basename "$file")"
		elif [ "$RESULT" == "$opposite" ]; then
			fail "$(basename "$file")" "it $oppositeMessage"
		else
			fail "$(basename "$file")" "it fails unexpectedly"
		fi
	done
}

# Runs the compiler up to the end of the semantic analysis (--check) over every
# program in a folder. A program must be accepted, with no error in its standard
# error. If it has an "NN-name.stderr" file next to it, each non-empty line of
# it must also be in the standard error.
check_semantic_accept() {
	local folder="$1"
	local file
	local missing
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		is_companion "$file" && continue
		run_compiler "$file" --check
		missing=""
		[ -f "$file.stderr" ] && missing="$(first_missing_message "$file.stderr")"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" == "1" ]; then
			fail "$(basename "$file")" "it rejects"
		elif [ "$RESULT" != "0" ]; then
			fail "$(basename "$file")" "it fails unexpectedly"
		elif grep --quiet --fixed-strings '[ERROR]' "$PLAIN_FILE"; then
			fail "$(basename "$file")" "it logs an error"
		elif [ -n "$missing" ]; then
			fail "$(basename "$file")" "this message is missing: $missing"
		else
			pass "$(basename "$file")"
		fi
	done
}

# Checks the messages of the last run against an "NN-name.stderr" file: each
# non-empty line of it must be in the standard error, and the lines of the
# component named in the second argument must be as many as those. Prints what
# is wrong, if anything is.
exhaustive_messages_problem() {
	local expected="$1"
	local component="$2"
	local missing
	local reported
	local wanted
	missing="$(first_missing_message "$expected")"
	reported="$(grep --count "$component" "$PLAIN_FILE")"
	wanted="$(grep --count '[^[:space:]]' "$expected")"
	if [ -n "$missing" ]; then
		echo "this message is missing: $missing"
	elif [ "$reported" != "$wanted" ]; then
		echo "it reports $reported messages instead of $wanted"
	fi
}

# Runs the compiler up to the end of the semantic analysis (--check) over every
# program in a folder. A program must be rejected (status 1) with the messages
# of its "NN-name.stderr" file, which are exhaustive: each non-empty line must
# be in the standard error, and the lines of the semantic analyzer in it must be
# as many as those.
check_semantic_reject() {
	local folder="$1"
	local file
	local problem
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		is_companion "$file" && continue
		run_compiler "$file" --check
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" == "0" ]; then
			fail "$(basename "$file")" "it accepts"
		elif [ "$RESULT" != "1" ]; then
			fail "$(basename "$file")" "it fails unexpectedly"
		elif [ ! -f "$file.stderr" ]; then
			fail "$(basename "$file")" "it has no .stderr file"
		else
			problem="$(exhaustive_messages_problem "$file.stderr" 'SemanticAnalyzer')"
			if [ -n "$problem" ]; then
				fail "$(basename "$file")" "$problem"
			else
				pass "$(basename "$file")"
			fi
		fi
	done
}

# Runs the whole compiler over every program in a folder, with the seed 1 and
# the options of its "NN-name.args" file, if it has one. A battle must end
# (status 0) with no error in the standard error, and write exactly what its
# "NN-name.stdout" file has.
check_simulation_accept() {
	local folder="$1"
	local file
	local options
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		is_companion "$file" && continue
		options=()
		[ -f "$file.args" ] && read -r -a options <"$file.args"
		run_compiler "$file" --seed 1 "${options[@]}"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" != "0" ]; then
			fail "$(basename "$file")" "it fails (the simulation does not end)"
		elif grep --quiet --fixed-strings '[ERROR]' "$PLAIN_FILE"; then
			fail "$(basename "$file")" "it logs an error"
		elif [ ! -f "$file.stdout" ]; then
			fail "$(basename "$file")" "it has no .stdout file"
		elif ! diff --brief "$file.stdout" "$STDOUT_FILE" >/dev/null; then
			fail "$(basename "$file")" "its output is not the one in the .stdout file"
			diff "$file.stdout" "$STDOUT_FILE" | head -n 6 | sed 's/^/        /'
		else
			pass "$(basename "$file")"
		fi
	done
}

# Runs the whole compiler over every program in a folder, with the seed 1 and
# the options of its "NN-name.args" file, if it has one. The simulation must
# fail (status 101) with the messages of its "NN-name.stderr" file, which are
# exhaustive: each non-empty line must be in the standard error, and the lines
# of the simulator in it must be as many as those.
check_simulation_reject() {
	local folder="$1"
	local file
	local options
	local problem
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		is_companion "$file" && continue
		options=()
		[ -f "$file.args" ] && read -r -a options <"$file.args"
		run_compiler "$file" --seed 1 "${options[@]}"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" == "0" ]; then
			fail "$(basename "$file")" "it ends"
		elif [ "$RESULT" != "101" ]; then
			fail "$(basename "$file")" "its status is not 101"
		elif [ ! -f "$file.stderr" ]; then
			fail "$(basename "$file")" "it has no .stderr file"
		else
			problem="$(exhaustive_messages_problem "$file.stderr" 'Simulator')"
			if [ -n "$problem" ]; then
				fail "$(basename "$file")" "$problem"
			else
				pass "$(basename "$file")"
			fi
		fi
	done
}

echo "Compiler should accept..."
echo ""
check_syntax "src/test/c/accept" 0 1 "rejects"
echo ""

echo "Compiler should reject..."
echo ""
check_syntax "src/test/c/reject" 1 0 "accepts"
echo ""

echo "Semantic analysis should accept..."
echo ""
check_semantic_accept "src/test/c/semantic-accept"
echo ""

echo "Semantic analysis should reject..."
echo ""
check_semantic_reject "src/test/c/semantic-reject"
echo ""

echo "Simulation should give..."
echo ""
check_simulation_accept "src/test/c/simulation"
echo ""

echo "Simulation should fail..."
echo ""
check_simulation_reject "src/test/c/simulation-reject"
echo ""

if [ "$FAILED" == "0" ]; then
	echo -e "${GREEN}$PASSED passed, $FAILED failed${OFF}"
else
	echo -e "${RED}$PASSED passed, $FAILED failed${OFF}"
fi
echo ""

echo "All done."
if [ "$FAILED" != "0" ]; then
	exit 1
fi
exit 0
