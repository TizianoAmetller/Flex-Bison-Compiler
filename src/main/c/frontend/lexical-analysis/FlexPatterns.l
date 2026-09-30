%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus DiceLexemeAction();
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus KeywordLexemeAction(TokenLabel label);
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus NumberTooLongLexemeAction();
extern CompilationStatus PunctuationLexemeAction(TokenLabel label);
extern CompilationStatus StringLexemeAction();
extern CompilationStatus UnknownLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
whitespace			[[:space:]]+
/**
 * Capped at 9 digits: "IntegerLexemeAction" converts this with "atoi" into a
 * "signed int", and 9 digits (max 999999999) always fits safely under
 * INT_MAX (2147483647, 10 digits) regardless of the actual value, so this
 * bound alone rules out the conversion ever overflowing.
 */
digit				[[:digit:]]{1,9}
/**
 * A run of digits of any length. It only exists to catch what "digit" can't
 * hold (10+ digits, alone or as a side of a dice literal), so that such a
 * number is reported as too long instead of being split into two adjacent
 * numbers, which the parser would reject with a misleading message. It never
 * wins over "digit" for 9 digits or fewer: on a tie Flex picks the rule that
 * comes first, and the "digit" rules come first.
 */
digitRun			[[:digit:]]+
identifier			[A-Za-z_][A-Za-z0-9_]*
/**
 * One multi-byte UTF-8 character (2 to 4 bytes). The language is ASCII-only,
 * so a character like "é" outside a string or a comment is an error, but it
 * is matched whole so the error names the character instead of its first
 * byte alone, which is not valid UTF-8 on its own.
 */
utf8Character		[\xC2-\xDF][\x80-\xBF]|[\xE0-\xEF][\x80-\xBF]{2}|[\xF0-\xF4][\x80-\xBF]{3}

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }
"//"[^\n]*							{ return IgnoredLexemeAction(); }

 /** Keywords (must precede the general "identifier" pattern below). */
"abilities"							{ return KeywordLexemeAction(ABILITIES); }
"ability"							{ return KeywordLexemeAction(ABILITY); }
"and"								{ return KeywordLexemeAction(AND); }
"apply"								{ return KeywordLexemeAction(APPLY); }
"arena"								{ return KeywordLexemeAction(ARENA); }
"around"							{ return KeywordLexemeAction(AROUND); }
"at"								{ return KeywordLexemeAction(AT); }
"away"								{ return KeywordLexemeAction(AWAY); }
"battle"							{ return KeywordLexemeAction(BATTLE); }
"deal"								{ return KeywordLexemeAction(DEAL); }
"effect"							{ return KeywordLexemeAction(EFFECT); }
"else"								{ return KeywordLexemeAction(ELSE); }
"encounter"							{ return KeywordLexemeAction(ENCOUNTER); }
"false"								{ return KeywordLexemeAction(FALSE); }
"for"								{ return KeywordLexemeAction(FOR); }
"from"								{ return KeywordLexemeAction(FROM); }
"heal"								{ return KeywordLexemeAction(HEAL); }
"if"								{ return KeywordLexemeAction(IF); }
"in"								{ return KeywordLexemeAction(IN); }
"log"								{ return KeywordLexemeAction(LOG); }
"move"								{ return KeywordLexemeAction(MOVE); }
"not"								{ return KeywordLexemeAction(NOT); }
"of"								{ return KeywordLexemeAction(OF); }
"on"								{ return KeywordLexemeAction(ON); }
"or"								{ return KeywordLexemeAction(OR); }
"party"								{ return KeywordLexemeAction(PARTY); }
"radius"							{ return KeywordLexemeAction(RADIUS); }
"to"								{ return KeywordLexemeAction(TO); }
"toward"							{ return KeywordLexemeAction(TOWARD); }
"true"								{ return KeywordLexemeAction(TRUE); }
"turn"								{ return KeywordLexemeAction(TURN); }
"unit"								{ return KeywordLexemeAction(UNIT); }
"use"								{ return KeywordLexemeAction(USE); }
"while"								{ return KeywordLexemeAction(WHILE); }

 /** Operators. */
"=="								{ return PunctuationLexemeAction(EQUALS); }
"!="								{ return PunctuationLexemeAction(NOT_EQUALS); }
"<="								{ return PunctuationLexemeAction(LESS_EQUAL); }
">="								{ return PunctuationLexemeAction(GREATER_EQUAL); }
"<"									{ return PunctuationLexemeAction(LESS_THAN); }
">"									{ return PunctuationLexemeAction(GREATER_THAN); }
"="									{ return PunctuationLexemeAction(ASSIGN); }
"+"									{ return PunctuationLexemeAction(ADD); }
"-"									{ return PunctuationLexemeAction(SUB); }
"*"									{ return PunctuationLexemeAction(MUL); }
"/"									{ return PunctuationLexemeAction(DIV); }

 /** Punctuation. */
"{"									{ return PunctuationLexemeAction(OPEN_BRACE); }
"}"									{ return PunctuationLexemeAction(CLOSE_BRACE); }
"["									{ return PunctuationLexemeAction(OPEN_BRACKET); }
"]"									{ return PunctuationLexemeAction(CLOSE_BRACKET); }
"("									{ return PunctuationLexemeAction(OPEN_PARENTHESIS); }
")"									{ return PunctuationLexemeAction(CLOSE_PARENTHESIS); }
","									{ return PunctuationLexemeAction(COMMA); }
"."									{ return PunctuationLexemeAction(DOT); }
":"									{ return PunctuationLexemeAction(COLON); }

 /** Literals. */
{digit}"d"{digit}					{ return DiceLexemeAction(); }
{digit}								{ return IntegerLexemeAction(); }
{digitRun}"d"{digitRun}				{ return NumberTooLongLexemeAction(); }
{digitRun}							{ return NumberTooLongLexemeAction(); }
\"[^\"\n]*\"						{ return StringLexemeAction(); }
{identifier}						{ return IdentifierLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
{utf8Character}						{ return UnknownLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
