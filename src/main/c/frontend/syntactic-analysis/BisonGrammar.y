%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"

/**
 * The error reporting function for Bison parser.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	SyntacticErrorSemanticAction(location, message);
}

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	signed int integer;
	char * string;
	DiceValue dice;
	TokenLabel token;

	/** Non-terminals. */

	AbilityDeclaration * abilityDeclaration;
	Attribute * attribute;
	AttributeList * attributeList;
	BattleDeclaration * battleDeclaration;
	CallSuffix callSuffix;
	Declaration * declaration;
	DeclarationList * declarationList;
	Expression * expression;
	ExpressionList * expressionList;
	IdentifierList * identifierList;
	Member * member;
	MemberList * memberList;
	Position * position;
	Program * program;
	Statement * statement;
	StatementList * statementList;
	TeamDeclaration * teamDeclaration;
	TurnDeclaration * turnDeclaration;
	UnitDeclaration * unitDeclaration;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { destroyAbilityDeclaration($$); } <abilityDeclaration>
%destructor { destroyAttribute($$); } <attribute>
%destructor { destroyAttributeList($$); } <attributeList>
%destructor { destroyBattleDeclaration($$); } <battleDeclaration>
%destructor { destroyCallSuffix($$); } <callSuffix>
%destructor { destroyDeclaration($$); } <declaration>
%destructor { destroyDeclarationList($$); } <declarationList>
%destructor { destroyExpression($$); } <expression>
%destructor { destroyExpressionList($$); } <expressionList>
%destructor { destroyIdentifierList($$); } <identifierList>
%destructor { destroyMember($$); } <member>
%destructor { destroyMemberList($$); } <memberList>
%destructor { destroyPosition($$); } <position>
%destructor { destroyStatement($$); } <statement>
%destructor { destroyStatementList($$); } <statementList>
%destructor { destroyTeamDeclaration($$); } <teamDeclaration>
%destructor { destroyTurnDeclaration($$); } <turnDeclaration>
%destructor { destroyUnitDeclaration($$); } <unitDeclaration>

/**
 * "ID" and "STRING" terminals carry a heap-allocated string (see
 * FlexActions.c). A successful reduction always either stores that pointer
 * into an AST node or frees it, so this destructor only ever runs on the
 * ID/STRING symbols left on the stack (or as lookahead) when a syntax error
 * aborts the parse before that happens.
 */
%destructor { free($$); } <string>

/** Terminals: literals. */
%token <integer> INTEGER
%token <dice> DICE
%token <string> STRING
%token <string> ID

/** Terminals: keywords. */
%token <token> ABILITIES
%token <token> ABILITY
%token <token> AND
%token <token> APPLY
%token <token> AT
%token <token> BATTLE
%token <token> DEAL
%token <token> ELSE
%token <token> ENCOUNTER
%token <token> FALSE
%token <token> FOR
%token <token> HEAL
%token <token> IF
%token <token> IN
%token <token> LOG
%token <token> NOT
%token <token> OF
%token <token> ON
%token <token> OR
%token <token> PARTY
%token <token> RADIUS
%token <token> TO
%token <token> TRUE
%token <token> TURN
%token <token> UNIT
%token <token> USE
%token <token> WHILE

/** Terminals: punctuation and operators. */
%token <token> ASSIGN
%token <token> CLOSE_BRACE
%token <token> CLOSE_BRACKET
%token <token> CLOSE_COMMENT
%token <token> CLOSE_PARENTHESIS
%token <token> COLON
%token <token> COMMA
%token <token> DOT
%token <token> EQUALS
%token <token> GREATER_EQUAL
%token <token> GREATER_THAN
%token <token> LESS_EQUAL
%token <token> LESS_THAN
%token <token> NOT_EQUALS
%token <token> OPEN_BRACE
%token <token> OPEN_BRACKET
%token <token> OPEN_COMMENT
%token <token> OPEN_PARENTHESIS

