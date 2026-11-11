#include "SemanticModel.h"
#include <stdlib.h>
#include <string.h>

/* PRIVATE FUNCTIONS */

static bool _appendDeclaration(DeclarationArray * array, const Declaration * declaration);
static const Declaration * _findDeclaration(const DeclarationArray * array, const char * name);
static const char * _nameOf(const Declaration * declaration);

/**
 * Appends a declaration to the array, doubling its capacity when it is full.
 */
static bool _appendDeclaration(DeclarationArray * array, const Declaration * declaration) {
	if (array->count == array->capacity) {
		const size_t capacity = (array->capacity == 0) ? 8 : (2 * array->capacity);
		const Declaration ** declarations = realloc((void *) array->declarations, capacity * sizeof(const Declaration *));
		if (declarations == NULL) {
			return false;
		}
		array->declarations = declarations;
		array->capacity = capacity;
	}
	array->declarations[array->count] = declaration;
	++array->count;
	return true;
}

/**
 * Returns the first declaration in the array with that name, or NULL.
 */
static const Declaration * _findDeclaration(const DeclarationArray * array, const char * name) {
	for (size_t k = 0; k < array->count; ++k) {
		if (strcmp(_nameOf(array->declarations[k]), name) == 0) {
			return array->declarations[k];
		}
	}
	return NULL;
}

/**
 * The name that identifies a declaration among those of its kind: its own name
 * or, for an "on turn" declaration, the name of the unit it belongs to. The
 * kinds that have no name (arena and battle) never reach here.
 */
static const char * _nameOf(const Declaration * declaration) {
	switch (declaration->type) {
		case ABILITY_DECLARATION:
			return declaration->abilityDeclaration->name;
		case EFFECT_DECLARATION:
			return declaration->effectDeclaration->name;
		case TEAM_DECLARATION:
			return declaration->teamDeclaration->name;
		case TURN_DECLARATION:
			return declaration->turnDeclaration->unitName;
		case UNIT_DECLARATION:
			return declaration->unitDeclaration->name;
		case ARENA_DECLARATION:
		case BATTLE_DECLARATION:
			break;
	}
	return "";
}

/* PUBLIC FUNCTIONS */

SemanticModel * createSemanticModel(void) {
	return calloc(1, sizeof(SemanticModel));
}

void destroySemanticModel(SemanticModel * semanticModel) {
	if (semanticModel != NULL) {
		free((void *) semanticModel->units.declarations);
		free((void *) semanticModel->abilities.declarations);
		free((void *) semanticModel->effects.declarations);
		free((void *) semanticModel->teams.declarations);
		free((void *) semanticModel->turns.declarations);
		free((void *) semanticModel->tags.names);
		free(semanticModel);
	}
}

bool addUnit(SemanticModel * semanticModel, const Declaration * declaration) {
	return _appendDeclaration(&semanticModel->units, declaration);
}

bool addAbility(SemanticModel * semanticModel, const Declaration * declaration) {
	return _appendDeclaration(&semanticModel->abilities, declaration);
}

bool addEffect(SemanticModel * semanticModel, const Declaration * declaration) {
	return _appendDeclaration(&semanticModel->effects, declaration);
}

bool addTeam(SemanticModel * semanticModel, const Declaration * declaration) {
	return _appendDeclaration(&semanticModel->teams, declaration);
}

bool addTurn(SemanticModel * semanticModel, const Declaration * declaration) {
	return _appendDeclaration(&semanticModel->turns, declaration);
}

bool addTag(SemanticModel * semanticModel, const char * tag) {
	if (hasTag(semanticModel, tag)) {
		return true;
	}
	NameArray * tags = &semanticModel->tags;
	if (tags->count == tags->capacity) {
		const size_t capacity = (tags->capacity == 0) ? 8 : (2 * tags->capacity);
		const char ** names = realloc((void *) tags->names, capacity * sizeof(const char *));
		if (names == NULL) {
			return false;
		}
		tags->names = names;
		tags->capacity = capacity;
	}
	tags->names[tags->count] = tag;
	++tags->count;
	return true;
}

const Declaration * findUnit(const SemanticModel * semanticModel, const char * name) {
	return _findDeclaration(&semanticModel->units, name);
}

const Declaration * findAbility(const SemanticModel * semanticModel, const char * name) {
	return _findDeclaration(&semanticModel->abilities, name);
}

const Declaration * findEffect(const SemanticModel * semanticModel, const char * name) {
	return _findDeclaration(&semanticModel->effects, name);
}

const Declaration * findTeam(const SemanticModel * semanticModel, const char * name) {
	return _findDeclaration(&semanticModel->teams, name);
}

const Declaration * findTurn(const SemanticModel * semanticModel, const char * unitName) {
	return _findDeclaration(&semanticModel->turns, unitName);
}

bool hasTag(const SemanticModel * semanticModel, const char * tag) {
	for (size_t k = 0; k < semanticModel->tags.count; ++k) {
		if (strcmp(semanticModel->tags.names[k], tag) == 0) {
			return true;
		}
	}
	return false;
}
