#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * This type definitions allows self-referencing types (e.g., an expression
 * that is made of another expressions, such as talking about you in 3rd
 * person, but without the madness).
 */

typedef enum DeclarationType DeclarationType;
typedef enum ExpressionType ExpressionType;
typedef enum MoveDirection MoveDirection;
typedef enum StatementType StatementType;
typedef enum TeamKind TeamKind;

typedef struct AbilityDeclaration AbilityDeclaration;
typedef struct ApplyStatement ApplyStatement;
typedef struct ArenaDeclaration ArenaDeclaration;
typedef struct Attribute Attribute;
typedef struct AttributeList AttributeList;
typedef struct BattleDeclaration BattleDeclaration;
typedef struct DealStatement DealStatement;
typedef struct Declaration Declaration;
typedef struct DeclarationList DeclarationList;
typedef struct EffectDeclaration EffectDeclaration;
typedef struct Expression Expression;
typedef struct ExpressionList ExpressionList;
typedef struct ForStatement ForStatement;
typedef struct HealStatement HealStatement;
typedef struct IdentifierList IdentifierList;
typedef struct IfStatement IfStatement;
typedef struct LogStatement LogStatement;
typedef struct Member Member;
typedef struct MemberList MemberList;
typedef struct MoveStatement MoveStatement;
typedef struct Position Position;
typedef struct Program Program;
typedef struct Statement Statement;
typedef struct StatementList StatementList;
typedef struct TeamDeclaration TeamDeclaration;
typedef struct TurnDeclaration TurnDeclaration;
typedef struct UnitDeclaration UnitDeclaration;
typedef struct UseStatement UseStatement;
typedef struct WhileStatement WhileStatement;

/**
 * A dice literal's value, e.g. "2d6" is { .count = 2, .sides = 6 }. It's not
 * a pointer type because it's small and only ever copied by value, both from
 * Flex (the token's semantic value) and into the AST (see Expression).
 */
typedef struct {
	int count;
	int sides;
} DiceValue;

/**
 * The optional "(argumentList)" suffix after an ID in an expression. It's a
 * plain struct (copied by value, like DiceValue), used to eliminate a
 * shift/reduce conflict in the grammar: instead of two overlapping
 * alternatives ("expression: ID" and "expression: ID OPEN_PARENTHESIS
 * argumentList CLOSE_PARENTHESIS", which both start by shifting ID), there is
 * a single "expression: ID callSuffix" rule, and callSuffix alone decides
 * -deterministically on the OPEN_PARENTHESIS lookahead- whether a call is
 * being made. When "isCall" is false, "arguments" is always NULL.
 */
typedef struct {
	bool isCall;
	ExpressionList * arguments;
} CallSuffix;

/**
 * Node types for the Abstract Syntax Tree (AST).
 */

enum DeclarationType {
	ABILITY_DECLARATION,
	ARENA_DECLARATION,
	BATTLE_DECLARATION,
	EFFECT_DECLARATION,
	TEAM_DECLARATION,
	TURN_DECLARATION,
	UNIT_DECLARATION
};

enum TeamKind {
	PARTY_TEAM,
	ENCOUNTER_TEAM
};

/** Whether a "move" statement closes the distance to its target, or opens it. */
enum MoveDirection {
	AWAY_MOVE,
	TOWARD_MOVE
};

enum StatementType {
	APPLY_STATEMENT,
	DEAL_STATEMENT,
	FOR_STATEMENT,
	HEAL_STATEMENT,
	IF_STATEMENT,
	LOG_STATEMENT,
	MOVE_STATEMENT,
	USE_STATEMENT,
	WHILE_STATEMENT
};

enum ExpressionType {
	/** Binary: expression OP expression (arithmetic, relational, logical). */
	ADD_EXPRESSION,
	AND_EXPRESSION,
	DIV_EXPRESSION,
	EQUALS_EXPRESSION,
	GREATER_EQUAL_EXPRESSION,
	GREATER_THAN_EXPRESSION,
	LESS_EQUAL_EXPRESSION,
	LESS_THAN_EXPRESSION,
	MUL_EXPRESSION,
	NOT_EQUALS_EXPRESSION,
	OR_EXPRESSION,
	SUB_EXPRESSION,
	/** Unary. */
	NOT_EXPRESSION,
	/** expression '.' identifier (e.g., self.attack, target.defense). */
	MEMBER_EXPRESSION,
	/** identifier '(' expressionList ')' (e.g., nearest(enemy)). */
	CALL_EXPRESSION,
	/** 'radius' expression 'of' expression (e.g., radius 5 of target). */
	RADIUS_EXPRESSION,
	/** Literals and references. */
	BOOLEAN_EXPRESSION,
	DICE_EXPRESSION,
	IDENTIFIER_EXPRESSION,
	INTEGER_EXPRESSION,
	STRING_EXPRESSION
};