%token <token> ADD
%token <token> DIV
%token <token> MUL
%token <token> SUB

%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <abilityDeclaration> abilityDeclaration
%type <attribute> attribute
%type <attributeList> attributeList
%type <battleDeclaration> battleDeclaration
%type <callSuffix> callSuffix
%type <declaration> declaration
%type <declarationList> declarationList
%type <expression> expression
%type <expressionList> argumentList
%type <identifierList> abilitiesClause
%type <identifierList> idList
%type <member> member
%type <memberList> memberList
%type <position> positionClause
%type <program> program
%type <statement> statement
%type <statementList> block
%type <statementList> statementList
%type <identifierList> targetTypeClause
%type <teamDeclaration> teamDeclaration
%type <turnDeclaration> turnDeclaration
%type <unitDeclaration> unitDeclaration

/**
 * Precedence and associativity (lowest to highest).
 *
 * @see https://en.cppreference.com/w/cpp/language/operator_precedence.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%left OR
%left AND
%right NOT
%nonassoc EQUALS NOT_EQUALS LESS_THAN GREATER_THAN LESS_EQUAL GREATER_EQUAL
%left ADD SUB
%left MUL DIV
%left DOT

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

/** At least one declaration is required (an empty program is meaningless). */
program: declaration declarationList							{ $$ = ProgramSemanticAction(AddDeclarationSemanticAction($1, $2)); }
	;

declarationList: declaration declarationList					{ $$ = AddDeclarationSemanticAction($1, $2); }
	| %empty													{ $$ = NULL; }
	;

declaration: unitDeclaration									{ $$ = UnitDeclarationSemanticAction($1); }
	| abilityDeclaration										{ $$ = AbilityDeclarationSemanticAction($1); }
	| turnDeclaration											{ $$ = TurnDeclarationSemanticAction($1); }
	| teamDeclaration											{ $$ = TeamDeclarationSemanticAction($1); }
	| battleDeclaration											{ $$ = BattleDeclarationSemanticAction($1); }
	;

/** unit <name> [at (<x>, <y>)] { <attributes> [abilities: [<id>, ...]] } */
unitDeclaration: UNIT ID positionClause OPEN_BRACE attributeList abilitiesClause CLOSE_BRACE
																{ $$ = UnitSemanticAction($2, $3, $5, $6); }
	;

positionClause: AT OPEN_PARENTHESIS expression COMMA expression CLOSE_PARENTHESIS
																{ $$ = PositionSemanticAction($3, $5); }
	| %empty													{ $$ = NULL; }
	;

attributeList: attribute COMMA attributeList					{ $$ = AddAttributeSemanticAction($1, $3); }
	| attribute													{ $$ = AddAttributeSemanticAction($1, NULL); }
	;

/**
 * An attribute's name is a plain identifier (not a fixed keyword): this way
 * the very same word (e.g. "health", "attack") is also a valid member name
 * in a member-access expression (self.attack, target.defense). Checking
 * that the declared attribute names are exactly {health, attack, defense,
 * speed}, each appearing once, is a semantic-analysis concern (Stage III),
 * not a syntactic one.
 */
attribute: ID COLON expression									{ $$ = AttributeSemanticAction($1, $3); }
	;

abilitiesClause: ABILITIES COLON OPEN_BRACKET idList CLOSE_BRACKET
																{ $$ = $4; }
	| %empty													{ $$ = NULL; }
	;

idList: ID COMMA idList										{ $$ = AddIdentifierSemanticAction($1, $3); }
	| ID														{ $$ = AddIdentifierSemanticAction($1, NULL); }
	;

/**
 * ability <name> on <targetParameter> [: <targetType>, ...] { <body> }
 *
 * "targetParameter" is a plain identifier bound within the ability body (by
 * convention, "target"), not a fixed keyword: an ability's second name works
 * like a function parameter. The optional "targetTypeClause" restricts the
 * relational type(s) of a valid target (e.g. "on target: ally", "on target:
 * enemy, self"); "ally"/"enemy"/"self" are plain identifiers here too (not
 * keywords), resolved against actual team membership in Stage III.
 */
