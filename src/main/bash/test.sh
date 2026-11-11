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

SANITIZER_PATTERN='AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:'

STDERR_FILE="$(mktemp)"
trap 'rm --force "$STDERR_FILE"' EXIT

PASSED=0
FAILED=0

# Runs the compiler (syntax only) over every program in a folder, and expects
# its exit status to be exactly the given one and its standard error to hold
# no sanitizer report. The standard output is discarded.
#
# Arguments: folder, expected status, and the messages of the two failures that
# have their own wording: the status of the opposite outcome, and the rest.
check() {
	local folder="$1"
	local expected="$2"
	local opposite="$3"
	local oppositeMessage="$4"
	local file
	local test
	local result
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		test="$(basename "$file")"
		".build/Flex-Bison-Compiler" --syntax-only <"$file" >/dev/null 2>"$STDERR_FILE"
		result="$?"
		if grep --quiet --extended-regexp "$SANITIZER_PATTERN" "$STDERR_FILE"; then
			FAILED=$((FAILED + 1))
			echo -e "    $test, ${RED}but there is a sanitizer report${OFF} (status $result)"
		elif [ "$result" == "$expected" ]; then
			PASSED=$((PASSED + 1))
			echo -e "    $test, ${GREEN}and it does${OFF} (status $result)"
		elif [ "$result" == "$opposite" ]; then
			FAILED=$((FAILED + 1))
			echo -e "    $test, ${RED}but it $oppositeMessage${OFF} (status $result)"
		else
			FAILED=$((FAILED + 1))
			echo -e "    $test, ${RED}but it fails unexpectedly${OFF} (status $result)"
		fi
	done
}

echo "Compiler should accept..."
echo ""
check "src/test/c/accept" 0 1 "rejects"
echo ""

echo "Compiler should reject..."
echo ""
check "src/test/c/reject" 1 0 "accepts"
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
