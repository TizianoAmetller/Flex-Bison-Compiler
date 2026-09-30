#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PUBLIC FUNCTIONS */

void destroyIdentifierList(IdentifierList * identifierList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (identifierList != NULL) {
		destroyIdentifierList(identifierList->next);
		free(identifierList->identifier);
		free(identifierList);
	}
}

void destroyCallSuffix(const CallSuffix callSuffix) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	destroyExpressionList(callSuffix.arguments);
}

void destroyExpressionList(ExpressionList * expressionList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (expressionList != NULL) {
		destroyExpressionList(expressionList->next);
		destroyExpression(expressionList->expression);
		free(expressionList);
	}
}

void destroyPosition(Position * position) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (position != NULL) {
		destroyExpression(position->x);
		destroyExpression(position->y);
		free(position);
	}
}

void destroyAttribute(Attribute * attribute) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (attribute != NULL) {
		free(attribute->name);
		destroyExpression(attribute->value);
		free(attribute);
	}
}

void destroyAttributeList(AttributeList * attributeList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (attributeList != NULL) {
		destroyAttributeList(attributeList->next);
		destroyAttribute(attributeList->attribute);
		free(attributeList);
	}
}

void destroyExpression(Expression * expression) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (expression != NULL) {
		switch (expression->type) {
			case ADD_EXPRESSION:
			case AND_EXPRESSION:
			case DIV_EXPRESSION:
			case EQUALS_EXPRESSION:
			case GREATER_EQUAL_EXPRESSION:
			case GREATER_THAN_EXPRESSION:
			case LESS_EQUAL_EXPRESSION:
			case LESS_THAN_EXPRESSION:
			case MUL_EXPRESSION:
			case NOT_EQUALS_EXPRESSION:
			case OR_EXPRESSION:
			case SUB_EXPRESSION:
				destroyExpression(expression->leftExpression);
				destroyExpression(expression->rightExpression);
				break;
			case NOT_EXPRESSION:
				destroyExpression(expression->operand);
				break;
			case MEMBER_EXPRESSION:
				destroyExpression(expression->object);
				free(expression->member);
				break;
			case CALL_EXPRESSION:
				free(expression->functionName);
				destroyExpressionList(expression->arguments);
				break;
			case RADIUS_EXPRESSION:
				destroyExpression(expression->radius);
				destroyExpression(expression->center);
				break;
			case BOOLEAN_EXPRESSION:
			case DICE_EXPRESSION:
			case INTEGER_EXPRESSION:
				break;
			case IDENTIFIER_EXPRESSION:
			case STRING_EXPRESSION:
				free(expression->text);
				break;
		}
		free(expression);
	}
}

void destroyDealStatement(DealStatement * dealStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (dealStatement != NULL) {
		destroyExpression(dealStatement->amount);
		destroyExpression(dealStatement->target);
		free(dealStatement);
	}
}

void destroyHealStatement(HealStatement * healStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (healStatement != NULL) {
		destroyExpression(healStatement->amount);
		destroyExpression(healStatement->target);
		free(healStatement);
	}
}

void destroyUseStatement(UseStatement * useStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (useStatement != NULL) {
		free(useStatement->abilityName);
		destroyExpression(useStatement->target);
		free(useStatement);
	}
}

void destroyApplyStatement(ApplyStatement * applyStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (applyStatement != NULL) {
		free(applyStatement->effectName);
		destroyExpression(applyStatement->target);
		destroyExpression(applyStatement->duration);
		free(applyStatement);
	}
}

void destroyLogStatement(LogStatement * logStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (logStatement != NULL) {
		free(logStatement->message);
		free(logStatement);
	}
}

void destroyIfStatement(IfStatement * ifStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (ifStatement != NULL) {
		destroyExpression(ifStatement->condition);
		destroyStatementList(ifStatement->thenBranch);
		destroyStatementList(ifStatement->elseBranch);
		free(ifStatement);
	}
}

