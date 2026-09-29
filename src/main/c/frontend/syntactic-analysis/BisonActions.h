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
 */

/** Program / declarations. */
Program * ProgramSemanticAction(DeclarationList * declarations);
DeclarationList * AddDeclarationSemanticAction(Declaration * declaration, DeclarationList * next);
Declaration * UnitDeclarationSemanticAction(UnitDeclaration * unitDeclaration);
Declaration * AbilityDeclarationSemanticAction(AbilityDeclaration * abilityDeclaration);
Declaration * TurnDeclarationSemanticAction(TurnDeclaration * turnDeclaration);
Declaration * TeamDeclarationSemanticAction(TeamDeclaration * teamDeclaration);
Declaration * BattleDeclarationSemanticAction(BattleDeclaration * battleDeclaration);

/** unit ... */
UnitDeclaration * UnitSemanticAction(char * name, Position * position, AttributeList * attributes, IdentifierList * abilities);
Position * PositionSemanticAction(Expression * x, Expression * y);
AttributeList * AddAttributeSemanticAction(Attribute * attribute, AttributeList * next);
Attribute * AttributeSemanticAction(char * name, Expression * value);
IdentifierList * AddIdentifierSemanticAction(char * identifier, IdentifierList * next);

/** ability / on turn / party|encounter / battle. */
AbilityDeclaration * AbilitySemanticAction(char * name, char * targetParameter, IdentifierList * targetTypes, StatementList * body);
TurnDeclaration * TurnSemanticAction(char * unitName, StatementList * body);
TeamDeclaration * TeamSemanticAction(TeamKind kind, char * name, IdentifierList * members);
BattleDeclaration * BattleSemanticAction(IdentifierList * teams);

/** Statements. */
StatementList * AddStatementSemanticAction(Statement * statement, StatementList * next);
Statement * DealStatementSemanticAction(Expression * amount, Expression * target);
Statement * HealStatementSemanticAction(Expression * amount, Expression * target);
Statement * UseStatementSemanticAction(char * abilityName, Expression * target);
Statement * ApplyStatementSemanticAction(char * effectName, Expression * target, Expression * duration);
Statement * LogStatementSemanticAction(char * message);
Statement * IfStatementSemanticAction(Expression * condition, StatementList * thenBranch, StatementList * elseBranch);
Statement * ForStatementSemanticAction(char * iterator, Expression * collection, StatementList * body);
Statement * WhileStatementSemanticAction(Expression * condition, StatementList * body);

/** Expressions. */
Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type);
Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type);
Expression * MemberExpressionSemanticAction(Expression * object, char * member);
Expression * CallExpressionSemanticAction(char * functionName, ExpressionList * arguments);
Expression * DiceExpressionSemanticAction(const DiceValue dice);
Expression * IntegerExpressionSemanticAction(const int value);
Expression * BooleanExpressionSemanticAction(const bool value);
Expression * StringExpressionSemanticAction(char * text);
Expression * IdentifierExpressionSemanticAction(char * text);
ExpressionList * AddExpressionSemanticAction(Expression * expression, ExpressionList * next);

#endif
