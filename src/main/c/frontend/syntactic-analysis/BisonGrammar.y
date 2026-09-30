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
	ArenaDeclaration * arenaDeclaration;
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
%destructor { destroyArenaDeclaration($$); } <arenaDeclaration>
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
%token <token> ARENA
%token <token> AROUND
%token <token> AT
%token <token> AWAY
%token <token> BATTLE
%token <token> DEAL
%token <token> ELSE
%token <token> ENCOUNTER
%token <token> FALSE
%token <token> FOR
%token <token> FROM
%token <token> HEAL
%token <token> IF
%token <token> IN
%token <token> LOG
%token <token> MOVE
%token <token> NOT
%token <token> OF
%token <token> ON
%token <token> OR
%token <token> PARTY
%token <token> RADIUS
%token <token> TO
%token <token> TOWARD
%token <token> TRUE
%token <token> TURN
%token <token> UNIT
%token <token> USE
%token <token> WHILE

/**
 * Terminals: punctuation and operators. Each one gets a string alias (e.g.
 * "{" for OPEN_BRACE), so a syntax-error message names the actual character
 * ("unexpected '{'") instead of the symbolic token name ("unexpected
 * OPEN_BRACE") -- the same idea as the upstream template's own alias for
 * OPEN_BRACE.
 */
%token <token> ASSIGN "="
%token <token> CLOSE_BRACE "}"
%token <token> CLOSE_BRACKET "]"
%token <token> CLOSE_COMMENT "*/"
%token <token> CLOSE_PARENTHESIS ")"
%token <token> COLON ":"
%token <token> COMMA ","
%token <token> DOT "."
%token <token> EQUALS "=="
%token <token> GREATER_EQUAL ">="
%token <token> GREATER_THAN ">"
%token <token> LESS_EQUAL "<="
%token <token> LESS_THAN "<"
%token <token> NOT_EQUALS "!="
%token <token> OPEN_BRACE "{"
%token <token> OPEN_BRACKET "["
%token <token> OPEN_COMMENT "/*"
%token <token> OPEN_PARENTHESIS "("

%token <token> ADD "+"
%token <token> DIV "/"
%token <token> MUL "*"
%token <token> SUB "-"

%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <abilityDeclaration> abilityDeclaration
%type <arenaDeclaration> arenaDeclaration
%type <position> aroundClause
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
%precedence NOT
%nonassoc EQUALS NOT_EQUALS LESS_THAN GREATER_THAN LESS_EQUAL GREATER_EQUAL
%left ADD SUB
%left MUL DIV
/**
 * A dedicated level for "RADIUS expression OF expression" (see its %prec
 * below), strictly between MUL/DIV and DOT. Using %prec DOT there (as this
 * rule originally did) ties the rule's precedence with DOT's, and since DOT
 * is %left, Bison breaks that tie by reducing -- so "radius 5 of
 * target.health" was grouping as "(radius 5 of target).health" instead of
 * "radius 5 of (target.health)". Placing RADIUS below DOT here makes DOT
 * win that comparison instead (token precedence > rule precedence: shift,
 * i.e. let ".health" extend "target" before closing the radius-of form),
 * while still sitting above every other operator (ADD, MUL, comparisons,
 * AND, OR: rule precedence > token precedence there, so those still reduce
 * early, e.g. "radius 5 of target + 1" stays "(radius 5 of target) + 1").
 */
%precedence RADIUS
%precedence DOT

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
	| arenaDeclaration											{ $$ = ArenaDeclarationSemanticAction($1); }
	| battleDeclaration											{ $$ = BattleDeclarationSemanticAction($1); }
	;

/** unit <name> [at (<x>, <y>)] { <attributes> [abilities: [<id>, ...]] } */
unitDeclaration: UNIT ID positionClause OPEN_BRACE attributeList abilitiesClause CLOSE_BRACE
																{ $$ = UnitSemanticAction($ID, $positionClause, $attributeList, $abilitiesClause); }
	;

positionClause: AT OPEN_PARENTHESIS expression COMMA expression CLOSE_PARENTHESIS
																{ $$ = PositionSemanticAction($3, $5); }
	| %empty													{ $$ = NULL; }
	;

/**
 * The trailing-comma alternative below ("attribute COMMA" with nothing
 * after) exists only so "speed: 6, abilities: [...]" parses -- without it,
 * the comma right before "abilities:" reads naturally but is rejected
 * ("unexpected ABILITIES, expecting ID"), since attributeList and
 * abilitiesClause are otherwise unrelated productions with no comma of
 * their own between them. This is unambiguous (unlike memberList, which
 * deliberately still rejects a trailing comma, see
 * reject/08-trailing-comma): after "attribute COMMA", an ID lookahead can
 * only start another attribute, while ABILITIES or CLOSE_BRACE can only
 * end the list.
 */
attributeList: attribute COMMA attributeList					{ $$ = AddAttributeSemanticAction($1, $3); }
	| attribute COMMA											{ $$ = AddAttributeSemanticAction($1, NULL); }
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
																{ $$ = AbilitiesClauseSemanticAction($idList); }
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
abilityDeclaration: ABILITY ID[name] ON ID[targetParameter] targetTypeClause OPEN_BRACE statementList CLOSE_BRACE
																{ $$ = AbilitySemanticAction($name, $targetParameter, $targetTypeClause, $statementList); }
	;

