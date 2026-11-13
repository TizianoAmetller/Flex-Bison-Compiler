#include "Evaluator.h"
#include "SimulationLimits.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PRIVATE FUNCTIONS */

static Value _integer(const int64_t number);
static Value _boolean(const bool boolean);
static Value _unit(const int unit);
static void _unexpected(Runtime * runtime, const int line, const char * expected, const Value * value);
static bool _isCandidate(const World * world, const int perspective, const int other, const bool allies);
static Value _collectUnits(Runtime * runtime, const int perspective, const bool allies, const int line);
static Value _rollDice(Runtime * runtime, const DiceValue dice, const int line);
static Value _evaluateMember(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope);
static Value _nearest(Runtime * runtime, const char * relation, const int perspective, const int line);
static Value _evaluateCall(Runtime * runtime, const Expression * expression, const int perspective);
static Value _evaluateRadius(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope);
static Value _applyOperator(Runtime * runtime, const Expression * expression, const Value * left, const Value * right);
static Value _evaluateLogical(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope);
static Value _evaluateBinary(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope);
static Value _evaluateExpression(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope);

static Value _integer(const int64_t number) {
	Value value = noneValue();
	value.kind = VALUE_INT;
	value.number = number;
	return value;
}

static Value _boolean(const bool boolean) {
	Value value = noneValue();
	value.kind = VALUE_BOOL;
	value.number = boolean ? 1 : 0;
	return value;
}

static Value _unit(const int unit) {
	Value value = noneValue();
	value.kind = VALUE_UNIT;
	value.number = unit;
	return value;
}

/**
 * Fails because a value is not of the kind that the program needs. The
 * semantic analysis rules this out, so it is a defense and not an expected
 * error.
 */
static void _unexpected(Runtime * runtime, const int line, const char * expected, const Value * value) {
	runtimeFail(runtime, "line %d: expected %s, got %s", line, expected, valueKindName(value->kind));
}

/**
 * Whether a unit counts as an ally (the same team, but not the perspective
 * itself) or as an enemy (another team) of the perspective, and is alive.
 */
static bool _isCandidate(const World * world, const int perspective, const int other, const bool allies) {
	if (!world->units[other].alive) {
		return false;
	}
	return allies ? isAlly(world, perspective, other) : isEnemy(world, perspective, other);
}

/**
 * The allies or the enemies of a unit that are alive, in the order of creation.
 */
static Value _collectUnits(Runtime * runtime, const int perspective, const bool allies, const int line) {
	Value value = noneValue();
	if (perspective < 0) {
		runtimeFail(runtime, "line %d: '%s' needs a unit to look from", line, allies ? "allies" : "enemies");
		return value;
	}
	const World * world = runtime->world;
	size_t count = 0;
	for (size_t k = 0; k < world->unitCount; ++k) {
		if (_isCandidate(world, perspective, (int) k, allies)) {
			++count;
		}
	}
	if (0 < count) {
		value.units = malloc(count * sizeof(int));
		if (value.units == NULL) {
			runtimeOutOfMemory(runtime);
			return noneValue();
		}
		size_t next = 0;
		for (size_t k = 0; k < world->unitCount; ++k) {
			if (_isCandidate(world, perspective, (int) k, allies)) {
				value.units[next] = (int) k;
				++next;
			}
		}
	}
	value.kind = VALUE_UNITS;
	value.count = count;
	return value;
}

/**
 * Throws the dice of a literal like "2d6": the sum of that many rolls between 1
 * and the number of sides, and 0 when there are no dice or no sides.
 */
static Value _rollDice(Runtime * runtime, const DiceValue dice, const int line) {
	if (dice.count <= 0 || dice.sides <= 0) {
		return _integer(0);
	}
	if (MAX_DICE < dice.count) {
		runtimeFail(runtime, "line %d: too many dice (%d): a roll can throw at most %d", line, dice.count, MAX_DICE);
		return noneValue();
	}
	int64_t total = 0;
	for (int k = 0; k < dice.count; ++k) {
		total += randomBetween(runtime->rng, 1, dice.sides);
	}
	return _integer(total);
}

/**
 * "unit.attribute": an int. Where there is no unit, there is no attribute.
 */