void destroyForStatement(ForStatement * forStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (forStatement != NULL) {
		free(forStatement->iterator);
		destroyExpression(forStatement->collection);
		destroyStatementList(forStatement->body);
		free(forStatement);
	}
}

void destroyWhileStatement(WhileStatement * whileStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (whileStatement != NULL) {
		destroyExpression(whileStatement->condition);
		destroyStatementList(whileStatement->body);
		free(whileStatement);
	}
}

void destroyStatement(Statement * statement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (statement != NULL) {
		switch (statement->type) {
			case APPLY_STATEMENT: destroyApplyStatement(statement->applyStatement); break;
			case DEAL_STATEMENT: destroyDealStatement(statement->dealStatement); break;
			case FOR_STATEMENT: destroyForStatement(statement->forStatement); break;
			case HEAL_STATEMENT: destroyHealStatement(statement->healStatement); break;
			case IF_STATEMENT: destroyIfStatement(statement->ifStatement); break;
			case LOG_STATEMENT: destroyLogStatement(statement->logStatement); break;
			case USE_STATEMENT: destroyUseStatement(statement->useStatement); break;
			case WHILE_STATEMENT: destroyWhileStatement(statement->whileStatement); break;
		}
		free(statement);
	}
}

void destroyStatementList(StatementList * statementList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (statementList != NULL) {
		destroyStatementList(statementList->next);
		destroyStatement(statementList->statement);
		free(statementList);
	}
}

void destroyUnitDeclaration(UnitDeclaration * unitDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (unitDeclaration != NULL) {
		free(unitDeclaration->name);
		destroyPosition(unitDeclaration->position);
		destroyAttributeList(unitDeclaration->attributes);
		destroyIdentifierList(unitDeclaration->abilities);
		free(unitDeclaration);
	}
}

void destroyAbilityDeclaration(AbilityDeclaration * abilityDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (abilityDeclaration != NULL) {
		free(abilityDeclaration->name);
		free(abilityDeclaration->targetParameter);
		destroyIdentifierList(abilityDeclaration->targetTypes);
		destroyStatementList(abilityDeclaration->body);
		free(abilityDeclaration);
	}
}

void destroyTurnDeclaration(TurnDeclaration * turnDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (turnDeclaration != NULL) {
		free(turnDeclaration->unitName);
		destroyStatementList(turnDeclaration->body);
		free(turnDeclaration);
	}
}

void destroyMember(Member * member) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (member != NULL) {
		free(member->unitName);
		destroyExpression(member->quantity);
		free(member);
	}
}

void destroyMemberList(MemberList * memberList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (memberList != NULL) {
		destroyMemberList(memberList->next);
		destroyMember(memberList->member);
		free(memberList);
	}
}

void destroyTeamDeclaration(TeamDeclaration * teamDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (teamDeclaration != NULL) {
		free(teamDeclaration->name);
		destroyMemberList(teamDeclaration->members);
		free(teamDeclaration);
	}
}

void destroyBattleDeclaration(BattleDeclaration * battleDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (battleDeclaration != NULL) {
		destroyIdentifierList(battleDeclaration->teams);
		free(battleDeclaration);
	}
}

void destroyDeclaration(Declaration * declaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (declaration != NULL) {
		switch (declaration->type) {
			case ABILITY_DECLARATION: destroyAbilityDeclaration(declaration->abilityDeclaration); break;
			case BATTLE_DECLARATION: destroyBattleDeclaration(declaration->battleDeclaration); break;
			case TEAM_DECLARATION: destroyTeamDeclaration(declaration->teamDeclaration); break;
			case TURN_DECLARATION: destroyTurnDeclaration(declaration->turnDeclaration); break;
			case UNIT_DECLARATION: destroyUnitDeclaration(declaration->unitDeclaration); break;
		}
		free(declaration);
	}
}

void destroyDeclarationList(DeclarationList * declarationList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (declarationList != NULL) {
		destroyDeclarationList(declarationList->next);
		destroyDeclaration(declarationList->declaration);
		free(declarationList);
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroyDeclarationList(program->declarations);
		free(program);
	}
}