abilityDeclaration: ABILITY ID ON ID targetTypeClause OPEN_BRACE statementList CLOSE_BRACE
																{ $$ = AbilitySemanticAction($2, $4, $targetTypeClause, $7); }
	;

targetTypeClause: COLON idList									{ $$ = $2; }
	| %empty											{ $$ = NULL; }
	;

/** on turn <unitName> { <body> } */
turnDeclaration: ON TURN ID OPEN_BRACE statementList CLOSE_BRACE
																{ $$ = TurnSemanticAction($3, $5); }
	;

teamDeclaration: PARTY ID ASSIGN OPEN_BRACKET memberList CLOSE_BRACKET
																{ $$ = TeamSemanticAction(PARTY_TEAM, $2, $5); }
	| ENCOUNTER ID ASSIGN OPEN_BRACKET memberList CLOSE_BRACKET	{ $$ = TeamSemanticAction(ENCOUNTER_TEAM, $2, $5); }
	;

memberList: member COMMA memberList							{ $$ = AddMemberSemanticAction($1, $3); }
	| member											{ $$ = AddMemberSemanticAction($1, NULL); }
	;

/**
 * A team member is either a single, individually-named unit (e.g. "Hero"),
 * or a declared unit repeated a number of times (e.g. "Archer * 20"). Both
 * forms reference the very same "unit" declaration; whether a repeated unit
 * is instantiated as N independent copies (sharing that declaration's
 * attributes/abilities) is a semantic-analysis concern (Stage III).
 */
member: ID MUL INTEGER											{ $$ = MemberSemanticAction($1, IntegerExpressionSemanticAction($3)); }
	| ID												{ $$ = MemberSemanticAction($1, NULL); }
	;

/** battle: [<teamName>, ...] (any number of participating teams). */
battleDeclaration: BATTLE COLON OPEN_BRACKET idList CLOSE_BRACKET
																{ $$ = BattleSemanticAction($4); }
	;

block: OPEN_BRACE statementList CLOSE_BRACE					{ $$ = $2; }
	;

statementList: statement statementList							{ $$ = AddStatementSemanticAction($1, $2); }
	| %empty													{ $$ = NULL; }
	;

statement: DEAL expression TO expression						{ $$ = DealStatementSemanticAction($2, $4); }
	| HEAL expression TO expression							{ $$ = HealStatementSemanticAction($2, $4); }
	| USE ID ON expression										{ $$ = UseStatementSemanticAction($2, $4); }
	| APPLY ID TO expression FOR expression					{ $$ = ApplyStatementSemanticAction($2, $4, $6); }
	| LOG STRING												{ $$ = LogStatementSemanticAction($2); }
	| IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block[thenBranch] ELSE block[elseBranch]
																{ $$ = IfStatementSemanticAction($3, $thenBranch, $elseBranch); }
	| IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block	{ $$ = IfStatementSemanticAction($3, $5, NULL); }
	| FOR OPEN_PARENTHESIS ID IN expression CLOSE_PARENTHESIS block
																{ $$ = ForStatementSemanticAction($3, $5, $7); }
	| WHILE OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block
																{ $$ = WhileStatementSemanticAction($3, $5); }
	;

/**
 * NOTE: there is deliberately no "statement: expression" (bare-expression)
 * alternative. Every meaningful action already has its own keyword-led
 * statement form (deal/heal/use/apply/log/if/for/while); allowing an
 * arbitrary expression to stand alone as a statement would make the grammar
 * ambiguous, since statements have no separator (no ';', per the feedback):
 * "foo\n(bar)" could then be read either as two statements ("foo" and the
 * parenthesized expression "(bar)") or as a single call "foo(bar)", which
 * Bison reports as an unresolved shift/reduce conflict.
 */