static Value _evaluateMember(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope) {
	Value object = evaluate(runtime, expression->object, perspective, scope);
	if (runtime->failed || object.kind == VALUE_NONE) {
		releaseValue(&object);
		return noneValue();
	}
	if (object.kind != VALUE_UNIT) {
		_unexpected(runtime, expression->line, "a unit", &object);
		releaseValue(&object);
		return noneValue();
	}
	int64_t number = 0;
	if (!getUnitAttribute(getUnit(runtime->world, (int) object.number), expression->member, &number)) {
		runtimeFail(runtime, "line %d: unknown attribute '%s'", expression->line, expression->member);
		return noneValue();
	}
	return _integer(number);
}

/**
 * The closest unit alive that is an ally (or an enemy) of the perspective, by
 * the square of the distance. Of those at the same distance, the first one
 * created. None if there isn't one.
 */
static Value _nearest(Runtime * runtime, const char * relation, const int perspective, const int line) {
	if (perspective < 0) {
		runtimeFail(runtime, "line %d: 'nearest' needs a unit to look from", line);
		return noneValue();
	}
	const World * world = runtime->world;
	const bool allies = (strcmp(relation, "ally") == 0);
	const Unit * from = getUnit(world, perspective);
	int best = -1;
	int64_t bestDistance = 0;
	for (size_t k = 0; k < world->unitCount; ++k) {
		if (_isCandidate(world, perspective, (int) k, allies)) {
			const int64_t distance = squaredDistance(from, &world->units[k]);
			if (best < 0 || distance < bestDistance) {
				best = (int) k;
				bestDistance = distance;
			}
		}
	}
	return (best < 0) ? noneValue() : _unit(best);
}

/**
 * "nearest(ally)" and "nearest(enemy)", the only calls there are.
 */
static Value _evaluateCall(Runtime * runtime, const Expression * expression, const int perspective) {
	const char * name = expression->functionName;
	const ExpressionList * arguments = expression->arguments;
	if (arguments == NULL || arguments->next != NULL) {
		runtimeFail(runtime, "line %d: function '%s' expects 1 argument", expression->line, name);
		return noneValue();
	}
	if (strcmp(name, "nearest") == 0) {
		const Expression * relation = arguments->expression;
		if (relation->type != IDENTIFIER_EXPRESSION || (strcmp(relation->text, "ally") != 0 && strcmp(relation->text, "enemy") != 0)) {
			runtimeFail(runtime, "line %d: 'nearest' needs 'ally' or 'enemy'", expression->line);
			return noneValue();
		}
		return _nearest(runtime, relation->text, perspective, expression->line);
	}
	runtimeFail(runtime, "line %d: unknown function '%s'", expression->line, name);
	return noneValue();
}

/**
 * "radius r of c": the units alive of any team (c included) whose distance to c
 * is at most r. A radius of 0 or less is just the cell of c, and a very large
 * one is capped.
 */
static Value _evaluateRadius(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope) {
	Value distance = evaluate(runtime, expression->radius, perspective, scope);
	Value center = evaluate(runtime, expression->center, perspective, scope);
	Value result = noneValue();
	if (!runtime->failed && distance.kind != VALUE_NONE && center.kind != VALUE_NONE) {
		if (distance.kind != VALUE_INT) {
			_unexpected(runtime, expression->line, "an int", &distance);
		}
		else if (center.kind != VALUE_UNIT) {
			_unexpected(runtime, expression->line, "a unit", &center);
		}
		else {
			int64_t radius = distance.number;
			if (MAX_RADIUS < radius) {
				radius = MAX_RADIUS;
			}
			const int64_t limit = (0 < radius) ? radius * radius : 0;
			const World * world = runtime->world;
			const Unit * origin = getUnit(world, (int) center.number);
			size_t count = 0;
			for (size_t k = 0; k < world->unitCount; ++k) {
				if (world->units[k].alive && squaredDistance(origin, &world->units[k]) <= limit) {
					++count;
				}
			}
			int * units = NULL;
			if (0 < count) {
				units = malloc(count * sizeof(int));
				if (units == NULL) {
					runtimeOutOfMemory(runtime);
				}
				else {
					size_t next = 0;
					for (size_t k = 0; k < world->unitCount; ++k) {
						if (world->units[k].alive && squaredDistance(origin, &world->units[k]) <= limit) {
							units[next] = (int) k;
							++next;
						}
					}
				}
			}
			if (!runtime->failed) {
				result.kind = VALUE_UNITS;
				result.units = units;
				result.count = count;
			}
		}
	}
	releaseValue(&distance);
	releaseValue(&center);
	return result;
}

/**
 * Applies a binary operator to two values that are not NONE. The arithmetic is
 * in 64 bits and fails if it overflows or divides by 0; a division truncates
 * toward 0.
 */