/**
 * A simple singly-linked list of identifiers (lexemes), used both for
 * party/encounter membership (e.g. "[Hero, Mage]") and for a unit's
 * "abilities" clause.
 */
struct IdentifierList {
	char * identifier;
	IdentifierList * next;
};

struct ExpressionList {
	Expression * expression;
	ExpressionList * next;
};

/** A unit's declared (x, y) position. NULL when omitted (position is optional). */
struct Position {
	Expression * x;
	Expression * y;
};

/**
 * The name is a plain identifier's text (e.g. "health", "attack"), not a
 * fixed enum: see the grammar's "attribute" production for why.
 */
struct Attribute {
	char * name;
	Expression * value;
};

struct AttributeList {
	Attribute * attribute;
	AttributeList * next;
};

struct Expression {
	union {
		/** ADD, SUB, MUL, DIV, EQUALS, NOT_EQUALS, LESS_THAN, GREATER_THAN, LESS_EQUAL, GREATER_EQUAL, AND, OR. */
		struct {
			Expression * leftExpression;
			Expression * rightExpression;
		};
		/** NOT. */
		Expression * operand;
		/** MEMBER: object '.' member (e.g., self.attack). */
		struct {
			Expression * object;
			char * member;
		};
		/** CALL: functionName '(' arguments ')' (e.g., nearest(enemy)). */
		struct {
			char * functionName;
			ExpressionList * arguments;
		};
		/** RADIUS: units within "radius" distance of "center". */
		struct {
			Expression * radius;
			Expression * center;
		};
		DiceValue dice;
		int integerValue;
		bool booleanValue;
		/** Shared by STRING (raw literal, interpolation resolved in Stage III) and IDENTIFIER. */
		char * text;
	};
	ExpressionType type;
};

/** deal <amount> to <target> */
struct DealStatement {
	Expression * amount;
	Expression * target;
};

/** heal <amount> to <target> */
struct HealStatement {
	Expression * amount;
	Expression * target;
};

/** use <abilityName> on <target> */
struct UseStatement {
	char * abilityName;
	Expression * target;
};

/** apply <effectName> to <target> for <duration> */
struct ApplyStatement {
	char * effectName;
	Expression * target;
	Expression * duration;
};

/** log <message> (a string literal; "{identifier}" is interpolated in Stage III). */
struct LogStatement {
	char * message;
};

/**
 * move toward <target> | move away from <target>
 *
 * The unit running the statement moves relative to "target" (a unit or a
 * position-bearing expression); how far it gets is up to its speed, which is
 * a Stage III concern.
 */
struct MoveStatement {
	MoveDirection direction;
	Expression * target;
};

struct IfStatement {
	Expression * condition;
	StatementList * thenBranch;
	/** NULL when there's no "else" branch. */
	StatementList * elseBranch;
};

/** for (<iterator> in <collection>) { ... } */
struct ForStatement {
	char * iterator;
	Expression * collection;
	StatementList * body;
};

struct WhileStatement {
	Expression * condition;
	StatementList * body;
};

struct Statement {
	union {
		ApplyStatement * applyStatement;
		DealStatement * dealStatement;
		ForStatement * forStatement;
		HealStatement * healStatement;
		IfStatement * ifStatement;
		LogStatement * logStatement;
		MoveStatement * moveStatement;
		UseStatement * useStatement;
		WhileStatement * whileStatement;
	};
	StatementType type;
};

struct StatementList {
	Statement * statement;
	StatementList * next;
};

/**
 * unit <name> [: <tag>, ...] [at (<x>, <y>)] {
 *     health: <e>, attack: <e>, defense: <e>, speed: <e>
 *     [abilities: [<id>, ...]]
 * }
 *
 * The tags are the kinds of unit this one belongs to (e.g. "unit Skeleton:
 * undead, beast { ... }"), which an ability can target by name ("on target:
 * undead"), next to the relational types ally/enemy/self. Like those, they
 * are plain identifiers, not keywords; resolving them is a Stage III concern.
 */
struct UnitDeclaration {
	char * name;
	/** NULL when the unit declares no tags. */
	IdentifierList * tags;
	/** NULL when the position is omitted. */
	Position * position;
	AttributeList * attributes;
	/** NULL when the unit has no abilities. */
	IdentifierList * abilities;
};

/**
 * ability <name> on <targetParameter> [: <targetType>, ...] { <body> }
 *
 * "targetTypes" is the optional relational-type restriction on the target
 * (e.g. "on target: ally", "on target: enemy, self"). The type names
 * themselves (ally/enemy/self) are plain identifiers, not keywords -
 * resolving them against actual team membership is a semantic-analysis
 * concern (Stage III). NULL when the clause is omitted (any target valid).
 */
struct AbilityDeclaration {
	char * name;
	char * targetParameter;
	IdentifierList * targetTypes;
	StatementList * body;
};

