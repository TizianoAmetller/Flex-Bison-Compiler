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
DATA_FILE="$(mktemp)"
trap 'rm --force "$STDOUT_FILE" "$STDERR_FILE" "$PLAIN_FILE" "$DATA_FILE"' EXIT

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
# report (.stderr), the output it must give (.stdout), the data of its replay
# (.data) or its options (.args).
is_companion() {
	case "$1" in
		*.stderr | *.stdout | *.data | *.args) return 0 ;;
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
# no sanitizer report. If a program has an "NN-name.stderr" file next to it, the
# messages of the parser must be those of the file, which is exhaustive: each
# non-empty line must be in the standard error, and the lines of the parser in
# it must be as many as those.
#
# Arguments: folder, expected status, and the status of the opposite outcome
# with the words that describe it.
check_syntax() {
	local folder="$1"
	local expected="$2"
	local opposite="$3"
	local oppositeMessage="$4"
	local file
	local problem
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		is_companion "$file" && continue
		run_compiler "$file" --syntax-only
		problem=""
		[ -f "$file.stderr" ] && problem="$(exhaustive_messages_problem "$file.stderr" 'BisonActions')"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" == "$expected" ] && [ -n "$problem" ]; then
			fail "$(basename "$file")" "$problem"
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

# The page that has to be written, whatever the program: it is an HTML document, it
# has the content security policy, and it loads nothing from outside.
CONTENT_SECURITY_POLICY="<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:\">"

# Prints what is wrong with the page in the last standard output, if anything is.
html_problem() {
	if [ "$(head -n 1 "$STDOUT_FILE")" != "<!DOCTYPE html>" ]; then
		echo "it does not start with <!DOCTYPE html>"
	elif [ "$(tail -n 1 "$STDOUT_FILE")" != "</html>" ]; then
		echo "it does not end with </html>"
	elif ! grep --text --quiet --fixed-strings "$CONTENT_SECURITY_POLICY" "$STDOUT_FILE"; then
		echo "it has no content security policy"
	elif grep --text --quiet --ignore-case --extended-regexp 'http://|https://|<script src|<link' "$STDOUT_FILE"; then
		echo "it loads something from outside"
	fi
}

# Runs the compiler over every program in a folder, to write the page of its
# replay (--format html), with the seed 1 and the options of its "NN-name.args"
# file, if it has one. The battle must end (status 0, with no error) or, if there
# is an "NN-name.stderr" file, fail as it says (status 101, as in
# simulation-reject). The page must be right (see "html_problem"), and the
# lines of its data block must be those of the "NN-name.data" file and have no
# "<", ">" or "&" in them, which the program of the user could use to get out.
check_html() {
	local folder="$1"
	local file
	local options
	local expected
	local problem
	for file in "$folder"/*; do
		[ -f "$file" ] || continue
		is_companion "$file" && continue
		options=()
		[ -f "$file.args" ] && read -r -a options <"$file.args"
		expected=0
		[ -f "$file.stderr" ] && expected=101
		run_compiler "$file" --format html --seed 1 "${options[@]}"
		awk '/<script id="battle-data" type="application\/json">/ { inside = 1; next } inside && /<\/script>/ { exit } inside { print }' "$STDOUT_FILE" >"$DATA_FILE"
		problem="$(html_problem)"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
		elif [ "$RESULT" != "$expected" ]; then
			fail "$(basename "$file")" "its status is not $expected"
		elif [ "$expected" == "101" ] && [ -n "$(exhaustive_messages_problem "$file.stderr" 'Simulator')" ]; then
			fail "$(basename "$file")" "$(exhaustive_messages_problem "$file.stderr" 'Simulator')"
		elif [ "$expected" == "0" ] && grep --quiet --fixed-strings '[ERROR]' "$PLAIN_FILE"; then
			fail "$(basename "$file")" "it logs an error"
		elif [ -n "$problem" ]; then
			fail "$(basename "$file")" "$problem"
		elif [ ! -f "$file.data" ]; then
			fail "$(basename "$file")" "it has no .data file"
		elif grep --text --quiet '[<>&]' "$DATA_FILE"; then
			fail "$(basename "$file")" "its data have a character that is not escaped"
		elif ! diff --brief "$file.data" "$DATA_FILE" >/dev/null; then
			fail "$(basename "$file")" "its data are not the ones in the .data file"
			diff "$file.data" "$DATA_FILE" | head -n 6 | sed 's/^/        /'
		else
			pass "$(basename "$file")"
		fi
	done
}

# Prints what is wrong with the teams in the text of a battle (the last standard
# output), if anything is: each team of the header ("Team X: N units") must have
# a line at the end ("Team X: A alive, D dead") with A + D = N, and no team that is
# not in the header can have one.
team_counts_problem() {
	awk '
		/^Team [^ :]+: [0-9]+ units?$/ {
			name = $2
			sub(/:$/, "", name)
			units[name] = $3 + 0
			headers++
		}
		/^Team [^ :]+: [0-9]+ alive, [0-9]+ dead$/ {
			name = $2
			sub(/:$/, "", name)
			if (!(name in units)) {
				print "team " name " is not in the header"
				failed = 1
				exit
			}
			if ($3 + $5 != units[name]) {
				print "team " name " ends with " $3 " alive and " $5 " dead, and it started with " units[name] " units"
				failed = 1
				exit
			}
			closed[name] = 1
		}
		END {
			if (failed) {
				exit
			}
			if (headers == 0) {
				print "it has no teams"
				exit
			}
			for (name in units) {
				if (!(name in closed)) {
					print "team " name " has no counts at the end"
					exit
				}
			}
		}
	' "$STDOUT_FILE"
}

# Runs the compiler over every example program (the ".rpg" files of a folder),
# with the seed 1, once for the text of the battle and once for its page. The
# battle must end with a winner (status 0, with no error) in both, the alive and
# the dead of each team in the text must add up to its units (see
# "team_counts_problem"), and the page must be right (see "html_problem").
check_examples() {
	local folder="$1"
	local file
	local problem
	for file in "$folder"/*.rpg; do
		[ -f "$file" ] || continue
		run_compiler "$file" --seed 1
		problem="$(team_counts_problem)"
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report"
			continue
		elif [ "$RESULT" != "0" ]; then
			fail "$(basename "$file")" "its battle does not end"
			continue
		elif grep --quiet --fixed-strings '[ERROR]' "$PLAIN_FILE"; then
			fail "$(basename "$file")" "it logs an error"
			continue
		elif ! grep --quiet '^Winner: ' "$STDOUT_FILE"; then
			fail "$(basename "$file")" "its battle has no winner"
			continue
		elif [ -n "$problem" ]; then
			fail "$(basename "$file")" "its teams do not add up: $problem"
			continue
		fi
		run_compiler "$file" --format html --seed 1
		if has_sanitizer_report; then
			fail "$(basename "$file")" "there is a sanitizer report in its page"
		elif [ "$RESULT" != "0" ]; then
			fail "$(basename "$file")" "its page is not written"
		elif [ -n "$(html_problem)" ]; then
			fail "$(basename "$file")" "its page is wrong: $(html_problem)"
		else
			pass "$(basename "$file")"
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

echo "HTML replay should hold..."
echo ""
check_html "src/test/c/html"
echo ""

echo "Examples should end with a winner..."
echo ""
check_examples "examples"
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
