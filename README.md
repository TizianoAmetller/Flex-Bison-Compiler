[![✗](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml)

# Flex-Bison-Compiler

A compiler, written in C with Flex and Bison, for a DSL that describes turn-based RPG combat (units, abilities, effects, teams, an arena and a battle). It validates a program, simulates the battle it describes and writes it as text or as a self-contained HTML replay with metrics. No external libraries are used.

Group G-138: Alvaro Capridi (64.206) and Tiziano Ametller (65.004). The Stage III report is [`doc/Informe-Stage3.pdf`](doc/Informe-Stage3.pdf).

* [Quick start](#quick-start)
* [Pipeline](#pipeline)
* [Language](#language)
* [Semantic analysis](#semantic-analysis)
* [Simulation](#simulation)
* [HTML replay](#html-replay)
* [Examples](#examples)
* [Known limitations](#known-limitations)
* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [Tests](#tests)
* [Documentation](#documentation)
* [CI/CD](#cicd)
* [Notes](#notes)
* [Recommended Extensions](#recommended-extensions)

## Quick start

```bash
docker compose run --rm compiler  # open the container (the next lines run inside it)
src/main/bash/build.sh  # build the compiler
LOGGING_LEVEL=ERROR src/main/bash/run.sh examples/01-skirmish.rpg --seed 1  # a battle, as text
LOGGING_LEVEL=ERROR src/main/bash/run.sh examples/01-skirmish.rpg --seed 1 --format html > battle.html  # the replay: open it in a browser
src/main/bash/test.sh  # the tests
```

`LOGGING_LEVEL=ERROR` is there because under Docker Compose the default level is `ALL`, which writes thousands of debug lines to the standard error. The standard output is always reserved for the result.

## Pipeline

```
stdin -> lexer (Flex) -> parser (Bison) -> AST -> semantic analysis -> simulation -> trace -+-> text
                          |                        |                                        +-> HTML replay
                          +- --syntax-only stops   +- --check stops
```

The grammar is LALR(1) with no conflicts. The simulation records a trace of structured events, and both outputs are generated from the trace alone.

## Language

A program is a sequence of declarations, in any order. Statements need no terminator.

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
	health: 10, attack: 2, defense: 1, speed: 3, abilities: [Shoot]
}

unit Goblin {
	health: 20, attack: 6, defense: 2, speed: 4, abilities: [Stab]
}

ability Slash on target: enemy {
	log "{self} slashes {target}"
	deal self.attack - target.defense to target
	apply Poison to target for 3
}

ability Fireball on target: enemy {
	for (victim in radius 5 of target) {
		if (victim != self) {
			deal 2d6 to victim
		}
	}
}

ability Shoot on target: enemy {
	deal self.attack + 1d4 - target.defense to target
}

ability Stab on target: enemy {
	deal self.attack + 1d4 - target.defense to target
}

effect Poison on victim {
	deal 2 to victim
}

on turn Hero {
	if (self.health < 30) {
		move away from nearest(enemy)
	} else {
		move toward nearest(enemy)
		if (1d3 == 3) {
			use Fireball on nearest(enemy)
		} else {
			use Slash on nearest(enemy)
		}
	}
}

on turn Archer {
	use Shoot on nearest(enemy)
}

on turn Goblin {
	move toward nearest(enemy)
	use Stab on nearest(enemy)
}

party Heroes = [Hero, Archer * 20 around (5, 10)]
encounter Goblins = [Goblin * 2d6 around (35, 10)]

battle: [Heroes, Goblins]
```

This program is tested in `src/test/c/simulation/27-readme-example`.

| Construct | Syntax | Notes |
| :-------- | :----- | :---- |
| Unit | `unit <name> [: <tag>, ...] [at (<x>, <y>)] { health: <expr>, attack: ..., defense: ..., speed: ..., [abilities: [<ability>, ...]] }` | Tags are kinds of unit (`undead`). The unit's `at` is its default position. |
| Ability | `ability <name> on <param>[: <type>, ...] { <statements> }` | `<param>` names the target. The optional types restrict it: `ally`, `enemy`, `self` or a tag. |
| Effect | `effect <name> on <param> { <statements> }` | What `apply` does, every turn, to the unit that carries it. |
| Turn | `on turn <unit> { <statements> }` | What a unit does on its turn. |
| Team | `party` / `encounter` `<name> = [<member>, ...]` | A member is `Hero`, `Hero at (x, y)`, `Archer * 20` or `Archer * 2d6 around (x, y)`. |
| Arena, battle | `arena (<w>, <h>)`, `battle: [<team>, ...]` | A battle takes any number of teams. |
| Statements | `deal` / `heal <expr> to <expr>`, `use <ability> on <expr>`, `apply <effect> to <expr> for <expr>`, `move toward <expr>`, `move away from <expr>`, `log "<text>"`, `if` / `else`, `for (<id> in <expr>)`, `while` | `log` text may hold `{name}` and `{name.attribute}` placeholders. |
| Expressions | `+ - * /`, `== != < > <= >=`, `and or not`, `a.b`, dice (`2d6`), `radius <r> of <center>`, `nearest(ally)`, `nearest(enemy)` | Types: `int`, `bool`, `string`, `unit`, `units`. |

Keywords are reserved, identifiers are ASCII-only, and a number has at most 9 digits.

**How the language evolved since Stage I** (the specification in `doc/` is the Stage I one): attributes are spelled out (`health`, not `hp`), statements no longer end with `;`, an ability belongs to the units that list it and can restrict its targets, effects are declared with `effect`, and a battle takes any number of teams. The big addition is space: positions, `arena`, `move`, `radius ... of ...` for area abilities, and masses of units (`Archer * 20 around (5, 10)`, with random quantities too).

## Semantic analysis

It runs after the parser (skipped by `--syntax-only`) and reports every error, not just the first one, as `line N: <message>` on the standard error, up to 100. A program with any error is rejected with exit status `1`.

* **Structure.** Names are unique per kind and not reserved; units have exactly the four attributes with constant values; every referenced unit, ability and tag exists; a team member's quantity is greater than 0; there is at most one `battle` (with at least two distinct teams, and a unit in only one of them) and at most one `arena`; positions are constant and inside the arena. A program with no `battle` is valid, with a warning.
* **Bodies.** Abilities, effects and `on turn` are type-checked with scopes: `self`, the target parameter, `allies`, `enemies` and the variable of each `for`. Operators, statements and `log` placeholders are checked against the five types, and `use` inside `on turn` must name an ability that unit has.

The rules are specified by the `semantic-reject` fixtures: one mistake per file, with the exhaustive list of messages next to it.

## Simulation

A valid program with a `battle` is simulated and written to the standard output. The same program and seed always give the same battle, on any platform (the random numbers come from SplitMix64, written in the compiler). The seed is the first line of the output. This is the output of `src/test/c/html/01-one-on-one` with `--seed 1`:

```
Seed: 1
Arena: unbounded
Team Heroes: 1 unit
Team Goblins: 1 unit

Round 1
Hero uses Slash on Goblin
  Goblin takes 7 damage (health 5/12)
Goblin uses Slash on Hero
  Hero takes 3 damage (health 27/30)

Round 2
Hero uses Slash on Goblin
  Goblin takes 7 damage (health 0/12)
  Goblin dies

Battle over after 2 rounds.
Winner: Heroes
Team Heroes: 1 alive, 0 dead
Team Goblins: 0 alive, 1 dead
```

* **Turns.** Each round, the units alive act in order of speed (ties: creation order). A unit first lets its effects act, then runs its `on turn`. After every turn the battle ends if at most one team has units alive (a draw if none), or after `--max-rounds`, as a draw.
* **Targets.** `nearest` picks the closest ally or enemy by squared Euclidean distance; if there is none the statement is skipped. `radius r of c` is every unit alive within distance `r` of `c`. A dead unit leaves every collection.
* **Numbers.** 64 bits; an overflow or a division by zero stops the simulation (exit status `101`). `NdM` is the sum of `N` rolls from 1 to `M`. `and` / `or` short-circuit.
* **Damage and healing.** A unit never goes below 0 or above its initial health. Effects renew instead of stacking.

| Option | Meaning |
| :----- | :------ |
| `--seed N` | Seed (0 to 2^64 - 1). Without it, the `SEED` variable, else a random one. |
| `--max-rounds N` | Most rounds the battle lasts (1 to 1000000, 1000 by default). |
| `--format text\|html` | Output format (`text` by default). |
| `--check` | Parse and validate only. No output. |
| `--syntax-only` | Stop after the parser. |
| `--help`, `-h` | Usage on the standard error. |

The program is read from the standard input (`run.sh` does it for you). Exit status: `0` accepted and the battle ended (a draw is a way to end), `1` rejected, `2` out of memory, `3` the output could not be written, `100` invalid command line, `101` the simulation failed (the base project reserves the statuses below 100). A failed simulation still writes what happened until then. Logs go to the standard error.

| Limit | Value | When it is exceeded |
| :---- | :---- | :------------------ |
| Units in a battle | 10000 | The simulation fails |
| Dice in one roll | 100000 | The simulation fails |
| Events in the trace | 500000 | The simulation fails |
| Rounds | 1000 (`--max-rounds`) | The battle is a draw |
| Iterations of a `while` | 10000 | The simulation fails |
| `use` inside `use` | 32 | The simulation fails |
| Nested statements | 1000 | The simulation fails |
| Nesting of an expression | 2000 | The simulation fails |
| Nesting of an expression when the program is read | 10000 | The program is rejected (exit status 1) |
| Depth of the parser's stack (about 2000 nested blocks, 5000 members in a team, or 10000 statements in a block or declarations in a program) | 10000 | The program is rejected with `memory exhausted` (exit status 1) |

## HTML replay

`--format html` writes one HTML page to the standard output that replays the battle: the arena with each unit (color of its team, initial, health bar), a **Log** tab, and a **Metrics** tab (units alive and total health per round, damage by team, tables per team and per unit). Controls: play / pause (space), step (left / right), round (shift + left / right), speed and progress sliders.

* **Self-contained.** One file, no network access (no libraries, fonts or images), works offline. The same program and seed give the same page, byte for byte.
* **How it is made.** The page is a template (`src/main/resources/replay-template.html`) that the build embeds in the binary (`src/main/cmake/EmbedFile.cmake`). The compiler writes the battle into it as compact JSON (`<script id="battle-data">`). The page rebuilds the state at any event by applying the recorded values; it runs no rule of the game, and the metrics (effective damage and healing, kills, effect damage) are computed only from those values. This JavaScript player is the runtime of the final artifact.
* **Safety.** Everything the program writes (names, tags, `log` messages) is data: the generator escapes `<`, `>`, `&` and control characters, replaces invalid UTF-8, and the page declares a content security policy that allows no network and no scripts but its own.
* **Size.** About 80 KB plus 40 bytes per event; a battle of 10000 units and 125000 events loads in about 0.3 seconds (headless Chromium).

## Examples

| File | Teams | What it shows |
| :--- | :---- | :------------ |
| `examples/01-skirmish.rpg` | 2 teams, 6 against 6 | A readable small battle: healing, an area attack that also burns its friends, damage over time, units that back off when hurt. |
| `examples/02-three-armies.rpg` | 3 teams, 5 + 5 + 4 | All against all: abilities by tag, an effect that makes its victim flee, healing over time, `while`. |
| `examples/03-siege.rpg` | 2 teams, 40 against 60 | A big battle with groups scattered `around` points. Good to see the metrics with some volume. |

Each one ends with a winner in less than 80 rounds, for every seed from 1 to 20. Between them they show the main constructs of the language; the tests in `src/test/c` cover all of them.

## Known limitations

* Abilities have no range (`nearest` is chosen no matter how far it is), cost or cooldown, and the language has no variables.
* The cost grows with the square of the number of units: a battle of 5000 against 5000 takes about 11 seconds with the sanitizers on. There is no timeout on the clock.
* `deal` and `heal` log the amount evaluated, not the effective one (`heals 5 (health 10/10)` may have healed 3). The metrics do use the effective amounts.
* Numbers above 2^53 lose precision in the HTML replay (JavaScript has no exact integers beyond that).
* A byte order mark at the start of the file is not accepted, as any other non-ASCII character outside a string or comment.
* `--max-rounds` does not make room for events: a battle that writes more than 500000 events fails (exit status `101`) before its rounds run out.

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

| Name | Default | Description |
| :--- | :-----: | :---------- |
| `ENVIRONMENT` | `Local` | The active environment name: `Local`, `Development` or `Production`. |
| `LOG_IGNORED_LEXEMES` | `true` | When `true`, logs every ignored lexeme found by Flex at `DEBUGGING` level. |
| `LOGGING_LEVEL` | `ALL` | The minimum level to log on the standard error: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` or `CRITICAL`. `INFORMATION` when the binary runs on its own. |
| `SEED` | (empty) | Seed of the simulation when `--seed` is not given. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml`).

## Commands

```bash
docker compose run --rm compiler                           # start an ephemeral container, ready for development
src/main/bash/build.sh                                     # build (or rebuild) the compiler
LOGGING_LEVEL=ERROR src/main/bash/run.sh <program> [opts]  # compile a program; the options are the ones of Simulation
src/main/bash/test.sh                                      # run every test
exit                                                       # leave the container
docker compose down                                        # on the host: remove the containers and the network
```

## Tests

`test.sh` runs every program of the folders under `src/test/c` and the examples, requires the exact exit status, and fails if the build's sanitizers (AddressSanitizer and UndefinedBehaviorSanitizer, always linked in) report anything. Each fixture is as small as possible and covers a single feature.

| Folder | The compiler must... |
| :----- | :------------------- |
| `accept` | run the whole pipeline on the program with `--seed 1` and exit with `0` and no error. |
| `syntax-accept` | accept the program with `--syntax-only`. They are valid syntax only (for example, they use undeclared names to isolate one grammar feature), so they do not pass the semantic analysis. |
| `reject` | reject the program as a syntax error (exit status `1`). |
| `semantic-accept`, `semantic-reject` | accept (`0`) or reject (`1`) with `--check`. The `.stderr` file of a `semantic-reject` lists every message and is exhaustive. |
| `simulation`, `simulation-reject` | end with exactly the text of the `.stdout` file, or fail (`101`) with the messages of the `.stderr` file. A `.args` file adds options. |
| `html` | produce a complete page that loads nothing from outside and has exactly the data block of the `.data` file. |
| `examples/` | end with a winner, as text and as a page, for `--seed 1`. |

There are 202 tests. The same script runs in the CI.

## Documentation

* [`doc/Informe-Stage3.pdf`](doc/Informe-Stage3.pdf): the Stage III report (in Spanish).
* [`doc/Especificación.pdf`](doc/Especificación.pdf): the Stage I specification, kept as delivered. The language evolved since (see [Language](#language)); this README has the current syntax.

## CI/CD

_GitHub Actions_ is disabled by default on a new repository. To run the `pipeline.yaml` workflow (build and test on every push or pull request), turn it on under **Settings → Actions → General** and apply the following configuration:

| Key | Value |
| :-- | :---- |
| `Actions permissions` | `Allow all actions and reusable workflows` |
| `Allow GitHub Actions to create and approve pull requests` | `false` |
| `Artifact and log retention` | `30 days` |
| `Fork pull request workflows from outside collaborators` | `Require approval for all outside collaborators` |
| `Workflow permissions` | `Read repository contents and packages permissions` |

## Notes

The base template (`Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler:development`) has 5 commits that this repository does not merge. Two of them rework the template's own arithmetic frontend (`15ef912`, `694d39a`), which this project replaced; one adds a GDB setup (`7edbac8`); one adapts its CI to older runners (`e329b9c`); and one updates its test script, CMake and Docker image (`d2014d3`). `694d39a` also fixes an `UnknownLexemeAction` leak that we had already fixed, and our Docker image is Ubuntu 24.04 LTS, the same system as the CI (the template moved to Ubuntu 26.04).

## Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
