#include "SemanticAnalyzer.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MODULE INTERNAL STATE */

/** The number of errors reported before the rest are only counted. */
#define MAX_ERRORS 100

/** The size of the buffers that hold a message, and the context it is about. */
#define MESSAGE_SIZE 512
#define CONTEXT_SIZE 256

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static Logger * _logger = NULL;
static int _errorCount = 0;
static bool _errorsSuppressed = false;
static bool _outOfMemory = false;

/** Shutdown module's internal state. */
static void _shutdownSemanticAnalyzerModule(void) {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSemanticAnalyzerModule(void) {
	_logger = createLogger("SemanticAnalyzer");
	_errorCount = 0;
	_errorsSuppressed = false;
	_outOfMemory = false;
	return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE TYPES AND CONSTANTS */

/**
 * The value of a constant expression. When "valid" is false an error was
 * already reported for it (so nothing else is said about it). When "hasDice" is
 * true some part of it is a dice roll, so it has no value to check yet.
 */
typedef struct {
	bool valid;
	bool hasDice;
	int64_t value;
} ConstantValue;

/** The size of the arena, when it is declared and valid. */
typedef struct {
	bool known;
	int64_t width;
	int64_t height;
} ArenaBounds;

/** The team of a battle that a unit was found in. */
typedef struct {
	const Declaration * unit;
	const char * team;
} UnitOwner;

/** Names that the language uses itself, so no declaration can take them. */
static const char * const RESERVED_NAMES[] = { "self", "allies", "enemies", "ally", "enemy", "nearest", "all" };

/** The attributes every unit declares exactly once. */
static const char * const UNIT_ATTRIBUTES[] = { "health", "attack", "defense", "speed" };

/** The types of target that need no declaration, next to the tags. */
static const char * const RELATIONAL_TYPES[] = { "ally", "enemy", "self" };

/* PRIVATE FUNCTIONS */

static void _semanticError(const int line, const char * format, ...) __attribute__((format(printf, 2, 3)));
static int _indexOf(const char * name, const char * const * names, const size_t count);
static bool _isRepeated(const IdentifierList * list, const IdentifierList * item);
static bool _checkReservedName(const int line, const char * name, const char * kind);
static void _collectTags(const UnitDeclaration * unit, SemanticModel * semanticModel);
static void _collectDeclarations(const Program * program, SemanticModel * semanticModel);
static ConstantValue _evaluateConstant(const Expression * expression, const char * context);
static ConstantValue _evaluateArithmetic(const Expression * expression, const char * context);
static bool _validateArenaSide(const Expression * expression, const char * side, int64_t * value);
static ArenaBounds _validateArena(const SemanticModel * semanticModel);
static void _validateCoordinate(const Expression * expression, const ConstantValue value, const char axis, const int64_t limit, const char * context);
static void _validatePosition(const Position * position, const char * context, const ArenaBounds * arena);
static void _validateUnit(const Declaration * declaration, const SemanticModel * semanticModel, const ArenaBounds * arena);
static void _validateAbility(const Declaration * declaration, const SemanticModel * semanticModel);
static void _validateTurn(const Declaration * declaration, const SemanticModel * semanticModel);
static void _validateTeam(const Declaration * declaration, const SemanticModel * semanticModel, const ArenaBounds * arena);
static void _validateBattleUnits(const BattleDeclaration * battle, const SemanticModel * semanticModel);
static void _validateBattle(const Declaration * declaration, const SemanticModel * semanticModel);
static void _validateDeclarations(const Program * program, const SemanticModel * semanticModel);

/**
 * Reports a semantic error, as "line N: <message>", and counts it. Only the
 * first MAX_ERRORS are logged, to keep the output bounded.
 */
static void _semanticError(const int line, const char * format, ...) {
	if (MAX_ERRORS <= _errorCount) {
		_errorsSuppressed = true;
		return;
	}
	char message[MESSAGE_SIZE];
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);
	logError(_logger, "line %d: %s", line, message);
	++_errorCount;
}

/**
 * The position of the name in the list of names, or -1 when it is not there.
 */
static int _indexOf(const char * name, const char * const * names, const size_t count) {
	for (size_t k = 0; k < count; ++k) {
		if (strcmp(name, names[k]) == 0) {
			return (int) k;
		}
	}
	return -1;
}

/**
 * Whether some identifier before the item, in the same list, is the same.
 */
static bool _isRepeated(const IdentifierList * list, const IdentifierList * item) {
	for (const IdentifierList * previous = list; previous != item; previous = previous->next) {
		if (strcmp(previous->identifier, item->identifier) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * Reports the name if it is reserved. The kind is what the name was given to,
 * with its article (for example, "a unit" or "an ability").
 */
static bool _checkReservedName(const int line, const char * name, const char * kind) {
	const bool reserved = (0 <= _indexOf(name, RESERVED_NAMES, COUNT_OF(RESERVED_NAMES)));
	if (reserved) {
		_semanticError(line, "'%s' is a reserved name and cannot be used as %s name", name, kind);
	}
	return reserved;
}

/**
 * Adds the tags of a unit to the model, reporting the reserved and repeated ones.
 */
static void _collectTags(const UnitDeclaration * unit, SemanticModel * semanticModel) {
	for (const IdentifierList * tag = unit->tags; tag != NULL; tag = tag->next) {
		const bool reserved = _checkReservedName(tag->line, tag->identifier, "a tag");
		if (_isRepeated(unit->tags, tag)) {
			_semanticError(tag->line, "tag '%s' is repeated in unit '%s'", tag->identifier, unit->name);
		}
		else if (!reserved && !addTag(semanticModel, tag->identifier)) {
			_outOfMemory = true;
		}
	}
}

/**
 * First pass: adds every declaration to the model, and reports the reserved
 * names and the declarations that repeat a name (or that should be unique).
 * The declarations can come in any order, so nothing else can be checked yet.
 */
static void _collectDeclarations(const Program * program, SemanticModel * semanticModel) {
	for (const DeclarationList * item = program->declarations; item != NULL; item = item->next) {
		const Declaration * declaration = item->declaration;
		const Declaration * first = NULL;
		switch (declaration->type) {
			case UNIT_DECLARATION:
				_checkReservedName(declaration->line, declaration->unitDeclaration->name, "a unit");
				first = findUnit(semanticModel, declaration->unitDeclaration->name);
				if (first != NULL) {
					_semanticError(declaration->line, "unit '%s' is already declared (first declared at line %d)", declaration->unitDeclaration->name, first->line);
				}
				else if (!addUnit(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				_collectTags(declaration->unitDeclaration, semanticModel);
				break;
			case ABILITY_DECLARATION:
				_checkReservedName(declaration->line, declaration->abilityDeclaration->name, "an ability");
				first = findAbility(semanticModel, declaration->abilityDeclaration->name);
				if (first != NULL) {
					_semanticError(declaration->line, "ability '%s' is already declared (first declared at line %d)", declaration->abilityDeclaration->name, first->line);
				}
				else if (!addAbility(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				break;
			case EFFECT_DECLARATION:
				_checkReservedName(declaration->line, declaration->effectDeclaration->name, "an effect");
				first = findEffect(semanticModel, declaration->effectDeclaration->name);
				if (first != NULL) {
					_semanticError(declaration->line, "effect '%s' is already declared (first declared at line %d)", declaration->effectDeclaration->name, first->line);
				}
				else if (!addEffect(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				break;
			case TEAM_DECLARATION:
				_checkReservedName(declaration->line, declaration->teamDeclaration->name, "a team");
				first = findTeam(semanticModel, declaration->teamDeclaration->name);
				if (first != NULL) {
					_semanticError(declaration->line, "team '%s' is already declared (first declared at line %d)", declaration->teamDeclaration->name, first->line);
				}
				else if (!addTeam(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				break;
			case TURN_DECLARATION:
				if (!addTurn(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				break;
			case ARENA_DECLARATION:
				if (semanticModel->arena != NULL) {
					_semanticError(declaration->line, "arena is already declared (first declared at line %d)", semanticModel->arena->line);
				}
				else {
					semanticModel->arena = declaration;
				}
				break;
			case BATTLE_DECLARATION:
				if (semanticModel->battle != NULL) {
					_semanticError(declaration->line, "battle is already declared (first declared at line %d)", semanticModel->battle->line);
				}
				else {
					semanticModel->battle = declaration;
				}
				break;
		}
	}
}

/**
 * Evaluates an expression in a constant context: int literals, dice and
 * arithmetic only. The context says where the expression is (for example,
 * "attribute 'health' of unit 'Hero'"), and ends every message about it.
 */
static ConstantValue _evaluateConstant(const Expression * expression, const char * context) {
	ConstantValue result = { .valid = false, .hasDice = false, .value = 0 };
	switch (expression->type) {
		case INTEGER_EXPRESSION:
			result.valid = true;
			result.value = expression->integerValue;
			break;
		case DICE_EXPRESSION:
			result.valid = true;
			result.hasDice = true;
			break;
		case BOOLEAN_EXPRESSION:
			_semanticError(expression->line, "expected int, got bool (in %s)", context);
			break;
		case STRING_EXPRESSION:
			_semanticError(expression->line, "expected int, got string (in %s)", context);
			break;
		case ADD_EXPRESSION:
		case DIV_EXPRESSION:
		case MUL_EXPRESSION:
		case SUB_EXPRESSION:
			result = _evaluateArithmetic(expression, context);
			break;
		case AND_EXPRESSION:
		case CALL_EXPRESSION:
		case EQUALS_EXPRESSION:
		case GREATER_EQUAL_EXPRESSION:
		case GREATER_THAN_EXPRESSION:
		case IDENTIFIER_EXPRESSION:
		case LESS_EQUAL_EXPRESSION:
		case LESS_THAN_EXPRESSION:
		case MEMBER_EXPRESSION:
		case NOT_EQUALS_EXPRESSION:
		case NOT_EXPRESSION:
		case OR_EXPRESSION:
		case RADIUS_EXPRESSION:
			_semanticError(expression->line, "expression is not allowed here: only int literals, dice and arithmetic (in %s)", context);
			break;
	}
	return result;
}

/**
 * Evaluates "+", "-", "*" or "/" over constants, with 64 bits. Both operands
 * are always evaluated, so each error in them is reported. Nothing is computed
 * when a dice roll is involved, because there is no value to compute with.
 */
static ConstantValue _evaluateArithmetic(const Expression * expression, const char * context) {
	ConstantValue result = { .valid = false, .hasDice = false, .value = 0 };
	const ConstantValue left = _evaluateConstant(expression->leftExpression, context);
	const ConstantValue right = _evaluateConstant(expression->rightExpression, context);
	if (!left.valid || !right.valid) {
		return result;
	}
	if (expression->type == DIV_EXPRESSION && !right.hasDice && right.value == 0) {
		_semanticError(expression->line, "division by zero in constant expression (in %s)", context);
		return result;
	}
	result.valid = true;
	result.hasDice = left.hasDice || right.hasDice;
	if (result.hasDice) {
		return result;
	}
	bool overflow = false;
	switch (expression->type) {
		case ADD_EXPRESSION:
			overflow = __builtin_add_overflow(left.value, right.value, &result.value);
			break;
		case SUB_EXPRESSION:
			overflow = __builtin_sub_overflow(left.value, right.value, &result.value);
			break;
		case MUL_EXPRESSION:
			overflow = __builtin_mul_overflow(left.value, right.value, &result.value);
			break;
		default:
			if (left.value == INT64_MIN && right.value == -1) {
				overflow = true;
			}
			else {
				result.value = left.value / right.value;
			}
			break;
	}
	if (overflow) {
		_semanticError(expression->line, "constant expression overflows (in %s)", context);
		result.valid = false;
		result.value = 0;
	}
	return result;
}

/**
 * Checks one side of the arena: a constant, with no dice, greater than 0.
 */
static bool _validateArenaSide(const Expression * expression, const char * side, int64_t * value) {
	char context[CONTEXT_SIZE];
	snprintf(context, sizeof(context), "arena '%s'", side);
	const ConstantValue constant = _evaluateConstant(expression, context);
	if (!constant.valid) {
		return false;
	}
	if (constant.hasDice) {
		_semanticError(expression->line, "arena '%s' must not contain dice", side);
		return false;
	}
	if (constant.value <= 0) {
		_semanticError(expression->line, "arena '%s' must be greater than 0, got %lld", side, (long long) constant.value);
		return false;
	}
	*value = constant.value;
	return true;
}

/**
 * Validates the arena (the first, if there are several) and returns its size,
 * which the positions are checked against. It is not known when there is no
 * arena, or when its size has an error that was already reported.
 */
static ArenaBounds _validateArena(const SemanticModel * semanticModel) {
	ArenaBounds bounds = { .known = false, .width = 0, .height = 0 };
	if (semanticModel->arena != NULL) {
		const ArenaDeclaration * arena = semanticModel->arena->arenaDeclaration;
		const bool validWidth = _validateArenaSide(arena->width, "width", &bounds.width);
		const bool validHeight = _validateArenaSide(arena->height, "height", &bounds.height);
		bounds.known = validWidth && validHeight;
	}
	return bounds;
}

/**
 * Checks that a constant coordinate is inside the arena: 0 <= value < limit.
 */
static void _validateCoordinate(const Expression * expression, const ConstantValue value, const char axis, const int64_t limit, const char * context) {
	if (value.valid && !value.hasDice && (value.value < 0 || limit <= value.value)) {
		_semanticError(expression->line, "%s is outside the arena: %c is %lld, but it must satisfy 0 <= %c < %lld", context, axis, (long long) value.value, axis, (long long) limit);
	}
}

/**
 * Checks that both coordinates are constants and, if the arena is known, that
 * they are inside it.
 */
static void _validatePosition(const Position * position, const char * context, const ArenaBounds * arena) {
	const ConstantValue x = _evaluateConstant(position->x, context);
	const ConstantValue y = _evaluateConstant(position->y, context);
	if (arena->known) {
		_validateCoordinate(position->x, x, 'x', arena->width, context);
		_validateCoordinate(position->y, y, 'y', arena->height, context);
	}
}

/**
 * Checks the attributes, abilities and position of a unit. The tags were
 * already checked in the first pass.
 */
static void _validateUnit(const Declaration * declaration, const SemanticModel * semanticModel, const ArenaBounds * arena) {
	const UnitDeclaration * unit = declaration->unitDeclaration;
	char context[CONTEXT_SIZE];
	bool declared[COUNT_OF(UNIT_ATTRIBUTES)] = { false };
	for (const AttributeList * item = unit->attributes; item != NULL; item = item->next) {
		const Attribute * attribute = item->attribute;
		const int index = _indexOf(attribute->name, UNIT_ATTRIBUTES, COUNT_OF(UNIT_ATTRIBUTES));
		if (index < 0) {
			_semanticError(attribute->line, "unknown attribute '%s' in unit '%s'", attribute->name, unit->name);
			continue;
		}
		if (declared[index]) {
			_semanticError(attribute->line, "attribute '%s' is repeated in unit '%s'", attribute->name, unit->name);
		}
		declared[index] = true;
		snprintf(context, sizeof(context), "attribute '%s' of unit '%s'", attribute->name, unit->name);
		_evaluateConstant(attribute->value, context);
	}
	for (size_t k = 0; k < COUNT_OF(UNIT_ATTRIBUTES); ++k) {
		if (!declared[k]) {
			_semanticError(declaration->line, "unit '%s' is missing attribute '%s'", unit->name, UNIT_ATTRIBUTES[k]);
		}
	}
	for (const IdentifierList * ability = unit->abilities; ability != NULL; ability = ability->next) {
		if (findAbility(semanticModel, ability->identifier) == NULL) {
			_semanticError(ability->line, "unit '%s' references undeclared ability '%s'", unit->name, ability->identifier);
		}
		if (_isRepeated(unit->abilities, ability)) {
			_semanticError(ability->line, "ability '%s' is repeated in the abilities of unit '%s'", ability->identifier, unit->name);
		}
	}
	if (unit->position != NULL) {
		snprintf(context, sizeof(context), "position of unit '%s'", unit->name);
		_validatePosition(unit->position, context, arena);
	}
}

/**
 * Checks the types of target of an ability. Its parameter and its body are not
 * analyzed yet.
 */
static void _validateAbility(const Declaration * declaration, const SemanticModel * semanticModel) {
	const AbilityDeclaration * ability = declaration->abilityDeclaration;
	for (const IdentifierList * type = ability->targetTypes; type != NULL; type = type->next) {
		const bool relational = (0 <= _indexOf(type->identifier, RELATIONAL_TYPES, COUNT_OF(RELATIONAL_TYPES)));
		if (!relational && !hasTag(semanticModel, type->identifier)) {
			_semanticError(type->line, "unknown target type '%s' in ability '%s'", type->identifier, ability->name);
		}
		if (_isRepeated(ability->targetTypes, type)) {
			_semanticError(type->line, "target type '%s' is repeated in ability '%s'", type->identifier, ability->name);
		}
	}
}

/**
 * Checks that the unit of an "on turn" declaration exists, and that it is the
 * only one for that unit. The body is not analyzed yet.
 */
static void _validateTurn(const Declaration * declaration, const SemanticModel * semanticModel) {
	const TurnDeclaration * turn = declaration->turnDeclaration;
	if (findUnit(semanticModel, turn->unitName) == NULL) {
		_semanticError(declaration->line, "on turn references undeclared unit '%s'", turn->unitName);
	}
	const Declaration * first = findTurn(semanticModel, turn->unitName);
	if (first != NULL && first != declaration) {
		_semanticError(declaration->line, "unit '%s' already has an on turn declaration (first declared at line %d)", turn->unitName, first->line);
	}
}

/**
 * Checks the members of a team: their units, quantities and positions.
 */
static void _validateTeam(const Declaration * declaration, const SemanticModel * semanticModel, const ArenaBounds * arena) {
	const TeamDeclaration * team = declaration->teamDeclaration;
	char context[CONTEXT_SIZE];
	for (const MemberList * item = team->members; item != NULL; item = item->next) {
		const Member * member = item->member;
		if (findUnit(semanticModel, member->unitName) == NULL) {
			_semanticError(member->line, "team '%s' references undeclared unit '%s'", team->name, member->unitName);
		}
		for (const MemberList * previous = team->members; previous != item; previous = previous->next) {
			if (strcmp(previous->member->unitName, member->unitName) == 0) {
				_semanticError(member->line, "unit '%s' is repeated in team '%s'", member->unitName, team->name);
				break;
			}
		}
		if (member->quantity != NULL) {
			snprintf(context, sizeof(context), "quantity of '%s' in team '%s'", member->unitName, team->name);
			const ConstantValue quantity = _evaluateConstant(member->quantity, context);
			if (quantity.valid && !quantity.hasDice && quantity.value <= 0) {
				_semanticError(member->quantity->line, "quantity of '%s' in team '%s' must be greater than 0, got %lld", member->unitName, team->name, (long long) quantity.value);
			}
		}
		if (member->position != NULL) {
			snprintf(context, sizeof(context), "position of '%s' in team '%s'", member->unitName, team->name);
			_validatePosition(member->position, context, arena);
		}
	}
}

/**
 * Checks that no unit is in more than one of the teams of the battle. The
 * teams that don't exist, or that the battle repeats, are skipped: they were
 * already reported. A unit that a team repeats is not a second team.
 */
static void _validateBattleUnits(const BattleDeclaration * battle, const SemanticModel * semanticModel) {
	size_t capacity = 0;
	for (const IdentifierList * name = battle->teams; name != NULL; name = name->next) {
		const Declaration * team = findTeam(semanticModel, name->identifier);
		if (team != NULL && !_isRepeated(battle->teams, name)) {
			for (const MemberList * item = team->teamDeclaration->members; item != NULL; item = item->next) {
				++capacity;
			}
		}
	}
	UnitOwner * owners = (0 < capacity) ? malloc(capacity * sizeof(UnitOwner)) : NULL;
	if (0 < capacity && owners == NULL) {
		_outOfMemory = true;
		return;
	}
	size_t count = 0;
	for (const IdentifierList * name = battle->teams; name != NULL; name = name->next) {
		const Declaration * team = findTeam(semanticModel, name->identifier);
		if (team == NULL || _isRepeated(battle->teams, name)) {
			continue;
		}
		for (const MemberList * item = team->teamDeclaration->members; item != NULL; item = item->next) {
			const Member * member = item->member;
			const Declaration * unit = findUnit(semanticModel, member->unitName);
			if (unit == NULL) {
				continue;
			}
			const UnitOwner * owner = NULL;
			for (size_t k = 0; k < count && owner == NULL; ++k) {
				if (owners[k].unit == unit) {
					owner = &owners[k];
				}
			}
			if (owner == NULL) {
				owners[count].unit = unit;
				owners[count].team = team->teamDeclaration->name;
				++count;
			}
			else if (strcmp(owner->team, team->teamDeclaration->name) != 0) {
				_semanticError(member->line, "unit '%s' appears in more than one team of the battle: '%s' and '%s'", member->unitName, owner->team, team->teamDeclaration->name);
			}
		}
	}
	free(owners);
}

/**
 * Checks the battle: at least two teams, all of them declared and with no
 * repeats, and no unit in more than one of them.
 */
static void _validateBattle(const Declaration * declaration, const SemanticModel * semanticModel) {
	const BattleDeclaration * battle = declaration->battleDeclaration;
	int teamCount = 0;
	for (const IdentifierList * name = battle->teams; name != NULL; name = name->next) {
		++teamCount;
		if (findTeam(semanticModel, name->identifier) == NULL) {
			_semanticError(name->line, "battle references undeclared team '%s'", name->identifier);
		}
		if (_isRepeated(battle->teams, name)) {
			_semanticError(name->line, "team '%s' is repeated in the battle", name->identifier);
		}
	}
	if (teamCount < 2) {
		_semanticError(declaration->line, "battle must have at least 2 teams, got %d", teamCount);
	}
	_validateBattleUnits(battle, semanticModel);
}

/**
 * Second pass: validates the references between declarations and the rules of
 * each kind, now that every declaration is in the model. The ability, effect
 * and "on turn" bodies are left alone.
 */
static void _validateDeclarations(const Program * program, const SemanticModel * semanticModel) {
	const ArenaBounds arena = _validateArena(semanticModel);
	for (const DeclarationList * item = program->declarations; item != NULL; item = item->next) {
		const Declaration * declaration = item->declaration;
		switch (declaration->type) {
			case UNIT_DECLARATION:
				_validateUnit(declaration, semanticModel, &arena);
				break;
			case ABILITY_DECLARATION:
				_validateAbility(declaration, semanticModel);
				break;
			case TURN_DECLARATION:
				_validateTurn(declaration, semanticModel);
				break;
			case TEAM_DECLARATION:
				_validateTeam(declaration, semanticModel, &arena);
				break;
			case BATTLE_DECLARATION:
				if (declaration == semanticModel->battle) {
					_validateBattle(declaration, semanticModel);
				}
				break;
			case ARENA_DECLARATION:
			case EFFECT_DECLARATION:
				break;
		}
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	_errorCount = 0;
	_errorsSuppressed = false;
	_outOfMemory = false;
	logDebugging(_logger, "Analyzing...");
	SemanticModel * semanticModel = createSemanticModel();
	compilerState->semanticModel = semanticModel;
	if (semanticModel == NULL) {
		return OUT_OF_MEMORY;
	}
	const Program * program = compilerState->abstractSyntaxTree;
	_collectDeclarations(program, semanticModel);
	_validateDeclarations(program, semanticModel);
	if (_errorsSuppressed) {
		logError(_logger, "too many errors: only the first %d are reported", MAX_ERRORS);
	}
	if (_outOfMemory) {
		return OUT_OF_MEMORY;
	}
	if (0 < _errorCount) {
		return FAILED;
	}
	if (semanticModel->battle == NULL) {
		logWarning(_logger, "no battle declared: nothing to simulate");
	}
	return SUCCEEDED;
}