/**
 * effect <name> on <targetParameter> { <body> }
 *
 * What the effect named in "apply <name> to <target> for <turns>" does to the
 * unit carrying it (e.g. "effect Poison on victim { deal 2 to victim }"). The
 * body is the same statement language as an ability's, and "targetParameter"
 * is a plain identifier bound to the affected unit inside it. When and how
 * often the body runs while the effect lasts is a Stage III concern, and so
 * is checking that every applied effect has a declaration.
 */
struct EffectDeclaration {
	char * name;
	char * targetParameter;
	StatementList * body;
};

/** on turn <unitName> { <body> } */
struct TurnDeclaration {
	char * unitName;
	StatementList * body;
};

/**
 * A single party/encounter member: a reference to a declared "unit" name,
 * optionally repeated "quantity" times (e.g. "Archer * 20"). Both a plain
 * "Hero" and a repeated "Archer * 20" reference the very same kind of "unit"
 * declaration; whether a repeated unit is instantiated as N independent
 * copies (sharing that declaration's attributes/abilities) is a
 * semantic-analysis concern (Stage III).
 *
 * The grammar only allows "at (x, y)" on a single member ("Hero at (0, 0)")
 * and "around (x, y)" on a repeated one ("Archer * 20 around (10, 5)"), so
 * "position" needs no separate kind: it is the exact spot when "quantity" is
 * NULL, or the center of the group when it isn't. Either way it overrides
 * the position declared on the unit itself, if any.
 */
struct Member {
	char * unitName;
	/** NULL when no multiplier was given (a single instance). */
	Expression * quantity;
	/** NULL when the member has no position of its own. */
	Position * position;
};

struct MemberList {
	Member * member;
	MemberList * next;
};

/** (party|encounter) <name> = [<member>, ...] */
struct TeamDeclaration {
	TeamKind kind;
	char * name;
	MemberList * members;
};

/**
 * arena (<width>, <height>)
 *
 * The size of the battlefield, which is what gives every "at"/"around"
 * position its bounds. Checking that there is at most one, and that every
 * position fits inside it, is a semantic-analysis concern (Stage III).
 */
struct ArenaDeclaration {
	Expression * width;
	Expression * height;
};

/**
 * battle: [<teamName>, ...]
 *
 * A list of 2+ participating team names (any number, not just 2). Checking
 * that there are at least 2, that they're all declared, and that no unit
 * appears in more than one, is a semantic-analysis concern (Stage III).
 */
struct BattleDeclaration {
	IdentifierList * teams;
};

struct Declaration {
	union {
		AbilityDeclaration * abilityDeclaration;
		ArenaDeclaration * arenaDeclaration;
		BattleDeclaration * battleDeclaration;
		EffectDeclaration * effectDeclaration;
		TeamDeclaration * teamDeclaration;
		TurnDeclaration * turnDeclaration;
		UnitDeclaration * unitDeclaration;
	};
	DeclarationType type;
};

struct DeclarationList {
	Declaration * declaration;
	DeclarationList * next;
};

struct Program {
	DeclarationList * declarations;
};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyAbilityDeclaration(AbilityDeclaration * abilityDeclaration);
void destroyApplyStatement(ApplyStatement * applyStatement);
void destroyArenaDeclaration(ArenaDeclaration * arenaDeclaration);
void destroyAttribute(Attribute * attribute);
void destroyAttributeList(AttributeList * attributeList);
void destroyBattleDeclaration(BattleDeclaration * battleDeclaration);
void destroyCallSuffix(const CallSuffix callSuffix);
void destroyDealStatement(DealStatement * dealStatement);
void destroyDeclaration(Declaration * declaration);
void destroyDeclarationList(DeclarationList * declarationList);
void destroyEffectDeclaration(EffectDeclaration * effectDeclaration);
void destroyExpression(Expression * expression);
void destroyExpressionList(ExpressionList * expressionList);
void destroyForStatement(ForStatement * forStatement);
void destroyHealStatement(HealStatement * healStatement);
void destroyIdentifierList(IdentifierList * identifierList);
void destroyIfStatement(IfStatement * ifStatement);
void destroyLogStatement(LogStatement * logStatement);
void destroyMember(Member * member);
void destroyMemberList(MemberList * memberList);
void destroyMoveStatement(MoveStatement * moveStatement);
void destroyPosition(Position * position);
void destroyProgram(Program * program);
void destroyStatement(Statement * statement);
void destroyStatementList(StatementList * statementList);
void destroyTeamDeclaration(TeamDeclaration * teamDeclaration);
void destroyTurnDeclaration(TurnDeclaration * turnDeclaration);
void destroyUnitDeclaration(UnitDeclaration * unitDeclaration);
void destroyUseStatement(UseStatement * useStatement);
void destroyWhileStatement(WhileStatement * whileStatement);

#endif