static Value _applyOperator(Runtime * runtime, const Expression * expression, const Value * left, const Value * right) {
	const int line = expression->line;
	int64_t result = 0;
	switch (expression->type) {
		case ADD_EXPRESSION:
		case DIV_EXPRESSION:
		case MUL_EXPRESSION:
		case SUB_EXPRESSION:
			if (left->kind != VALUE_INT || right->kind != VALUE_INT) {
				_unexpected(runtime, line, "an int", (left->kind != VALUE_INT) ? left : right);
				return noneValue();
			}
			if (expression->type == DIV_EXPRESSION) {
				if (right->number == 0) {
					runtimeFail(runtime, "line %d: division by zero", line);
					return noneValue();
				}
				if (left->number == INT64_MIN && right->number == -1) {
					runtimeFail(runtime, "line %d: arithmetic overflow", line);
					return noneValue();
				}
				return _integer(left->number / right->number);
			}
			if ((expression->type == ADD_EXPRESSION && __builtin_add_overflow(left->number, right->number, &result))
				|| (expression->type == SUB_EXPRESSION && __builtin_sub_overflow(left->number, right->number, &result))
				|| (expression->type == MUL_EXPRESSION && __builtin_mul_overflow(left->number, right->number, &result))) {
				runtimeFail(runtime, "line %d: arithmetic overflow", line);
				return noneValue();
			}
			return _integer(result);
		case GREATER_EQUAL_EXPRESSION:
		case GREATER_THAN_EXPRESSION:
		case LESS_EQUAL_EXPRESSION:
		case LESS_THAN_EXPRESSION:
			if (left->kind != VALUE_INT || right->kind != VALUE_INT) {
				_unexpected(runtime, line, "an int", (left->kind != VALUE_INT) ? left : right);
				return noneValue();
			}
			switch (expression->type) {
				case GREATER_EQUAL_EXPRESSION:
					return _boolean(right->number <= left->number);
				case GREATER_THAN_EXPRESSION:
					return _boolean(right->number < left->number);
				case LESS_EQUAL_EXPRESSION:
					return _boolean(left->number <= right->number);
				default:
					return _boolean(left->number < right->number);
			}
		case EQUALS_EXPRESSION:
		case NOT_EQUALS_EXPRESSION: {
			bool equal = false;
			if (left->kind != right->kind || left->kind == VALUE_UNITS) {
				_unexpected(runtime, line, valueKindName(left->kind), right);
				return noneValue();
			}
			if (left->kind == VALUE_STRING) {
				equal = (strcmp(left->string, right->string) == 0);
			}
			else {
				equal = (left->number == right->number);
			}
			return _boolean((expression->type == EQUALS_EXPRESSION) ? equal : !equal);
		}
		default:
			break;
	}
	runtimeFail(runtime, "line %d: unexpected operator", line);
	return noneValue();
}

/**
 * "and" and "or" short-circuit: the left side is evaluated first. If it is NONE,
 * so is the result, and the right side is not evaluated. If it decides the
 * result (false for "and", true for "or") that is the result, and the right side
 * is not evaluated either. Otherwise the result is the right side, which may be
 * NONE.
 */
static Value _evaluateLogical(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope) {
	Value left = evaluate(runtime, expression->leftExpression, perspective, scope);
	if (runtime->failed || left.kind == VALUE_NONE) {
		releaseValue(&left);
		return noneValue();
	}
	if (left.kind != VALUE_BOOL) {
		_unexpected(runtime, expression->line, "a bool", &left);
		releaseValue(&left);
		return noneValue();
	}
	const bool isAnd = (expression->type == AND_EXPRESSION);
	if ((left.number != 0) != isAnd) {
		return _boolean(!isAnd);
	}
	Value right = evaluate(runtime, expression->rightExpression, perspective, scope);
	if (!runtime->failed && right.kind != VALUE_NONE && right.kind != VALUE_BOOL) {
		_unexpected(runtime, expression->line, "a bool", &right);
		releaseValue(&right);
		return noneValue();
	}
	return right;
}

/**
 * The binary operators other than "and" and "or": both operands are always
 * evaluated, and if any of them is NONE, so is the result.
 */
static Value _evaluateBinary(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope) {
	Value left = evaluate(runtime, expression->leftExpression, perspective, scope);
	Value right = evaluate(runtime, expression->rightExpression, perspective, scope);
	Value result = noneValue();
	if (!runtime->failed && left.kind != VALUE_NONE && right.kind != VALUE_NONE) {
		result = _applyOperator(runtime, expression, &left, &right);
	}
	releaseValue(&left);
	releaseValue(&right);
	return result;
}

