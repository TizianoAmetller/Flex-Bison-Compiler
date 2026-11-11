#ifndef SEMANTIC_MODEL_HEADER
#define SEMANTIC_MODEL_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct SemanticModel SemanticModel;

/**
 * A growable array of borrowed pointers to declarations of the AST. The model
 * owns the array itself, but never the declarations: the AST does.
 */
typedef struct {
	const Declaration ** declarations;
	size_t count;
	size_t capacity;
} DeclarationArray;

/**
 * A growable array of borrowed pointers to names (the strings of the AST).
 * The model owns the array itself, but never the strings.
 */
typedef struct {
	const char ** names;
	size_t count;
	size_t capacity;
} NameArray;

/**
 * What the semantic analysis knows about a program: its declarations, by
 * kind, as pointers into the AST (so the AST must outlive the model, and the
 * model must be destroyed before it), and the set of tags the units declare.
 * The declarations of a kind keep the order of the source, and only the first
 * of those with the same name is added to the kinds that have unique names.
 */
struct SemanticModel {
	DeclarationArray units;
	DeclarationArray abilities;
	DeclarationArray effects;
	DeclarationArray teams;
	/** Every "on turn" declaration (a unit should have at most one). */
	DeclarationArray turns;
	/** The only arena declaration, or NULL when the program has none. */
	const Declaration * arena;
	/** The only battle declaration, or NULL when the program has none. */
	const Declaration * battle;
	/** The tags that at least one unit declares, with no repeated names. */
	NameArray tags;
};

/** Creates an empty model, or returns NULL if it runs out of memory. */
SemanticModel * createSemanticModel(void);

/** Destroys a model and its arrays (but not what they point to). NULL is fine. */
void destroySemanticModel(SemanticModel * semanticModel);

/** Each of these returns false only when it runs out of memory. */
bool addUnit(SemanticModel * semanticModel, const Declaration * declaration);
bool addAbility(SemanticModel * semanticModel, const Declaration * declaration);
bool addEffect(SemanticModel * semanticModel, const Declaration * declaration);
bool addTeam(SemanticModel * semanticModel, const Declaration * declaration);
bool addTurn(SemanticModel * semanticModel, const Declaration * declaration);

/** Adds a tag to the set, unless it is already there. Returns false only when it runs out of memory. */
bool addTag(SemanticModel * semanticModel, const char * tag);

/** Each of these returns the first declaration with that name, or NULL. */
const Declaration * findUnit(const SemanticModel * semanticModel, const char * name);
const Declaration * findAbility(const SemanticModel * semanticModel, const char * name);
const Declaration * findEffect(const SemanticModel * semanticModel, const char * name);
const Declaration * findTeam(const SemanticModel * semanticModel, const char * name);

/** Returns the first "on turn" declaration of a unit, by the unit's name, or NULL. */
const Declaration * findTurn(const SemanticModel * semanticModel, const char * unitName);

/** Whether some unit declares the tag. */
bool hasTag(const SemanticModel * semanticModel, const char * tag);

#endif
