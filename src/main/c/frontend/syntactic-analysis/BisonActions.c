#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* IMPORTED FUNCTIONS */

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

/* PUBLIC FUNCTIONS */

/**
 * Reports the source line, but not the column: this project doesn't track
 * columns yet (see the comment on "createToken" in Frontend.c), and
 * printing an always-zero column would be misleading.
 */
void SyntacticErrorSemanticAction(const YYLTYPE * location, const char * message) {
	logError(_logger, "%s (line %d).", message, location->first_line);
}

/** Program / declarations. */

Program * ProgramSemanticAction(DeclarationList * declarations) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->declarations = declarations;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

DeclarationList * AddDeclarationSemanticAction(Declaration * declaration, DeclarationList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeclarationList * declarationList = calloc(1, sizeof(DeclarationList));
	declarationList->declaration = declaration;
	declarationList->next = next;
	return declarationList;
}

Declaration * UnitDeclarationSemanticAction(UnitDeclaration * unitDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->unitDeclaration = unitDeclaration;
	declaration->type = UNIT_DECLARATION;
	return declaration;
}

Declaration * AbilityDeclarationSemanticAction(AbilityDeclaration * abilityDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->abilityDeclaration = abilityDeclaration;
	declaration->type = ABILITY_DECLARATION;
	return declaration;
}

Declaration * TurnDeclarationSemanticAction(TurnDeclaration * turnDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->turnDeclaration = turnDeclaration;
	declaration->type = TURN_DECLARATION;
	return declaration;
}

Declaration * TeamDeclarationSemanticAction(TeamDeclaration * teamDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->teamDeclaration = teamDeclaration;
	declaration->type = TEAM_DECLARATION;
	return declaration;
}

Declaration * BattleDeclarationSemanticAction(BattleDeclaration * battleDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->battleDeclaration = battleDeclaration;
	declaration->type = BATTLE_DECLARATION;
	return declaration;
}

/** unit ... */

UnitDeclaration * UnitSemanticAction(char * name, Position * position, AttributeList * attributes, IdentifierList * abilities) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	UnitDeclaration * unitDeclaration = calloc(1, sizeof(UnitDeclaration));
	unitDeclaration->name = name;
	unitDeclaration->position = position;
	unitDeclaration->attributes = attributes;
	unitDeclaration->abilities = abilities;
	return unitDeclaration;
}

Position * PositionSemanticAction(Expression * x, Expression * y) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Position * position = calloc(1, sizeof(Position));
	position->x = x;
	position->y = y;
	return position;
}

AttributeList * AddAttributeSemanticAction(Attribute * attribute, AttributeList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	AttributeList * attributeList = calloc(1, sizeof(AttributeList));
	attributeList->attribute = attribute;
	attributeList->next = next;
	return attributeList;
}

Attribute * AttributeSemanticAction(char * name, Expression * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Attribute * attribute = calloc(1, sizeof(Attribute));
	attribute->name = name;
	attribute->value = value;
	return attribute;
}

IdentifierList * AddIdentifierSemanticAction(char * identifier, IdentifierList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	IdentifierList * identifierList = calloc(1, sizeof(IdentifierList));
	identifierList->identifier = identifier;
	identifierList->next = next;
	return identifierList;
}

/** ability / on turn / party|encounter / battle. */

AbilityDeclaration * AbilitySemanticAction(char * name, char * targetParameter, IdentifierList * targetTypes, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	AbilityDeclaration * abilityDeclaration = calloc(1, sizeof(AbilityDeclaration));
	abilityDeclaration->name = name;
	abilityDeclaration->targetParameter = targetParameter;
	abilityDeclaration->targetTypes = targetTypes;
	abilityDeclaration->body = body;
	return abilityDeclaration;
}

TurnDeclaration * TurnSemanticAction(char * unitName, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TurnDeclaration * turnDeclaration = calloc(1, sizeof(TurnDeclaration));
	turnDeclaration->unitName = unitName;
	turnDeclaration->body = body;
	return turnDeclaration;
}

TeamDeclaration * TeamSemanticAction(TeamKind kind, char * name, MemberList * members) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TeamDeclaration * teamDeclaration = calloc(1, sizeof(TeamDeclaration));
	teamDeclaration->kind = kind;
	teamDeclaration->name = name;
	teamDeclaration->members = members;
	return teamDeclaration;
}

BattleDeclaration * BattleSemanticAction(IdentifierList * teams) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	BattleDeclaration * battleDeclaration = calloc(1, sizeof(BattleDeclaration));
	battleDeclaration->teams = teams;
	return battleDeclaration;
}

MemberList * AddMemberSemanticAction(Member * member, MemberList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	MemberList * memberList = calloc(1, sizeof(MemberList));
	memberList->member = member;
	memberList->next = next;
	return memberList;
}

