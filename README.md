[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/releases)

[![✗](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml)

# Flex-Bison-Compiler

A compiler for a custom DSL that simulates turn-based RPG combat (units, abilities, teams, a battlefield and battles), developed in C with Flex and Bison. The current repository state corresponds to **Stage 2 (Frontend)**: lexical analysis, syntactic analysis and AST construction.

* [Stage 2 Scope](#stage-2-scope)
* [Language Overview](#language-overview)
* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [Tests](#tests)
* [Documentation](#documentation)
* [CI/CD](#cicd)
* [Notes](#notes)
* [Recommended Extensions](#recommended-extensions)

## Stage 2 Scope

This deliverable implements:

* lexical analysis with Flex
* syntactic analysis with Bison, with no shift/reduce or reduce/reduce conflicts
* AST construction for every accepted program
* accept/reject tests, one language feature per test

It does **not** implement yet:

* semantic analysis (for example: that the units a team lists were declared, that a battle has at least two teams, that positions fit inside the arena)
* the simulation itself, and any visual output of a battle

Because there is no semantic analysis yet, some semantically invalid programs are still accepted (for example, a team that lists an undeclared unit).

## Language Overview

A program is a sequence of declarations (units, abilities, turn behaviors, teams, an arena and a battle), in any order. Statements need no terminator.

```
arena (40, 20)

unit Hero at (0, 10) {
	health: 100,
	attack: 15,
	defense: 5,
	speed: 10,
	abilities: [Slash, Fireball]
}

unit Archer {
	health: 10, attack: 3, defense: 1, speed: 5
}

unit Goblin {
	health: 12, attack: 5, defense: 1, speed: 4
}

ability Slash on target: enemy {
	deal self.attack - target.defense to target
	log "Hero slashes {target} for {self.attack}"
}

ability Fireball on target: enemy {
	for (victim in radius 5 of target) {
		deal 2d6 to victim
	}
}

on turn Hero {
	if (nearest(enemy).health < 20) {
		move away from nearest(enemy)
	} else {
		move toward nearest(enemy)
		use Slash on nearest(enemy)
	}
}

party Heroes = [Hero, Archer * 20 around (5, 10)]
encounter Goblins = [Goblin * 2d6 around (35, 10)]

battle: [Heroes, Goblins]
```

| Construct  | Syntax                                                                                                     | Notes |
| :--------- | :--------------------------------------------------------------------------------------------------------- | :---- |
| Unit       | `unit <name> [at (<x>, <y>)] { <attribute>: <expr>, ... [abilities: [<ability>, ...]] }`                   | The attributes are `health`, `attack`, `defense` and `speed`. The unit's own `at` is its default position. |
| Ability    | `ability <name> on <param>[: <type>, ...] { <statements> }`                                                | `<param>` names the target inside the body (by convention, `target`). The optional types (`ally`, `enemy`, `self`) restrict which targets are valid. |
| Turn       | `on turn <unit> { <statements> }`                                                                          | What a unit does when its turn comes. |
| Team       | `party` / `encounter` `<name> = [<member>, ...]`                                                           | A member is `Hero`, `Hero at (x, y)`, `Archer * 20` or `Archer * 2d6 around (x, y)`. A member's position overrides the unit's own. |
| Arena      | `arena (<width>, <height>)`                                                                                | The size of the battlefield. |
| Battle     | `battle: [<team>, ...]`                                                                                    | Any number of teams. |
| Statements | `deal` / `heal <expr> to <expr>`, `use <ability> on <expr>`, `apply <effect> to <expr> for <expr>`, `move toward <expr>`, `move away from <expr>`, `log "<text>"`, `if (...) {...} [else {...}]`, `for (<id> in <expr>) {...}`, `while (...) {...}` | `log` text may hold `{...}` placeholders, to be filled in by the backend. |
| Expressions | `+ - * /`, `== != < > <= >=`, `and or not`, `a.b`, dice (`2d6`), `radius <r> of <center>`, calls (`nearest(enemy)`) | `radius 5 of target` is every unit within distance 5 of `target`. `=` is only used to name a team. |

Keywords are reserved, so words such as `at`, `of`, `radius`, `move` or `from` can't be used as unit, attribute or member names.

### How the language evolved since Stage I

The language changed while the frontend was being built, largely following the feedback on the Stage I design. Attributes are now spelled out (`health`, `attack`, `defense`, `speed`, instead of `hp`, `atk`, `def`, `spd`) and statements no longer end with `;`. An ability belongs to the units that list it in `abilities:`, and it can say what kind of target it accepts. A battle takes any number of teams (`battle: [A, B, C]`) instead of exactly two.

The biggest addition is space. Units can have positions, an `arena` sets the size of the field, units can `move`, and `radius 5 of target` selects everything around a point, which is what area-of-effect abilities need. A team can also hold a whole mass of identical units (`Archer * 20`), scattered `around` a point, next to individually placed heroes. The quantity can be random too (`Goblin * 2d6`).

What is still open for Stage III: effect definitions (what `apply Poison` actually does), variables and assignment, unit-type targeting, and ability costs and cooldowns.

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description |
| :-------------------- | :-----: | :---------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`. |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. The default is `ALL` under Docker Compose, and `INFORMATION` when the binary runs on its own. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml` file).

## Commands

### Start

Rises an ephemeral container, ready to start development:

```bash
docker compose run --rm compiler
```

### Build

Builds or rebuilds the entire compiler:

```bash
src/main/bash/build.sh
```

### Run

Compiles a program:

```bash
src/main/bash/run.sh <program>
```

where `<program>` is the path to the file that represents its entry-point. The exit status is `0` when the program is accepted and `1` when it is rejected, and a syntax error reports its line. To watch the AST being built, run with `LOGGING_LEVEL=DEBUGGING`: the semantic actions log themselves in the order they build it.

### Test

Executes every available test under `src/test/c` folder:

```bash
src/main/bash/test.sh
```

### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

## Tests

The tests under `src/test/c` only check syntax:

* `src/test/c/accept`: valid programs, which must be accepted.
* `src/test/c/reject`: invalid programs, which must be rejected.

Each test is as small as possible and covers a single feature, and a few of the rejections pin down restrictions that are intentional (for example, `around` only goes with a quantity, and `at` only without one). A few larger programs show the features working together. The build links AddressSanitizer, so every run of the suite also checks for memory errors and leaks.

## Documentation

`doc/Especificación.pdf` is the Stage I specification, kept as it was delivered. The language has evolved since (see [How the language evolved since Stage I](#how-the-language-evolved-since-stage-i)), so this README is the reference for the current syntax.

## CI/CD

_GitHub Actions_ is disabled by default on a new repository. To run the `pipeline.yaml` workflow (build and test on every push or pull request), turn it on under **Settings → Actions → General** and apply the following configuration:

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

## Notes

This repository is currently 3 commits behind `Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler:development` (the base template). We reviewed them: one commit (`694d39a`, 2026-09-26) independently fixes the same bug we found and fixed ourselves in `UnknownLexemeAction` (it wasn't calling `pushToken`, so an invalid lexeme could leave already-built AST fragments unreleased); the other two (`d2014d3`, `e329b9c`, 2026-09-09) are testing-script, CI and Docker/CMake portability improvements. We chose not to merge them at this stage to avoid integration risk close to the delivery; this can be revisited for Stage III.

## Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