static Value _evaluateExpression(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope) {
	switch (expression->type) {
		case INTEGER_EXPRESSION:
			return _integer(expression->integerValue);
		case DICE_EXPRESSION:
			return _rollDice(runtime, expression->dice, expression->line);
		case BOOLEAN_EXPRESSION:
			return _boolean(expression->booleanValue);
		case STRING_EXPRESSION: {
			Value value = noneValue();
			value.kind = VALUE_STRING;
			value.string = expression->text;
			return value;
		}
		case IDENTIFIER_EXPRESSION:
			return resolveName(runtime, expression->text, perspective, scope, expression->line);
		case MEMBER_EXPRESSION:
			return _evaluateMember(runtime, expression, perspective, scope);
		case CALL_EXPRESSION:
			return _evaluateCall(runtime, expression, perspective);
		case RADIUS_EXPRESSION:
			return _evaluateRadius(runtime, expression, perspective, scope);
		case NOT_EXPRESSION: {
			Value operand = evaluate(runtime, expression->operand, perspective, scope);
			Value result = noneValue();
			if (!runtime->failed && operand.kind != VALUE_NONE) {
				if (operand.kind == VALUE_BOOL) {
					result = _boolean(operand.number == 0);
				}
				else {
					_unexpected(runtime, expression->line, "a bool", &operand);
				}
			}
			releaseValue(&operand);
			return result;
		}
		case AND_EXPRESSION:
		case OR_EXPRESSION:
			return _evaluateLogical(runtime, expression, perspective, scope);
		case ADD_EXPRESSION:
		case DIV_EXPRESSION:
		case EQUALS_EXPRESSION:
		case GREATER_EQUAL_EXPRESSION:
		case GREATER_THAN_EXPRESSION:
		case LESS_EQUAL_EXPRESSION:
		case LESS_THAN_EXPRESSION:
		case MUL_EXPRESSION:
		case NOT_EQUALS_EXPRESSION:
		case SUB_EXPRESSION:
			return _evaluateBinary(runtime, expression, perspective, scope);
	}
	return noneValue();
}

/* PUBLIC FUNCTIONS */

void runtimeFail(Runtime * runtime, const char * format, ...) {
	if (runtime->failed) {
		return;
	}
	runtime->failed = true;
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(runtime->error, sizeof(runtime->error), format, arguments);
	va_end(arguments);
}

void runtimeOutOfMemory(Runtime * runtime) {
	runtimeFail(runtime, "out of memory");
	runtime->outOfMemory = true;
}

Value noneValue(void) {
	Value value;
	value.kind = VALUE_NONE;
	value.number = 0;
	value.string = NULL;
	value.units = NULL;
	value.count = 0;
	return value;
}

void releaseValue(Value * value) {
	free(value->units);
	value->units = NULL;
	value->count = 0;
}

const char * valueKindName(const ValueKind kind) {
	switch (kind) {
		case VALUE_NONE:
			return "nothing";
		case VALUE_INT:
			return "int";
		case VALUE_BOOL:
			return "bool";
		case VALUE_STRING:
			return "string";
		case VALUE_UNIT:
			return "unit";
		case VALUE_UNITS:
			return "units";
	}
	return "unknown";
}

Value evaluate(Runtime * runtime, const Expression * expression, const int perspective, const RuntimeScope * scope) {
	if (runtime->failed) {
		return noneValue();
	}
	if (MAX_EXPRESSION_DEPTH <= runtime->expressionDepth) {
		runtimeFail(runtime, "line %d: the expression is nested too deep (the limit is %d)", expression->line, MAX_EXPRESSION_DEPTH);
		return noneValue();
	}
	++runtime->expressionDepth;
	Value value = _evaluateExpression(runtime, expression, perspective, scope);
	--runtime->expressionDepth;
	return value;
}

Value resolveName(Runtime * runtime, const char * name, const int perspective, const RuntimeScope * scope, const int line) {
	for (const RuntimeScope * current = scope; current != NULL; current = current->parent) {
		if (strcmp(current->name, name) == 0) {
			return _unit(current->unit);
		}
	}
	if (strcmp(name, "allies") == 0) {
		return _collectUnits(runtime, perspective, true, line);
	}
	if (strcmp(name, "enemies") == 0) {
		return _collectUnits(runtime, perspective, false, line);
	}
	runtimeFail(runtime, "line %d: '%s' is not defined", line, name);
	return noneValue();
}