Member * MemberSemanticAction(char * unitName, Expression * quantity) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Member * member = calloc(1, sizeof(Member));
	member->unitName = unitName;
	member->quantity = quantity;
	return member;
}

/** Statements. */

StatementList * AddStatementSemanticAction(Statement * statement, StatementList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StatementList * statementList = calloc(1, sizeof(StatementList));
	statementList->statement = statement;
	statementList->next = next;
	return statementList;
}

Statement * DealStatementSemanticAction(Expression * amount, Expression * target) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DealStatement * dealStatement = calloc(1, sizeof(DealStatement));
	dealStatement->amount = amount;
	dealStatement->target = target;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->dealStatement = dealStatement;
	statement->type = DEAL_STATEMENT;
	return statement;
}

Statement * HealStatementSemanticAction(Expression * amount, Expression * target) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	HealStatement * healStatement = calloc(1, sizeof(HealStatement));
	healStatement->amount = amount;
	healStatement->target = target;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->healStatement = healStatement;
	statement->type = HEAL_STATEMENT;
	return statement;
}

Statement * UseStatementSemanticAction(char * abilityName, Expression * target) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	UseStatement * useStatement = calloc(1, sizeof(UseStatement));
	useStatement->abilityName = abilityName;
	useStatement->target = target;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->useStatement = useStatement;
	statement->type = USE_STATEMENT;
	return statement;
}

Statement * ApplyStatementSemanticAction(char * effectName, Expression * target, Expression * duration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ApplyStatement * applyStatement = calloc(1, sizeof(ApplyStatement));
	applyStatement->effectName = effectName;
	applyStatement->target = target;
	applyStatement->duration = duration;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->applyStatement = applyStatement;
	statement->type = APPLY_STATEMENT;
	return statement;
}

Statement * LogStatementSemanticAction(char * message) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LogStatement * logStatement = calloc(1, sizeof(LogStatement));
	logStatement->message = message;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->logStatement = logStatement;
	statement->type = LOG_STATEMENT;
	return statement;
}

Statement * IfStatementSemanticAction(Expression * condition, StatementList * thenBranch, StatementList * elseBranch) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	IfStatement * ifStatement = calloc(1, sizeof(IfStatement));
	ifStatement->condition = condition;
	ifStatement->thenBranch = thenBranch;
	ifStatement->elseBranch = elseBranch;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->ifStatement = ifStatement;
	statement->type = IF_STATEMENT;
	return statement;
}

Statement * ForStatementSemanticAction(char * iterator, Expression * collection, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ForStatement * forStatement = calloc(1, sizeof(ForStatement));
	forStatement->iterator = iterator;
	forStatement->collection = collection;
	forStatement->body = body;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->forStatement = forStatement;
	statement->type = FOR_STATEMENT;
	return statement;
}

Statement * WhileStatementSemanticAction(Expression * condition, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	WhileStatement * whileStatement = calloc(1, sizeof(WhileStatement));
	whileStatement->condition = condition;
	whileStatement->body = body;
	Statement * statement = calloc(1, sizeof(Statement));
	statement->whileStatement = whileStatement;
	statement->type = WHILE_STATEMENT;
	return statement;
}

/** Expressions. */

Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->leftExpression = leftExpression;
	expression->rightExpression = rightExpression;
	expression->type = type;
	return expression;
}

Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->operand = operand;
	expression->type = type;
	return expression;
}

Expression * MemberExpressionSemanticAction(Expression * object, char * member) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->object = object;
	expression->member = member;
	expression->type = MEMBER_EXPRESSION;
	return expression;
}

Expression * CallExpressionSemanticAction(char * functionName, ExpressionList * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->functionName = functionName;
	expression->arguments = arguments;
	expression->type = CALL_EXPRESSION;
	return expression;
}

Expression * RadiusExpressionSemanticAction(Expression * radius, Expression * center) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->radius = radius;
	expression->center = center;
	expression->type = RADIUS_EXPRESSION;
	return expression;
}

Expression * DiceExpressionSemanticAction(const DiceValue dice) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->dice = dice;
	expression->type = DICE_EXPRESSION;
	return expression;
}

Expression * IntegerExpressionSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->integerValue = value;
	expression->type = INTEGER_EXPRESSION;
	return expression;
}

Expression * BooleanExpressionSemanticAction(const bool value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->booleanValue = value;
	expression->type = BOOLEAN_EXPRESSION;
	return expression;
}

Expression * StringExpressionSemanticAction(char * text) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = text;
	expression->type = STRING_EXPRESSION;
	return expression;
}

Expression * IdentifierExpressionSemanticAction(char * text) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = text;
	expression->type = IDENTIFIER_EXPRESSION;
	return expression;
}

ExpressionList * AddExpressionSemanticAction(Expression * expression, ExpressionList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ExpressionList * expressionList = calloc(1, sizeof(ExpressionList));
	expressionList->expression = expression;
	expressionList->next = next;
	return expressionList;
}