targetTypeClause: COLON idList									{ $$ = TargetTypeClauseSemanticAction($idList); }
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
 * or a declared unit repeated a number of times (e.g. "Archer * 20", or
 * "Archer * 2d6" for a randomized army size -- the quantity is a general
 * "expression", not just a literal, matching "Member.quantity"'s type in
 * the AST). Both forms reference the very same "unit" declaration; whether
 * a repeated unit is instantiated as N independent copies (sharing that
 * declaration's attributes/abilities) is a semantic-analysis concern
 * (Stage III).
 *
 * Each form also takes its own optional position, which is what tells the
 * two apart in the grammar: a single member is placed exactly ("Hero at (0,
 * 0)", reusing the unit's "positionClause"), while a repeated one is placed
 * as a group around a point ("Archer * 20 around (10, 5)"). Mixing them up
 * ("Hero around (0, 0)", "Archer * 20 at (1, 1)") is deliberately a syntax
 * error: a lone unit has nothing to surround, and twenty units can't share
 * one exact spot.
 */
member: ID[unitName] positionClause								{ $$ = MemberSemanticAction($unitName, NULL, $positionClause); }
	| ID[unitName] MUL expression[quantity] aroundClause		{ $$ = MemberSemanticAction($unitName, $quantity, $aroundClause); }
	;

aroundClause: AROUND OPEN_PARENTHESIS expression[x] COMMA expression[y] CLOSE_PARENTHESIS
																{ $$ = PositionSemanticAction($x, $y); }
	| %empty													{ $$ = NULL; }
	;

/** arena (<width>, <height>) */
arenaDeclaration: ARENA OPEN_PARENTHESIS expression[width] COMMA expression[height] CLOSE_PARENTHESIS
																{ $$ = ArenaSemanticAction($width, $height); }
	;

/** battle: [<teamName>, ...] (any number of participating teams). */
battleDeclaration: BATTLE COLON OPEN_BRACKET idList CLOSE_BRACKET
																{ $$ = BattleSemanticAction($4); }
	;

block: OPEN_BRACE statementList CLOSE_BRACE					{ $$ = BlockSemanticAction($statementList); }
	;

statementList: statement statementList							{ $$ = AddStatementSemanticAction($1, $2); }
	| %empty													{ $$ = NULL; }
	;

statement: DEAL expression TO expression						{ $$ = DealStatementSemanticAction($2, $4); }
	| HEAL expression TO expression							{ $$ = HealStatementSemanticAction($2, $4); }
	| USE ID ON expression										{ $$ = UseStatementSemanticAction($2, $4); }
	| APPLY ID TO expression FOR expression					{ $$ = ApplyStatementSemanticAction($2, $4, $6); }
	| LOG STRING												{ $$ = LogStatementSemanticAction($2); }
	| MOVE TOWARD expression[target]							{ $$ = MoveStatementSemanticAction(TOWARD_MOVE, $target); }
	| MOVE AWAY FROM expression[target]						{ $$ = MoveStatementSemanticAction(AWAY_MOVE, $target); }
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
 * statement form (deal/heal/use/apply/log/move/if/for/while); allowing an
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
	 * "%prec RADIUS" (see that dedicated precedence level, above -- between
	 * MUL/DIV and DOT) makes this production reduce as soon as "of
	 * expression" completes for operators below it (e.g. "radius 5 of
	 * target + 1" stays "(radius 5 of target) + 1"), while still letting a
	 * trailing ".member" bind to "center" first (e.g. "radius 5 of
	 * target.health" is "radius 5 of (target.health)", not
	 * "(radius 5 of target).health"), because DOT's own precedence is
	 * higher. Without a precedence here, this alternative's completed form
	 * directly competes -unresolved- with every other binary-operator
	 * continuation at the same point, since "expression" is shared by all of
	 * them.
	 */
	| RADIUS expression OF expression[center] %prec RADIUS		{ $$ = RadiusExpressionSemanticAction($2, $center); }
	| expression[object] DOT ID								{ $$ = MemberExpressionSemanticAction($object, $3); }
	| OPEN_PARENTHESIS expression[inner] CLOSE_PARENTHESIS	{ $$ = ParenthesizedExpressionSemanticAction($inner); }
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
	| ID callSuffix												{ $$ = IdOrCallExpressionSemanticAction($ID, $callSuffix); }
	;

callSuffix: OPEN_PARENTHESIS argumentList CLOSE_PARENTHESIS	{ $$ = CallSuffixSemanticAction($argumentList); }
	| %empty													{ $$ = NoCallSuffixSemanticAction(); }
	;

argumentList: expression COMMA argumentList					{ $$ = AddExpressionSemanticAction($1, $3); }
	| expression												{ $$ = AddExpressionSemanticAction($1, NULL); }
	| %empty													{ $$ = NULL; }
	;

%%
