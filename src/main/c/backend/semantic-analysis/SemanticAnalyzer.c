#include "SemanticAnalyzer.h"
#include "LogTemplate.h"
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
static const char * const RESERVED_NAMES[] = { "self", "allies", "enemies", "ally", "enemy", "nearest" };

/** The attributes every unit declares exactly once. */
static const char * const UNIT_ATTRIBUTES[] = { "health", "attack", "defense", "speed" };

/** The types of target that need no declaration, next to the tags. */
static const char * const RELATIONAL_TYPES[] = { "ally", "enemy", "self" };

/** The types of the values in the bodies, as the messages call them. */
typedef enum {
	/**
	 * The type of an expression that already has an error. It is compatible with
	 * everything, so that the error is reported once and not again by every
	 * expression around it.
	 */
	TYPE_ERROR,
	TYPE_BOOL,
	TYPE_INT,
	TYPE_STRING,
	TYPE_UNIT,
	TYPE_UNITS
} ValueType;

/** A name that a body can use, and the type of what it stands for. */
typedef struct {
	const char * name;
	ValueType type;
} Symbol;

/**
 * The names visible in a part of a body. A body starts with at most four (self,
 * its parameter, allies and enemies), and a "for" adds one in a scope of its
 * own, inside the one it is in. A scope lives in the stack, so it needs no
 * cleanup.
 */
#define SCOPE_CAPACITY 4

typedef struct Scope Scope;

struct Scope {
	const Scope * parent;
	Symbol symbols[SCOPE_CAPACITY];
	size_t count;
};

typedef enum {
	BODY_ABILITY,
	BODY_EFFECT,
	BODY_TURN
} BodyKind;

/** What a body needs to know about where it is. */
typedef struct {
	const SemanticModel * semanticModel;
	BodyKind kind;
	/** In an effect, the name of its parameter. */
	const char * parameter;
	/** In an "on turn", the unit it belongs to, or NULL when that unit doesn't exist. */
	const UnitDeclaration * unit;
} BodyContext;

/* PRIVATE FUNCTIONS */

static void _semanticError(const int line, const char * format, ...) __attribute__((format(printf, 2, 3)));
static int _indexOf(const char * name, const char * const * names, const size_t count);
static bool _isRepeated(const IdentifierList * list, const IdentifierList * item);
static bool _checkReservedName(const int line, const char * name, const char * usage);
static void _collectTags(const UnitDeclaration * unit, SemanticModel * semanticModel);
static void _collectDeclarations(const Program * program, SemanticModel * semanticModel);
static void _constantError(const int line, const char * context, const char * message);
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
static const char * _typeName(const ValueType type);
static bool _isUnitAttribute(const char * name);
static bool _listsIdentifier(const IdentifierList * list, const char * name);
static int _countArguments(const ExpressionList * arguments);
static void _define(Scope * scope, const char * name, const ValueType type);
static const Symbol * _lookup(const Scope * scope, const char * name);
static ValueType _resolveName(const int line, const char * name, const Scope * scope, const BodyContext * context);
static const char * _operatorOf(const ExpressionType type);
static ValueType _typeOf(const Expression * expression, const Scope * scope, const BodyContext * context);
static ValueType _typeOfMember(const Expression * expression, const Scope * scope, const BodyContext * context);
static ValueType _typeOfCall(const Expression * expression);
static ValueType _typeOfRadius(const Expression * expression, const Scope * scope, const BodyContext * context);
static ValueType _typeOfBinary(const Expression * expression, const Scope * scope, const BodyContext * context);
static void _checkInt(const char * keyword, const char * what, const Expression * expression, const Scope * scope, const BodyContext * context);
static void _checkTarget(const char * keyword, const Expression * target, const bool allowUnits, const Scope * scope, const BodyContext * context);
static void _checkCondition(const char * keyword, const Expression * condition, const Scope * scope, const BodyContext * context);
static void _checkPlaceholder(const int line, const LogSegment * segment, const Scope * scope, const BodyContext * context);
static void _checkLog(const Statement * statement, const Scope * scope, const BodyContext * context);
static void _checkUse(const Statement * statement, const Scope * scope, const BodyContext * context);
static void _checkApply(const Statement * statement, const Scope * scope, const BodyContext * context);
static void _checkFor(const Statement * statement, const Scope * scope, const BodyContext * context);
static void _checkStatement(const Statement * statement, const Scope * scope, const BodyContext * context);
static void _checkStatements(const StatementList * statements, const Scope * scope, const BodyContext * context);
static void _validateAbilityBody(const Declaration * declaration, const SemanticModel * semanticModel);
static void _validateEffectBody(const Declaration * declaration, const SemanticModel * semanticModel);
static void _validateTurnBody(const Declaration * declaration, const SemanticModel * semanticModel);

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
 * Reports the name if it is reserved. The usage is what the name was given to,
 * with its article (for example, "a unit name" or "an iterator").
 */