expression: expression[left] ADD expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, ADD_EXPRESSION); }
	| expression[left] SUB expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, SUB_EXPRESSION); }
	| expression[left] MUL expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, MUL_EXPRESSION); }
	| expression[left] DIV expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, DIV_EXPRESSION); }
	| expression[left] EQUALS expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, EQUALS_EXPRESSION); }
	| expression[left] NOT_EQUALS expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, NOT_EQUALS_EXPRESSION); }
	| expression[left] LESS_THAN expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, LESS_THAN_EXPRESSION); }
	| expression[left] GREATER_THAN expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, GREATER_THAN_EXPRESSION); }
	| expression[left] LESS_EQUAL expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, LESS_EQUAL_EXPRESSION); }
	| expression[left] GREATER_EQUAL expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, GREATER_EQUAL_EXPRESSION); }
	| expression[left] AND expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, AND_EXPRESSION); }
	| expression[left] OR expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, OR_EXPRESSION); }
	| NOT expression											{ $$ = UnaryExpressionSemanticAction($2, NOT_EXPRESSION); }
	/**
	 * "radius <r> of <center>" denotes the collection of units within
	 * distance "r" of "center" (e.g., used as "for (victim in radius 5 of
	 * target) { ... }"). Computing that collection from the declared (x, y)
	 * positions is a semantic-analysis concern (Stage III); Stage II only
	 * represents it in the AST.
	 *
	 * "%prec DOT" (the tightest-binding declared precedence) makes this
	 * production reduce as soon as "of expression" completes, instead of
	 * greedily absorbing a trailing operator into "center": "radius 5 of
	 * target + 1" parses as "(radius 5 of target) + 1", the same way "NOT
	 * expression" (see its own "%right NOT" precedence, above) doesn't
	 * greedily absorb a trailing operator into its operand either. Without an
	 * explicit precedence here, this alternative's completed form directly
	 * competes -unresolved- with every other binary-operator continuation at
	 * the same point, since "expression" is shared by all of them.
	 */
	| RADIUS expression OF expression[center] %prec DOT		{ $$ = RadiusExpressionSemanticAction($2, $center); }
	| expression[object] DOT ID								{ $$ = MemberExpressionSemanticAction($object, $3); }
	| OPEN_PARENTHESIS expression CLOSE_PARENTHESIS			{ $$ = $2; }
	| DICE														{ $$ = DiceExpressionSemanticAction($1); }
	| INTEGER													{ $$ = IntegerExpressionSemanticAction($1); }
	| STRING													{ $$ = StringExpressionSemanticAction($1); }
	| TRUE														{ $$ = BooleanExpressionSemanticAction(true); }
	| FALSE														{ $$ = BooleanExpressionSemanticAction(false); }
	/**
	 * A single "ID callSuffix" rule (instead of two overlapping alternatives,
	 * "ID" and "ID OPEN_PARENTHESIS argumentList CLOSE_PARENTHESIS") avoids a
	 * shift/reduce conflict: "callSuffix" alone, deterministically, decides
	 * from the OPEN_PARENTHESIS lookahead whether this is a call.
	 */
	| ID callSuffix												{ $$ = $callSuffix.isCall ? CallExpressionSemanticAction($1, $callSuffix.arguments) : IdentifierExpressionSemanticAction($1); }
	;

callSuffix: OPEN_PARENTHESIS argumentList CLOSE_PARENTHESIS	{ $$.isCall = true; $$.arguments = $2; }
	| %empty													{ $$.isCall = false; $$.arguments = NULL; }
	;

argumentList: expression COMMA argumentList					{ $$ = AddExpressionSemanticAction($1, $3); }
	| expression												{ $$ = AddExpressionSemanticAction($1, NULL); }
	| %empty													{ $$ = NULL; }
	;

%%
