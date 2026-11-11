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

# UndefinedBehaviorSanitizer reports as "<file>:<line>:<column>: runtime error:",
# and the anchor leaves out the errors of the compiled program that are worded alike.
SANITIZER_PATTERN='AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|[0-9]+:[0-9]+: runtime error:'

STDERR_FILE="$(mktemp)"
PLAIN_FILE="$(mktemp)"
trap 'rm --force "$STDERR_FILE" "$PLAIN_FILE"' EXIT

PASSED=0
FAILED=0
RESULT=0

# Runs the compiler over a program, with the given options. Leaves its exit
# status in RESULT, its standard error in STDERR_FILE, and that same text
# without colors in PLAIN_FILE. The standard output is discarded.
run_compiler() {
	local program="$1"
	shift
	".build/Flex-Bison-Compiler" "$@" <"$program" >/dev/null 2>"$STDERR_FILE"
	RESULT="$?"
	sed 's/\x1b\[[0-9;]*m//g' "$STDERR_FILE" >"$PLAIN_FILE"
}

has_sanitizer_report() {
	grep --quiet --extended-regexp "$SANITIZER_PATTERN" "$STDERR_FILE"
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

# Runs the whole compiler over every program in a folder. A program must be
# accepted, with no error in its standard error. If it has an "NN-name.stderr"
# file next to it, each non-empty line of it must also be in the standard error.
check_semantic_accept() {
	local folder="$1"
	local file
	local missing
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		case "$file" in *.stderr) continue ;; esac
		run_compiler "$file"
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

# Runs the whole compiler over every program in a folder. A program must be
# rejected (status 1) with the messages of its "NN-name.stderr" file, which are
# exhaustive: each non-empty line must be in the standard error, and the lines
# of the semantic analyzer in it must be as many as those.
check_semantic_reject() {
	local folder="$1"
	local file
	local missing
	local reported
	local expected
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		case "$file" in *.stderr) continue ;; esac
		run_compiler "$file"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" == "0" ]; then
			fail "$(basename "$file")" "it accepts"
		elif [ "$RESULT" != "1" ]; then
			fail "$(basename "$file")" "it fails unexpectedly"
		elif [ ! -f "$file.stderr" ]; then
			fail "$(basename "$file")" "it has no .stderr file"
		else
			missing="$(first_missing_message "$file.stderr")"
			reported="$(grep --count 'SemanticAnalyzer' "$PLAIN_FILE")"
			expected="$(grep --count '[^[:space:]]' "$file.stderr")"
			if [ -n "$missing" ]; then
				fail "$(basename "$file")" "this message is missing: $missing"
			elif [ "$reported" != "$expected" ]; then
				fail "$(basename "$file")" "it reports $reported messages instead of $expected"
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
