#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState);

/** Reports a syntax error, with its source location, through this module's logger. */
void SyntacticErrorSemanticAction(const YYLTYPE * location, const char * message);

/**
 * Bison semantic actions.
 *
 * The ones that create a node with a source line (Declaration, Statement,
 * Expression, IdentifierList, Member and Attribute) take that "line" as their
 * first parameter: the 1-based line of the first token of the construct.
 */

/** Program / declarations. */
Program * ProgramSemanticAction(DeclarationList * declarations);
DeclarationList * AddDeclarationSemanticAction(Declaration * declaration, DeclarationList * next);
Declaration * UnitDeclarationSemanticAction(const int line, UnitDeclaration * unitDeclaration);
Declaration * AbilityDeclarationSemanticAction(const int line, AbilityDeclaration * abilityDeclaration);
Declaration * TurnDeclarationSemanticAction(const int line, TurnDeclaration * turnDeclaration);
Declaration * TeamDeclarationSemanticAction(const int line, TeamDeclaration * teamDeclaration);
Declaration * ArenaDeclarationSemanticAction(const int line, ArenaDeclaration * arenaDeclaration);
Declaration * BattleDeclarationSemanticAction(const int line, BattleDeclaration * battleDeclaration);
Declaration * EffectDeclarationSemanticAction(const int line, EffectDeclaration * effectDeclaration);

/** unit ... */
UnitDeclaration * UnitSemanticAction(char * name, IdentifierList * tags, Position * position, AttributeList * attributes, IdentifierList * abilities);
IdentifierList * TagClauseSemanticAction(IdentifierList * tags);
Position * PositionSemanticAction(Expression * x, Expression * y);
AttributeList * AddAttributeSemanticAction(Attribute * attribute, AttributeList * next);
Attribute * AttributeSemanticAction(const int line, char * name, Expression * value);
IdentifierList * AddIdentifierSemanticAction(const int line, char * identifier, IdentifierList * next);
IdentifierList * AbilitiesClauseSemanticAction(IdentifierList * abilities);

/** ability / effect / on turn / party|encounter / arena / battle. */
AbilityDeclaration * AbilitySemanticAction(char * name, char * targetParameter, IdentifierList * targetTypes, StatementList * body);
IdentifierList * TargetTypeClauseSemanticAction(IdentifierList * targetTypes);
EffectDeclaration * EffectSemanticAction(char * name, char * targetParameter, StatementList * body);
TurnDeclaration * TurnSemanticAction(char * unitName, StatementList * body);
TeamDeclaration * TeamSemanticAction(TeamKind kind, char * name, MemberList * members);
ArenaDeclaration * ArenaSemanticAction(Expression * width, Expression * height);
BattleDeclaration * BattleSemanticAction(IdentifierList * teams);
MemberList * AddMemberSemanticAction(Member * member, MemberList * next);
Member * MemberSemanticAction(const int line, char * unitName, Expression * quantity, Position * position);

/** Statements. */
StatementList * AddStatementSemanticAction(Statement * statement, StatementList * next);
StatementList * BlockSemanticAction(StatementList * statements);
Statement * DealStatementSemanticAction(const int line, Expression * amount, Expression * target);
Statement * HealStatementSemanticAction(const int line, Expression * amount, Expression * target);
Statement * UseStatementSemanticAction(const int line, char * abilityName, Expression * target);
Statement * ApplyStatementSemanticAction(const int line, char * effectName, Expression * target, Expression * duration);
Statement * LogStatementSemanticAction(const int line, char * message);
Statement * MoveStatementSemanticAction(const int line, MoveDirection direction, Expression * target);
Statement * IfStatementSemanticAction(const int line, Expression * condition, StatementList * thenBranch, StatementList * elseBranch);
Statement * ForStatementSemanticAction(const int line, char * iterator, Expression * collection, StatementList * body);
Statement * WhileStatementSemanticAction(const int line, Expression * condition, StatementList * body);

/**
 * Expressions. The ones that build a node out of other expressions (every one but the
 * literals, the names and the parentheses) return NULL, after reporting the error
 * and releasing what they were given, if the expression is too deep.
 */
Expression * BinaryExpressionSemanticAction(const int line, Expression * leftExpression, Expression * rightExpression, ExpressionType type);
Expression * UnaryExpressionSemanticAction(const int line, Expression * operand, ExpressionType type);
Expression * MemberExpressionSemanticAction(const int line, Expression * object, char * member);
Expression * CallExpressionSemanticAction(const int line, char * functionName, ExpressionList * arguments);
Expression * IdOrCallExpressionSemanticAction(const int line, char * identifier, const CallSuffix callSuffix);
CallSuffix CallSuffixSemanticAction(ExpressionList * arguments);
CallSuffix NoCallSuffixSemanticAction(void);
Expression * ParenthesizedExpressionSemanticAction(Expression * expression);
Expression * RadiusExpressionSemanticAction(const int line, Expression * radius, Expression * center);
Expression * DiceExpressionSemanticAction(const int line, const DiceValue dice);
Expression * IntegerExpressionSemanticAction(const int line, const int value);
Expression * BooleanExpressionSemanticAction(const int line, const bool value);
Expression * StringExpressionSemanticAction(const int line, char * text);
Expression * IdentifierExpressionSemanticAction(const int line, char * text);
ExpressionList * AddExpressionSemanticAction(Expression * expression, ExpressionList * next);

#endif
