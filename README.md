[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/releases)

[![✗](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml)

# Flex-Bison-Compiler

A compiler for a custom DSL for RPG combat simulation (units, abilities, turns and battles), developed in C with Flex and Bison. The current repository state corresponds to Stage 2: lexical analysis, syntactic analysis and AST construction only.

* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [CI/CD](#cicd)
* [Language](#language)
  * [Syntax overview](#syntax-overview)
  * [From Stage I to Stage II](#from-stage-i-to-stage-ii)
  * [QRF feedback resolution](#qrf-feedback-resolution)
  * [Deferred to Stage III](#deferred-to-stage-iii)
* [Notes](#notes)
* [Recommended Extensions](#recommended-extensions)

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description                                                                                                                                                           |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`.                                                                 |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. To remove those logs from the console output set it to `false`.                    |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. |

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

where `<program>` is the path to the file that represents its entry-point.

### Test

Executes every available unit-test under `src/test/c` folder:

```bash
src/main/bash/test.sh
```

### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

### Docker

| Command                                 | Description                                             |
| :-------------------------------------- | :------------------------------------------------------ |
| `docker builder prune --all`            | Removes all builds and complete build cache.            |
| `docker compose --progress=plain build` | Forces a build or rebuild of the images in the cluster. |
| `docker image prune`                    | Removes all of the dangling images from Docker.         |
| `docker network prune`                  | Removes unused networks from Docker.                    |
| `docker volume prune`                   | Removes unused volumes from Docker.                     |

## CI/CD

_GitHub Actions_ is disabled by default on a new repository, so the `pipeline.yaml` workflow (build + test on every push or PR) won't run until it's turned on manually:

1. Open the repository on GitHub and go to the **Settings** tab.
2. In the left sidebar, under **Code and automation**, click **Actions → General**.
3. Under **Actions permissions**, select **Allow all actions and reusable workflows**.
4. Apply the rest of the configuration below, then click **Save**.

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

## Language

The DSL simulates turn-based RPG combat: units with stats and abilities, grouped into teams (`party`/`encounter`), that fight a `battle`. Stage II covers the lexer, the grammar and the AST only (§4 of the assignment's "Proyecto Especial" PDF); there is no semantic analysis and no backend yet, so nothing below is actually *evaluated* -- the grammar only has to be able to *represent* it. That part is Stage III's.

### Syntax overview

```
unit Hero at (0, 0) {
	health: 100,
	attack: 15,
	defense: 5,
	speed: 10,
	abilities: [Slash, Heal]
}

unit Archer {
	health: 10, attack: 3, defense: 1, speed: 5
}

ability Slash on target: enemy {
	deal self.attack - target.defense to target
	log "Hero slashes {target} for {self.attack}"
}

ability Fireball on target {
	for (victim in radius 5 of target) {
		deal 2d6 to victim
	}
}

on turn Hero {
	if (nearest(enemy).health < 20) {
		use Heal on self
	} else {
		use Slash on nearest(enemy)
	}
}

party Heroes = [Hero, Archer * 20]
encounter Goblins = [Goblin]

battle: [Heroes, Goblins]
```

| Construct | Syntax | Notes |
| :-------- | :----- | :---- |
| Unit | `unit <name> [at (<x>, <y>)] { <attributes> [abilities: [<id>, ...]] }` | Attributes are `health`, `attack`, `defense`, `speed`: expressions, not fixed literals. |
| Ability | `ability <name> on <target>[: <type>, ...] { <statements> }` | `target` is a bound parameter name, not a keyword; the optional clause restricts it to `ally`/`enemy`/`self` (or any combination). |
| Turn | `on turn <unitName> { <statements> }` | What a unit does on its turn. |
| Team | `party <name> = [<member>, ...]` / `encounter <name> = [<member>, ...]` | A member is either a named unit (`Hero`) or a declared unit repeated N times (`Archer * 20`, or `Archer * 2d6` for a randomized count). |
| Battle | `battle: [<team>, ...]` | Any number of teams, not just two. |
| Statements | `deal`/`heal <expr> to <expr>`, `use <ability> on <expr>`, `apply <effect> to <expr> for <expr>`, `log <string>`, `if (...) {...} [else {...}]`, `for (<id> in <expr>) {...}`, `while (<expr>) {...}` | No `;` terminator; no bare-expression statement (every action has its own keyword). |
| Expressions | `+ - * /`, `== != < > <= >=`, `and or not`, `.` (member access), dice (`2d6`), `radius <r> of <center>` (spatial AoE), calls (`nearest(enemy)`, `all(allies)`), string interpolation (`"{target} healed"`) | `=` is reserved for team assignment; comparisons use `==`/`!=`, never `=`. |

### From Stage I to Stage II

The attribute names from the Stage I specification were renamed to remove abbreviations (QRF feedback point 4, below): `hp` → `health`, `atk` → `attack`, `def` → `defense`, `spd` → `speed`. `doc/Especificación.pdf` is kept as-is from the Stage I delivery (a frozen snapshot, including the old attribute names and the Stage I delivery date) rather than rewritten to match the current grammar; this section of the README, not that PDF, is the up-to-date reference for Stage II's actual syntax.

### QRF feedback resolution

The table below goes through the QRF's Stage I feedback (16 numbered points) and says what Stage II did about each one. Points about the Stage I *document itself* (wrong date, broken table of contents, mixed numbering, wording suggestions) aren't a frontend concern and are left for the Stage III report.

| # | Feedback (summarized) | Resolution |
| :-: | :--------------------- | :--------- |
| 1 | Wrong report date. | N/A -- Stage I document issue. |
| 2 | Table of contents doesn't render. | N/A -- Stage I document issue. |
| 3 | Output is just a console log; wants some visual/dashboard representation. | Deferred: Stage II is frontend-only (no execution, so nothing to visualize yet); revisit once Stage III adds a backend. |
| 4 | Don't abbreviate keywords (`spd`, `atk`, ...). | **Resolved.** `hp`→`health`, `atk`→`attack`, `def`→`defense`, `spd`→`speed`. |
| 5 | Constructs, semantic restrictions and the resolution model are numbered as one mixed list. | N/A -- Stage I document issue. |
| 6 | No way to express a large, unnamed quantity of identical units (e.g. "20 archers"). | **Resolved.** `Archer * 20` (or `Archer * 2d6`) as a team member. |
| 7 | Target-individual abilities are too limiting; wants spatial AoE. | **Resolved** (syntax only). `radius <r> of <center>` expression, e.g. `for (victim in radius 5 of target) { ... }`; actually computing the collection from positions is Stage III. |
| 8 | "Etiqueta" (tag) is used once, undefined. | Deferred to Stage III/Package B (a `tags: [...]` clause was designed but not implemented this delivery). |
| 9 | Rename "modelo de resolución" to "mecanismo de evolución". | N/A -- wording suggestion for the report. |
| 10 | Simulation looks deterministic; wants randomness in the evolution mechanism. | Partially addressed: dice notation (`2d6`) is randomness at the expression level; whether/how it drives turn resolution is Stage III semantics. |
| 11 | Unclear which unit owns which ability. | **Resolved.** `abilities: [Slash, Heal]` clause on `unit`. |
| 12 | Wants typed targets (resistances, querying a target's type). | Partially addressed: `on target: ally, enemy, self` gives *relational* typing; unit-type-based targeting is deferred to Stage III/Package B. |
| 13 | `battle X vs Y` hardcodes exactly two teams. | **Resolved.** `battle: [Team1, Team2, ...]`, any number of teams. |
| 14 | No target-selection mechanism; positioning should consider space, not just speed (implicit/explicit/hybrid). | Partially addressed: explicit `at (x, y)` position and the `radius ... of ...` spatial expression exist; implicit/hybrid mass positioning (formations), movement and the actual selection mechanism are deferred to Stage III/Package B. |
| 15 | Don't require `;` between statements. | **Resolved.** No statement terminator. |
| 16 | Support string interpolation in `log`. | **Resolved.** `log "Hero slashes {target} for {self.attack}"`. |

### Deferred to Stage III

Scoped out of this delivery on purpose, consistent with "Stage II should have the desired/ideal syntax; Stage III only needs to implement the essential part" (assignment FAQ): semantic analysis and the backend in general; formations and implicit/hybrid positioning for mass units; unit movement and an arena/terrain size; `effect` definitions (what `Poison` actually does); ability cost/cooldown/range; unit-type tags and type-based targeting; variables and assignment; and any visual/dashboard output.

## Notes

This repository is currently 3 commits behind `Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler:development`
(the base template). We reviewed the diff: one commit (`694d39a`, 2026-09-26) independently fixes
the same bug we found and fixed ourselves in `UnknownLexemeAction` (it wasn't calling `pushToken`,
so an invalid lexeme could leave already-built AST fragments unreleased); the other two (`d2014d3`,
`e329b9c`, 2026-09-09) are testing-script, CI and Docker/CMake portability improvements, not
functional for this delivery. We chose not to merge them at this stage to avoid unnecessary
integration risk right before announcing a delivery commit hash; this can be revisited for Stage III.

## Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