static bool _checkReservedName(const int line, const char * name, const char * usage) {
	const bool reserved = (0 <= _indexOf(name, RESERVED_NAMES, COUNT_OF(RESERVED_NAMES)));
	if (reserved) {
		_semanticError(line, "'%s' is a reserved name and cannot be used as %s", name, usage);
	}
	return reserved;
}

/**
 * Adds the tags of a unit to the model, reporting the reserved and repeated ones.
 */
static void _collectTags(const UnitDeclaration * unit, SemanticModel * semanticModel) {
	for (const IdentifierList * tag = unit->tags; tag != NULL; tag = tag->next) {
		const bool reserved = _checkReservedName(tag->line, tag->identifier, "a tag name");
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
				_checkReservedName(declaration->line, declaration->unitDeclaration->name, "a unit name");
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
				_checkReservedName(declaration->line, declaration->abilityDeclaration->name, "an ability name");
				first = findAbility(semanticModel, declaration->abilityDeclaration->name);
				if (first != NULL) {
					_semanticError(declaration->line, "ability '%s' is already declared (first declared at line %d)", declaration->abilityDeclaration->name, first->line);
				}
				else if (!addAbility(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				break;
			case EFFECT_DECLARATION:
				_checkReservedName(declaration->line, declaration->effectDeclaration->name, "an effect name");
				first = findEffect(semanticModel, declaration->effectDeclaration->name);
				if (first != NULL) {
					_semanticError(declaration->line, "effect '%s' is already declared (first declared at line %d)", declaration->effectDeclaration->name, first->line);
				}
				else if (!addEffect(semanticModel, declaration)) {
					_outOfMemory = true;
				}
				break;
			case TEAM_DECLARATION:
				_checkReservedName(declaration->line, declaration->teamDeclaration->name, "a team name");
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
 * Reports an error in a constant expression, followed by where it is. Nothing
 * is reported when there is no context.
 */
static void _constantError(const int line, const char * context, const char * message) {
	if (context != NULL) {
		_semanticError(line, "%s (in %s)", message, context);
	}
}

/**
 * Evaluates an expression in a constant context: int literals, dice and
 * arithmetic only. The context says where the expression is (for example,
 * "attribute 'health' of unit 'Hero'"), and ends every message about it. With
 * no context (NULL) it reports nothing: the result is just not valid.
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
			_constantError(expression->line, context, "expected int, got bool");
			break;
		case STRING_EXPRESSION:
			_constantError(expression->line, context, "expected int, got string");
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
			_constantError(expression->line, context, "expression is not allowed here: only int literals, dice and arithmetic");
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
		_constantError(expression->line, context, "division by zero in constant expression");
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
		_constantError(expression->line, context, "constant expression overflows");
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
 * Checks the types of target of an ability. Its body is checked apart.
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
 * only one for that unit. The body is checked apart.
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
 * Checks the members of a team: their units, quantities and positions. A unit
 * can be in several members of a team (for example, two groups of archers).
 */
static void _validateTeam(const Declaration * declaration, const SemanticModel * semanticModel, const ArenaBounds * arena) {
	const TeamDeclaration * team = declaration->teamDeclaration;
	char context[CONTEXT_SIZE];
	for (const MemberList * item = team->members; item != NULL; item = item->next) {
		const Member * member = item->member;
		if (findUnit(semanticModel, member->unitName) == NULL) {
			_semanticError(member->line, "team '%s' references undeclared unit '%s'", team->name, member->unitName);
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
 * already reported. A unit that a team has in several members is not in a
 * second team.
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
 * The name of a type, as the messages call it.
 */
static const char * _typeName(const ValueType type) {
	switch (type) {
		case TYPE_BOOL:
			return "bool";
		case TYPE_INT:
			return "int";
		case TYPE_STRING:
			return "string";
		case TYPE_UNIT:
			return "unit";
		case TYPE_UNITS:
			return "units";
		case TYPE_ERROR:
			break;
	}
	return "error";
}

/**
 * Whether the name is one of the attributes that every unit has.
 */
static bool _isUnitAttribute(const char * name) {
	return 0 <= _indexOf(name, UNIT_ATTRIBUTES, COUNT_OF(UNIT_ATTRIBUTES));
}

/**
 * Whether the list has an identifier with that name.
 */
static bool _listsIdentifier(const IdentifierList * list, const char * name) {
	for (const IdentifierList * item = list; item != NULL; item = item->next) {
		if (strcmp(item->identifier, name) == 0) {
			return true;
		}
	}
	return false;
}

static int _countArguments(const ExpressionList * arguments) {
	int count = 0;
	for (const ExpressionList * item = arguments; item != NULL; item = item->next) {
		++count;
	}
	return count;
}

/**
 * Makes a name visible in a scope. A scope never needs more than its capacity.
 */
static void _define(Scope * scope, const char * name, const ValueType type) {
	if (scope->count < SCOPE_CAPACITY) {
		scope->symbols[scope->count].name = name;
		scope->symbols[scope->count].type = type;
		++scope->count;
	}
}

/**
 * What a name stands for in a scope, looking in the enclosing ones too, or NULL
 * when it is not visible. The last name defined in a scope wins.
 */
static const Symbol * _lookup(const Scope * scope, const char * name) {
	for (const Scope * current = scope; current != NULL; current = current->parent) {
		for (size_t k = current->count; 0 < k; --k) {
			if (strcmp(current->symbols[k - 1].name, name) == 0) {
				return &current->symbols[k - 1];
			}
		}
	}
	return NULL;
}

/**
 * The type of a name in a body. A name that is not visible is reported, with
 * the reason that fits it best, and it has no type (TYPE_ERROR).
 */
static ValueType _resolveName(const int line, const char * name, const Scope * scope, const BodyContext * context) {
	const Symbol * symbol = _lookup(scope, name);
	if (symbol != NULL) {
		return symbol->type;
	}
	if (context->kind == BODY_EFFECT && strcmp(name, "self") == 0) {
		_semanticError(line, "'self' is not available in an effect: use its parameter '%s'", context->parameter);
	}
	else if (strcmp(name, "ally") == 0 || strcmp(name, "enemy") == 0) {
		_semanticError(line, "'%s' can only be used as the argument of nearest", name);
	}
	else {
		_semanticError(line, "'%s' is not defined", name);
	}
	return TYPE_ERROR;
}

/**
 * How an operator is written, for the messages.
 */
static const char * _operatorOf(const ExpressionType type) {
	switch (type) {
		case ADD_EXPRESSION:
			return "+";
		case AND_EXPRESSION:
			return "and";
		case DIV_EXPRESSION:
			return "/";
		case EQUALS_EXPRESSION:
			return "==";
		case GREATER_EQUAL_EXPRESSION:
			return ">=";
		case GREATER_THAN_EXPRESSION:
			return ">";
		case LESS_EQUAL_EXPRESSION:
			return "<=";
		case LESS_THAN_EXPRESSION:
			return "<";
		case MUL_EXPRESSION:
			return "*";
		case NOT_EQUALS_EXPRESSION:
			return "!=";
		case NOT_EXPRESSION:
			return "not";
		case OR_EXPRESSION:
			return "or";
		case SUB_EXPRESSION:
			return "-";
		default:
			break;
	}
	return "?";
}

/**
 * The type of an expression in a body. Every error found inside it is reported
 * once, and then the expression has no type (TYPE_ERROR), so that the errors it
 * would cause in the expressions around it are not reported.
 */
static ValueType _typeOf(const Expression * expression, const Scope * scope, const BodyContext * context) {
	switch (expression->type) {
		case BOOLEAN_EXPRESSION:
			return TYPE_BOOL;
		case DICE_EXPRESSION:
		case INTEGER_EXPRESSION:
			return TYPE_INT;
		case STRING_EXPRESSION:
			return TYPE_STRING;
		case IDENTIFIER_EXPRESSION:
			return _resolveName(expression->line, expression->text, scope, context);
		case MEMBER_EXPRESSION:
			return _typeOfMember(expression, scope, context);
		case CALL_EXPRESSION:
			return _typeOfCall(expression);
		case RADIUS_EXPRESSION:
			return _typeOfRadius(expression, scope, context);
		case NOT_EXPRESSION: {
			const ValueType operand = _typeOf(expression->operand, scope, context);
			if (operand == TYPE_ERROR) {
				return TYPE_ERROR;
			}
			if (operand != TYPE_BOOL) {
				_semanticError(expression->line, "operator 'not' needs a bool operand, got %s", _typeName(operand));
				return TYPE_ERROR;
			}
			return TYPE_BOOL;
		}
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
			return _typeOfBinary(expression, scope, context);
	}
	return TYPE_ERROR;
}

/**
 * "unit.attribute": the object is a unit and the attribute is one of its
 * four, and the result is an int.
 */
static ValueType _typeOfMember(const Expression * expression, const Scope * scope, const BodyContext * context) {
	const ValueType object = _typeOf(expression->object, scope, context);
	if (object == TYPE_ERROR) {
		return TYPE_ERROR;
	}
	if (object != TYPE_UNIT) {
		_semanticError(expression->line, "member access needs a unit, got %s", _typeName(object));
		return TYPE_ERROR;
	}
	if (!_isUnitAttribute(expression->member)) {
		_semanticError(expression->line, "unknown attribute '%s'", expression->member);
		return TYPE_ERROR;
	}
	return TYPE_INT;
}

/**
 * The only function is "nearest", as "nearest(ally)" or "nearest(enemy)", and
 * it is a unit. The arguments of an unknown function are not looked into.
 */
static ValueType _typeOfCall(const Expression * expression) {
	const char * name = expression->functionName;
	const int count = _countArguments(expression->arguments);
	if (strcmp(name, "nearest") == 0) {
		if (count != 1) {
			_semanticError(expression->line, "function 'nearest' expects 1 argument(s), got %d", count);
			return TYPE_ERROR;
		}
		const Expression * relation = expression->arguments->expression;
		if (relation->type == IDENTIFIER_EXPRESSION && (strcmp(relation->text, "ally") == 0 || strcmp(relation->text, "enemy") == 0)) {
			return TYPE_UNIT;
		}
		_semanticError(relation->line, "'nearest' needs 'ally' or 'enemy' as its argument");
		return TYPE_ERROR;
	}
	_semanticError(expression->line, "unknown function '%s'", name);
	return TYPE_ERROR;
}

/**
 * "radius <int> of <unit>": the units within that distance, which is units.
 */
static ValueType _typeOfRadius(const Expression * expression, const Scope * scope, const BodyContext * context) {
	const ValueType distance = _typeOf(expression->radius, scope, context);
	const ValueType center = _typeOf(expression->center, scope, context);
	if (distance != TYPE_ERROR && distance != TYPE_INT) {
		_semanticError(expression->radius->line, "'radius' needs an int distance, got %s", _typeName(distance));
	}
	if (center != TYPE_ERROR && center != TYPE_UNIT) {
		_semanticError(expression->center->line, "'radius' needs a unit as its center, got %s", _typeName(center));
	}
	return (distance == TYPE_INT && center == TYPE_UNIT) ? TYPE_UNITS : TYPE_ERROR;
}

/**
 * The operators with two operands: arithmetic (int and int, giving an int),
 * order (int and int, giving a bool), equality (two of the same type, other
 * than units, giving a bool) and logic (bool and bool, giving a bool). Both
 * operands are always typed, so the error of each one is reported.
 */
static ValueType _typeOfBinary(const Expression * expression, const Scope * scope, const BodyContext * context) {
	const ValueType left = _typeOf(expression->leftExpression, scope, context);
	const ValueType right = _typeOf(expression->rightExpression, scope, context);
	if (left == TYPE_ERROR || right == TYPE_ERROR) {
		return TYPE_ERROR;
	}
	const char * symbol = _operatorOf(expression->type);
	switch (expression->type) {
		case ADD_EXPRESSION:
		case DIV_EXPRESSION:
		case MUL_EXPRESSION:
		case SUB_EXPRESSION:
			if (left != TYPE_INT || right != TYPE_INT) {
				_semanticError(expression->line, "operator '%s' needs int operands, got %s and %s", symbol, _typeName(left), _typeName(right));
				return TYPE_ERROR;
			}
			if (expression->type == DIV_EXPRESSION) {
				const ConstantValue divisor = _evaluateConstant(expression->rightExpression, NULL);
				if (divisor.valid && !divisor.hasDice && divisor.value == 0) {
					_semanticError(expression->line, "division by zero");
					return TYPE_ERROR;
				}
			}
			return TYPE_INT;
		case GREATER_EQUAL_EXPRESSION:
		case GREATER_THAN_EXPRESSION:
		case LESS_EQUAL_EXPRESSION:
		case LESS_THAN_EXPRESSION:
			if (left != TYPE_INT || right != TYPE_INT) {
				_semanticError(expression->line, "operator '%s' needs int operands, got %s and %s", symbol, _typeName(left), _typeName(right));
				return TYPE_ERROR;
			}
			return TYPE_BOOL;
		case EQUALS_EXPRESSION:
		case NOT_EQUALS_EXPRESSION:
			if (left != right || left == TYPE_UNITS) {
				_semanticError(expression->line, "operator '%s' cannot compare %s with %s", symbol, _typeName(left), _typeName(right));
				return TYPE_ERROR;
			}
			return TYPE_BOOL;
		case AND_EXPRESSION:
		case OR_EXPRESSION:
			if (left != TYPE_BOOL || right != TYPE_BOOL) {
				_semanticError(expression->line, "operator '%s' needs bool operands, got %s and %s", symbol, _typeName(left), _typeName(right));
				return TYPE_ERROR;
			}
			return TYPE_BOOL;
		default:
			break;
	}
	return TYPE_ERROR;
}

/**
 * Checks that a part of a statement is an int. The "what" says which part.
 */
static void _checkInt(const char * keyword, const char * what, const Expression * expression, const Scope * scope, const BodyContext * context) {
	const ValueType type = _typeOf(expression, scope, context);
	if (type != TYPE_ERROR && type != TYPE_INT) {
		_semanticError(expression->line, "'%s' needs an int %s, got %s", keyword, what, _typeName(type));
	}
}

/**
 * Checks the target of a statement: a unit, or also units when the statement
 * can affect several.
 */
static void _checkTarget(const char * keyword, const Expression * target, const bool allowUnits, const Scope * scope, const BodyContext * context) {
	const ValueType type = _typeOf(target, scope, context);
	if (type == TYPE_ERROR || type == TYPE_UNIT || (allowUnits && type == TYPE_UNITS)) {
		return;
	}
	if (allowUnits) {
		_semanticError(target->line, "'%s' needs a unit or units as its target, got %s", keyword, _typeName(type));
	}
	else {
		_semanticError(target->line, "'%s' needs a unit as its target, got %s", keyword, _typeName(type));
	}
}

/**
 * Checks the condition of an "if" or a "while": a bool.
 */
static void _checkCondition(const char * keyword, const Expression * condition, const Scope * scope, const BodyContext * context) {
	const ValueType type = _typeOf(condition, scope, context);
	if (type != TYPE_ERROR && type != TYPE_BOOL) {
		_semanticError(condition->line, "'%s' needs a bool condition, got %s", keyword, _typeName(type));
	}
}

/**
 * Checks a placeholder of a log message: its name is visible, and it is either
 * a value that can be printed or a unit with the attribute that follows it.
 */
static void _checkPlaceholder(const int line, const LogSegment * segment, const Scope * scope, const BodyContext * context) {
	const ValueType type = _resolveName(line, segment->text, scope, context);
	if (type == TYPE_ERROR) {
		return;
	}
	if (segment->attribute != NULL) {
		if (type != TYPE_UNIT) {
			_semanticError(line, "member access needs a unit, got %s", _typeName(type));
		}
		else if (!_isUnitAttribute(segment->attribute)) {
			_semanticError(line, "unknown attribute '%s'", segment->attribute);
		}
	}
	else if (type == TYPE_UNITS) {
		_semanticError(line, "'%s' is a collection of units and cannot be printed", segment->text);
	}
}

/**
 * Checks the message of a "log": its braces, and then each placeholder.
 */
static void _checkLog(const Statement * statement, const Scope * scope, const BodyContext * context) {
	LogTemplate * logTemplate = parseLogTemplate(statement->logStatement->message);
	if (logTemplate == NULL) {
		_outOfMemory = true;
		return;
	}
	switch (logTemplate->error) {
		case LOG_TEMPLATE_VALID:
			for (size_t k = 0; k < logTemplate->count; ++k) {
				if (logTemplate->segments[k].type == LOG_SEGMENT_PLACEHOLDER) {
					_checkPlaceholder(statement->line, &logTemplate->segments[k], scope, context);
				}
			}
			break;
		case LOG_TEMPLATE_UNCLOSED_BRACE:
			_semanticError(statement->line, "unclosed '{' in log message");
			break;
		case LOG_TEMPLATE_UNMATCHED_BRACE:
			_semanticError(statement->line, "unmatched '}' in log message");
			break;
		case LOG_TEMPLATE_EMPTY_PLACEHOLDER:
			_semanticError(statement->line, "empty placeholder '{}' in log message");
			break;
		case LOG_TEMPLATE_INVALID_PLACEHOLDER:
			_semanticError(statement->line, "invalid placeholder '%s' in log message: use {name} or {name.attribute}", logTemplate->invalid);
			break;
	}
	destroyLogTemplate(logTemplate);
}

/**
 * Checks "use <ability> on <unit>". In an "on turn" the ability must also be
 * one of those of its unit.
 */
static void _checkUse(const Statement * statement, const Scope * scope, const BodyContext * context) {
	const UseStatement * use = statement->useStatement;
	_checkTarget("use", use->target, false, scope, context);
	if (findAbility(context->semanticModel, use->abilityName) == NULL) {
		_semanticError(statement->line, "'use' references undeclared ability '%s'", use->abilityName);
	}
	else if (context->kind == BODY_TURN && context->unit != NULL && !_listsIdentifier(context->unit->abilities, use->abilityName)) {
		_semanticError(statement->line, "unit '%s' cannot use ability '%s': it is not in its abilities", context->unit->name, use->abilityName);
	}
}

/**
 * Checks "apply <effect> to <unit or units> for <int>".
 */
static void _checkApply(const Statement * statement, const Scope * scope, const BodyContext * context) {
	const ApplyStatement * apply = statement->applyStatement;
	_checkTarget("apply", apply->target, true, scope, context);
	_checkInt("apply", "duration", apply->duration, scope, context);
	if (findEffect(context->semanticModel, apply->effectName) == NULL) {
		_semanticError(statement->line, "'apply' references undeclared effect '%s'", apply->effectName);
	}
}

/**
 * Checks "for (<name> in <units>)". The name is a unit within the loop and it
 * can't take one that is already visible, but it is not visible in the
 * collection, which is checked first.
 */
static void _checkFor(const Statement * statement, const Scope * scope, const BodyContext * context) {
	const ForStatement * loop = statement->forStatement;
	const ValueType collection = _typeOf(loop->collection, scope, context);
	if (collection != TYPE_ERROR && collection != TYPE_UNITS) {
		_semanticError(loop->collection->line, "'for' needs units to iterate over, got %s", _typeName(collection));
	}
	if (!_checkReservedName(statement->line, loop->iterator, "an iterator") && _lookup(scope, loop->iterator) != NULL) {
		_semanticError(statement->line, "'%s' is already defined", loop->iterator);
	}
	Scope inner = { .parent = scope, .count = 0 };
	_define(&inner, loop->iterator, TYPE_UNIT);
	_checkStatements(loop->body, &inner, context);
}

static void _checkStatement(const Statement * statement, const Scope * scope, const BodyContext * context) {
	switch (statement->type) {
		case APPLY_STATEMENT:
			_checkApply(statement, scope, context);
			break;
		case DEAL_STATEMENT:
			_checkInt("deal", "amount", statement->dealStatement->amount, scope, context);
			_checkTarget("deal", statement->dealStatement->target, true, scope, context);
			break;
		case FOR_STATEMENT:
			_checkFor(statement, scope, context);
			break;
		case HEAL_STATEMENT:
			_checkInt("heal", "amount", statement->healStatement->amount, scope, context);
			_checkTarget("heal", statement->healStatement->target, true, scope, context);
			break;
		case IF_STATEMENT:
			_checkCondition("if", statement->ifStatement->condition, scope, context);
			_checkStatements(statement->ifStatement->thenBranch, scope, context);
			_checkStatements(statement->ifStatement->elseBranch, scope, context);
			break;
		case LOG_STATEMENT:
			_checkLog(statement, scope, context);
			break;
		case MOVE_STATEMENT:
			_checkTarget("move", statement->moveStatement->target, false, scope, context);
			break;
		case USE_STATEMENT:
			_checkUse(statement, scope, context);
			break;
		case WHILE_STATEMENT:
			_checkCondition("while", statement->whileStatement->condition, scope, context);
			_checkStatements(statement->whileStatement->body, scope, context);
			break;
	}
}

/**
 * The statements of a body (or of one of its blocks), in order. An "if" or a
 * "while" doesn't open a scope, and a "for" does.
 */
static void _checkStatements(const StatementList * statements, const Scope * scope, const BodyContext * context) {
	for (const StatementList * item = statements; item != NULL; item = item->next) {
		_checkStatement(item->statement, scope, context);
	}
}

/**
 * Checks the body of an ability, which sees the unit that uses it (self), its
 * target (by the name of its parameter) and the units around (allies and
 * enemies).
 */
static void _validateAbilityBody(const Declaration * declaration, const SemanticModel * semanticModel) {
	const AbilityDeclaration * ability = declaration->abilityDeclaration;
	_checkReservedName(declaration->line, ability->targetParameter, "a target parameter");
	Scope scope = { .parent = NULL, .count = 0 };
	_define(&scope, "self", TYPE_UNIT);
	_define(&scope, ability->targetParameter, TYPE_UNIT);
	_define(&scope, "allies", TYPE_UNITS);
	_define(&scope, "enemies", TYPE_UNITS);
	const BodyContext context = { .semanticModel = semanticModel, .kind = BODY_ABILITY, .parameter = ability->targetParameter, .unit = NULL };
	_checkStatements(ability->body, &scope, &context);
}

/**
 * Checks the body of an effect, which sees the unit that carries it (by the
 * name of its parameter) and the units around it (allies and enemies, from
 * its point of view). It has no "self": the unit that applied it is not known.
 */
static void _validateEffectBody(const Declaration * declaration, const SemanticModel * semanticModel) {
	const EffectDeclaration * effect = declaration->effectDeclaration;
	_checkReservedName(declaration->line, effect->targetParameter, "a target parameter");
	Scope scope = { .parent = NULL, .count = 0 };
	_define(&scope, effect->targetParameter, TYPE_UNIT);
	_define(&scope, "allies", TYPE_UNITS);
	_define(&scope, "enemies", TYPE_UNITS);
	const BodyContext context = { .semanticModel = semanticModel, .kind = BODY_EFFECT, .parameter = effect->targetParameter, .unit = NULL };
	_checkStatements(effect->body, &scope, &context);
}

/**
 * Checks the body of an "on turn", which sees its unit (self) and the units
 * around it (allies and enemies). It is checked even if the unit doesn't
 * exist, which was already reported, except for the abilities of the unit.
 */
static void _validateTurnBody(const Declaration * declaration, const SemanticModel * semanticModel) {
	const TurnDeclaration * turn = declaration->turnDeclaration;
	const Declaration * unit = findUnit(semanticModel, turn->unitName);
	Scope scope = { .parent = NULL, .count = 0 };
	_define(&scope, "self", TYPE_UNIT);
	_define(&scope, "allies", TYPE_UNITS);
	_define(&scope, "enemies", TYPE_UNITS);
	const BodyContext context = { .semanticModel = semanticModel, .kind = BODY_TURN, .parameter = NULL, .unit = (unit != NULL) ? unit->unitDeclaration : NULL };
	_checkStatements(turn->body, &scope, &context);
}

/**
 * Second pass: validates the references between declarations and the rules of
 * each kind, now that every declaration is in the model, and then the bodies
 * of the abilities, effects and "on turn" declarations.
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
				_validateAbilityBody(declaration, semanticModel);
				break;
			case EFFECT_DECLARATION:
				_validateEffectBody(declaration, semanticModel);
				break;
			case TURN_DECLARATION:
				_validateTurn(declaration, semanticModel);
				_validateTurnBody(declaration, semanticModel);
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
