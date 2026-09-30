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
digit				[[:digit:]]+
identifier			[A-Za-z_][A-Za-z0-9_]*

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
"at"								{ return KeywordLexemeAction(AT); }
"battle"							{ return KeywordLexemeAction(BATTLE); }
"deal"								{ return KeywordLexemeAction(DEAL); }
"else"								{ return KeywordLexemeAction(ELSE); }
"encounter"							{ return KeywordLexemeAction(ENCOUNTER); }
"false"								{ return KeywordLexemeAction(FALSE); }
"for"								{ return KeywordLexemeAction(FOR); }
"heal"								{ return KeywordLexemeAction(HEAL); }
"if"								{ return KeywordLexemeAction(IF); }
"in"								{ return KeywordLexemeAction(IN); }
"log"								{ return KeywordLexemeAction(LOG); }
"not"								{ return KeywordLexemeAction(NOT); }
"of"								{ return KeywordLexemeAction(OF); }
"on"								{ return KeywordLexemeAction(ON); }
"or"								{ return KeywordLexemeAction(OR); }
"party"								{ return KeywordLexemeAction(PARTY); }
"radius"							{ return KeywordLexemeAction(RADIUS); }
"to"								{ return KeywordLexemeAction(TO); }
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
\"[^\"\n]*\"						{ return StringLexemeAction(); }
{identifier}						{ return IdentifierLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
