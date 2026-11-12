[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/releases)

[![✗](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/TizianoAmetller/Flex-Bison-Compiler/actions/workflows/pipeline.yaml)

# Flex-Bison-Compiler

A compiler for a custom DSL that simulates turn-based RPG combat (units, abilities, teams, a battlefield and battles), developed in C with Flex and Bison. The project now covers the whole pipeline: lexical and syntactic analysis, semantic analysis, simulation of the battles, and generation of their text and of an HTML replay with metrics.

* [Quick start](#quick-start)
* [Stage 2 Scope](#stage-2-scope)
* [Language Overview](#language-overview)
* [Semantic analysis](#semantic-analysis)
* [Simulation](#simulation)
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
LOGGING_LEVEL=ERROR src/main/bash/run.sh examples/01-skirmish.rpg --seed 1 --format html > battle.html  # the replay: open battle.html with a double click
src/main/bash/test.sh  # the tests
```

`LOGGING_LEVEL=ERROR` is there because under Docker Compose the default level is `ALL`, which writes thousands of debug lines to the standard error (the standard output stays clean either way).

## Stage 2 Scope

This deliverable implements:

* lexical analysis with Flex
* syntactic analysis with Bison, with no shift/reduce or reduce/reduce conflicts
* AST construction for every accepted program
* accept/reject tests, one language feature per test

The semantic analysis checks the structure of a program (for example: that the units a team lists were declared, that a battle has at least two teams, that positions fit inside the arena) and the bodies of its abilities, effects and `on turn` declarations. It is described in [Semantic analysis](#semantic-analysis).

The battle of a valid program is simulated, and the text that tells it is written to the standard output (see [Simulation](#simulation)), or a page that replays it in a browser, with metrics (see [HTML replay](#html-replay) and [Metrics](#metrics)). Three programs to try are in [Examples](#examples).

## Language Overview

A program is a sequence of declarations (units, abilities, effects, turn behaviors, teams, an arena and a battle), in any order. Statements need no terminator.

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
	apply Poison to target for 3
}

ability Fireball on target: enemy {
	for (victim in radius 5 of target) {
		deal 2d6 to victim
	}
}

effect Poison on victim {
	deal 2 to victim
}

on turn Hero {
	if (self.health < 30) {
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

This program is tested in `src/test/c/simulation/27-readme-example`.

| Construct  | Syntax                                                                                                     | Notes |
| :--------- | :--------------------------------------------------------------------------------------------------------- | :---- |
| Unit       | `unit <name> [: <tag>, ...] [at (<x>, <y>)] { <attribute>: <expr>, ... [abilities: [<ability>, ...]] }`    | The attributes are `health`, `attack`, `defense` and `speed`. The tags are the kinds of unit it is (for example `undead`). The unit's own `at` is its default position. |
| Ability    | `ability <name> on <param>[: <type>, ...] { <statements> }`                                                | `<param>` names the target inside the body (by convention, `target`). The optional types restrict which targets are valid: a relation (`ally`, `enemy`, `self`) or a tag that units declare (`undead`). |
| Effect     | `effect <name> on <param> { <statements> }`                                                                | What `apply <name> ...` does to the unit carrying the effect. `<param>` names that unit inside the body. |
| Turn       | `on turn <unit> { <statements> }`                                                                          | What a unit does when its turn comes. |
| Team       | `party` / `encounter` `<name> = [<member>, ...]`                                                           | A member is `Hero`, `Hero at (x, y)`, `Archer * 20` or `Archer * 2d6 around (x, y)`. A member's position overrides the unit's own. |
| Arena      | `arena (<width>, <height>)`                                                                                | The size of the battlefield. |
| Battle     | `battle: [<team>, ...]`                                                                                    | Any number of teams. |
| Statements | `deal` / `heal <expr> to <expr>`, `use <ability> on <expr>`, `apply <effect> to <expr> for <expr>`, `move toward <expr>`, `move away from <expr>`, `log "<text>"`, `if (...) {...} [else {...}]`, `for (<id> in <expr>) {...}`, `while (...) {...}` | `log` text may hold `{...}` placeholders, to be filled in by the backend. |
| Expressions | `+ - * /`, `== != < > <= >=`, `and or not`, `a.b`, dice (`2d6`), `radius <r> of <center>`, calls (`nearest(enemy)`) | `radius 5 of target` is every unit within distance 5 of `target`. `=` is only used to name a team. |

Keywords are reserved, so words such as `at`, `of`, `radius`, `move` or `from` can't be used as unit, attribute or member names. Identifiers are ASCII-only, and a number has at most 9 digits (each side of a dice literal included).

### How the language evolved since Stage I

The language changed while the frontend was being built, largely following the feedback on the Stage I design. Attributes are now spelled out (`health`, `attack`, `defense`, `speed`, instead of `hp`, `atk`, `def`, `spd`) and statements no longer end with `;`. An ability belongs to the units that list it in `abilities:`, and it can say what kind of target it accepts, either a relation (`ally`, `enemy`, `self`) or a tag that units declare (`unit Skeleton: undead`). The effects an ability applies are defined with `effect`. A battle takes any number of teams (`battle: [A, B, C]`) instead of exactly two.

The biggest addition is space. Units can have positions, an `arena` sets the size of the field, units can `move`, and `radius 5 of target` selects everything around a point, which is what area-of-effect abilities need. A team can also hold a whole mass of identical units (`Archer * 20`), scattered `around` a point, next to individually placed heroes. The quantity can be random too (`Goblin * 2d6`).

What is still open for Stage III: variables and assignment, and ability costs and cooldowns.

## Semantic analysis

Once a program is accepted by the syntactic analysis, the compiler checks it (this is skipped with `--syntax-only`). The declarations can come in any order. Every error found is reported, not just the first one, as `line N: <message>` on the standard error, quoting the offending name between single quotes, and a program with any error is rejected with exit status `1`. It stops reporting after 100 errors.

What it validates:

* **Names.** Units, abilities, effects and teams can't repeat a name within their kind. A unit, ability, effect, team or tag can't be named after a reserved word: `self`, `allies`, `enemies`, `ally`, `enemy`, `nearest` or `all`.
* **Units.** Exactly the attributes `health`, `attack`, `defense` and `speed`, each once, with a constant `int` value. Every ability in `abilities:` exists, and neither an ability nor a tag is listed twice.
* **Abilities.** Each type of target (after `on x:`) is `ally`, `enemy`, `self` or a tag that some unit declares, with no repeats.
* **`on turn`.** The unit exists, and has at most one.
* **Teams.** Every member is a declared unit, and a team can have several members of the same unit (for example, two groups of archers around different points). A constant quantity must be greater than 0.
* **Battle.** At most one, with at least two teams that exist and are not repeated. A unit declaration can be in only one of its teams, so a mirror match needs a copy of the unit (`unit EnemyArcher { ... }`).
* **Arena.** At most one, with a constant width and height greater than 0 and no dice.
* **Positions** (of units and of team members) are constant, and inside the arena when there is one (`0 <= x < width` and `0 <= y < height`).

A constant expression is made of `int` literals, dice and `+ - * /`, computed with 64 bits, and an overflow or a division by zero is an error. Anything else in those places is rejected. Dice are accepted, but they have no value to check yet, so a quantity or a position with dice is not range-checked.

A program with no `battle` is valid: it exits with `0` and warns "no battle declared: nothing to simulate".

### Bodies

The bodies of abilities, effects and `on turn` declarations are checked too. A name must be visible where it is used:

| Body              | Names it sees |
| :---------------- | :------------ |
| `ability A on p`  | `self` (the unit that uses it), `p` (its target), and `allies` and `enemies` (units) |
| `effect E on p`   | `p` (the unit that carries it), and `allies` and `enemies` (units, seen from `p`). There is no `self`. |
| `on turn U`       | `self` (the unit), and `allies` and `enemies` |

A `for (x in c)` adds `x`, a unit, inside its block. The blocks of `if`, `else` and `while` open no scope. The target parameter and the iterator can't be reserved names, and an iterator can't take a name that is already visible.

An expression has one of five types: `int` (a dice roll is an `int` that is rolled every time it is evaluated), `bool`, `string`, `unit` and `units`. `unit.health` (and `attack`, `defense` and `speed`) is an `int`. The only functions are `nearest(ally)` and `nearest(enemy)`, which give a unit, and `all(units)`, which gives units. `radius r of c` needs an `int` and a unit, and gives units. `ally` and `enemy` can only be the argument of `nearest`.

* `+ - * /` take two `int`s and give an `int`. A divisor that is the constant 0 is an error, and integer division truncates toward zero.
* `< > <= >=` take two `int`s and give a `bool`.
* `==` and `!=` compare two values of the same type, which can be `int`, `bool`, `string` or `unit` (but not `units`), and give a `bool`.
* `and`, `or` and `not` take `bool`s and give a `bool`.

An expression with an error is reported once: it has no type, so the expressions around it say nothing more about it.

The statements need:

* `deal` and `heal`: an `int` amount, and a unit or units as the target.
* `use A on t`: a declared ability, and a unit `t`. In an `on turn U`, `A` must also be one of the `abilities` of `U`.
* `apply E to t for n`: a declared effect, a unit or units, and an `int`.
* `move toward t` and `move away from t`: a unit.
* `if` and `while`: a `bool`. `for`: units.

The message of a `log` can have placeholders, written `{name}` or `{name.attribute}`: `name` is written like an identifier and `attribute` is one of `health`, `attack`, `defense` and `speed`. The name must be visible and can't be units, and with an attribute it must be a unit. There is no way to write a literal brace: every `{` opens a placeholder and every `}` closes one, so an unclosed `{`, an unmatched `}`, an empty `{}` and anything else between braces are errors.

### Tests

The tests have two categories of their own under `src/test/c`, and both run the compiler up to the end of the semantic analysis (`--check`):

* `semantic-accept`: valid programs, which must be accepted with exit status `0` and no error. A `NN-name.stderr` file next to a program lists messages that must also be reported (for example, the "no battle" warning).
* `semantic-reject`: programs with a single mistake each (except `29-several-errors` and `74-several-body-errors`), which must be rejected with exit status `1`. The `NN-name.stderr` file lists every message, and it is exhaustive: each line must be in the standard error, and the analyzer can report no other.

## Simulation

When a program passes the semantic analysis and declares a `battle`, the compiler simulates it and writes what happens to the standard output, as text, or as a page that replays the battle in a browser with `--format html` (see [HTML replay](#html-replay)). The same program with the same seed always gives the same battle, on any platform. The compiler does not need anything else to play it: the simulation records a trace of structured events, and both outputs are made from the trace alone.

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

The consequences of an action are indented under it. A program that has no `battle` is only validated. The output above is what `LOGGING_LEVEL=ERROR src/main/bash/run.sh src/test/c/html/01-one-on-one --seed 1` writes.

### The model

| Subject | What happens |
| :------ | :----------- |
| Units | They are created in the order of the `battle`: by team, then by member, then by instance. A member's quantity is evaluated once (no quantity is one instance, and 0 or less is none), and the attributes of every instance are rolled on their own, in the order they are written, with no attribute under 0 and a health of at least 1 (that health is also the most it can have). A unit is named `Archer` when its declaration has a single instance in the battle, and `Archer#1`, `Archer#2`... if it has more. |
| Positions | The member's, else the unit's, else the center of the arena (or the origin without one). It is exact for one instance, and for a quantity the instances are scattered around it, up to the square root of their number (rounded up) on each axis. Everything is kept inside the arena. |
| Collections | From the point of view of the unit that acts (in an effect, the unit that carries it), `allies` are the units alive of its team other than itself and `enemies` those of the other teams, always in the order of creation. A unit that dies leaves every collection. |
| `nearest` | The closest unit alive that is an ally (or an enemy), by the square of the Euclidean distance, and the first one created if there is a tie. If there is none the value is _nothing_, which spreads to everything that uses it, and the statement that needs it is skipped (`no unit found`). |
| `radius r of c` | The units alive of any team, `c` included, at a distance of `r` or less from `c`. A radius of 0 or less is just the cell of `c`. |
| Numbers | They have 64 bits. An overflow or a division by 0 stops the simulation, and a division truncates toward 0. `NdM` is the sum of `N` rolls between 1 and `M`, and it is 0 if `N` or `M` is 0. `and` and `or` short-circuit: the left side is evaluated first, and the right side only if the left one does not decide the result (`false and ...` is false and `true or ...` is true). If the left side is _nothing_, so is the result and the right side is not evaluated; otherwise the result is the right side. |
| Rounds | In each, the units alive go in order of speed (the first one created first if they are tied). On its turn a unit first has its effects act, each one in the order it was applied, and then, if it is alive, runs its `on turn`. After every turn the battle ends if at most one team has units alive: that one wins, or it is a draw if none is left. If the rounds run out it is a draw too. |
| `deal` and `heal` | The amount is evaluated once for each target, and a negative one is 0. A unit never has less than 0 health or more than its health at the start, and at 0 it dies. The events say the amount that the statement asked for. |
| `apply` | The effect starts to act on each target that is alive. If it was already acting, its turns are set again, and they never add up. Each time the unit has a turn the effect acts, and its turns go down by one until it wears off. |
| Dead targets | `deal`, `heal`, `apply` and `move` aimed at a single unit that is dead (for example, one that died earlier in the same body) do nothing, and nothing is told. A collection never has the dead, so it needs no rule. A `nearest` that finds no one is still skipped with `no unit found`, and only `use` tells that its target is dead (`target is dead`). |
| `use` | It is skipped, and the skip is told, if the unit does not have the ability in its `abilities` (`not in its abilities`), if the target does not fit the types of the ability (`invalid target type`; `self`, `ally` and `enemy` are relative to the unit, and the tags are those of the target) or if it is dead (`target is dead`). Otherwise the body runs, with `self` as the unit and the parameter as the target. |
| `move` | Toward a unit, the mover goes as many cells as its speed along each axis, without getting past the cell next to the target. Away from it, the mover goes as many cells as its speed in the other direction; when it is in the same cell as the target there is no other direction, and it goes toward +x. The result is kept inside the arena. A move that leaves the mover where it was (it is next to the target already, or it is against the edge of the arena) does nothing, and nothing is told. |
| `for` and `while` | A `for` goes over a copy of the collection and leaves out the units that die on the way. A `while` stops the simulation after 10000 iterations. |
| `log` | The unit is written by its name, an `int` in decimal, a `bool` as `true` or `false` and a string as it is. |

The random numbers come from SplitMix64, written in the compiler (not from the C library). They are used in this order: for each member, its quantity; for each instance, its attributes and then, if there is a quantity, the two offsets of its position; and then the dice of the battle as they are evaluated.

### Options

| Option            | Meaning |
| :---------------- | :------ |
| `--seed N`        | The seed of the random numbers, a number from 0 to 18446744073709551615. If there is none, the `SEED` environment variable is used (if it is not empty), and else a random one. The seed is always the first line of the output, so any battle can be repeated. |
| `--max-rounds N`  | The most rounds that the battle lasts, from 1 to 1000000 (1000 by default). |
| `--format F`      | The output on the standard output: `text` (the default) or `html` (see [HTML replay](#html-replay)). |
| `--check`         | Parses and validates the program and does nothing else (it generates no output, whatever the format). |
| `--syntax-only`   | Stops after the syntactic analysis. |

A value that is missing or not valid is a usage error (exit status 100). The exit status is `0` when the program is accepted and the battle ends (a draw is a way to end), `1` when the program is rejected, `100` when the command line is invalid and `101` when the simulation fails. A failed simulation reports `simulation error: ...` on the standard error and still writes what happened until then, in the format that was asked for (in `html`, the page also has the error, in a red banner). The logs never go to the standard output.

### Limits

| Limit | Value | When it is exceeded |
| :---- | :---- | :------------------ |
| Units in a battle | 10000 | The simulation fails |
| Dice in one roll (`100000d6`) | 100000 | The simulation fails |
| Events in the trace | 500000 | The simulation fails |
| Rounds | 1000 (`--max-rounds`) | The battle is a draw |
| Iterations of a `while` | 10000 | The simulation fails |
| `use` inside `use` | 32 | The simulation fails |
| Statements nested (`if`, `while`, `for` and bodies inside one another, counted through every `use` in the chain) | 1000 | The simulation fails |
| Nesting of an expression | 2000 | The simulation fails |

A team that has no units when the battle starts (for example, a quantity that rolls 0) also fails the simulation.

### HTML replay

With `--format html` the compiler writes a single HTML page to the standard output that replays the battle:

```bash
LOGGING_LEVEL=ERROR src/main/bash/run.sh battle.rpg --format html --seed 1 > battle.html
```

Run under Docker Compose, the file lands in the folder of the repository, and it opens with a double click. The page has no other file and loads nothing from anywhere (no libraries, fonts or images from the network), so it works offline and can be sent as it is. The same program and seed always give the same page, byte for byte.

**What it shows.** The arena, with a circle for each unit in the color of its team and the initial of its name, a bar of its health, and a cross when it is dead; if the battle has no arena, the box that holds the units. The mouse over a unit tells its name, team, health, attributes, position and effects (with their turns left). A mark on a unit tells that an effect is acting on it, and a short flash that it was hit or healed. Next to it, a panel with two tabs: **Log**, the battle in the same lines as the text output, with the current event highlighted (click a line to go there), and **Metrics** (see [Metrics](#metrics)). The legend under the field tells, for each team, its color and its units alive, as the playback goes. The page is titled `RPG battle replay - seed N`. A red banner at the top tells that the simulation was aborted, with its message, when it failed: the page shows the battle until that point (and if it failed before the first event, for example because a team had no units at the start, there is nothing to play, and the play, step and progress controls are disabled).

**Controls.**

| Control | Key | Does |
| :------ | :-- | :--- |
| Play / Pause | space | Plays the events from the current one, at the speed of the slider (1 to 60 events per second, 10 by default) |
| Event ‹ / › | left / right arrow | Goes one event back or forward |
| « Round / Round » | shift + left / right arrow | Goes to the start of the previous or next round |
| Reset | | Goes back to the start |
| Progress | | A slider over all the events |
| Log / Metrics | left / right arrow on a tab | Shows the log or the metrics |

The keys are also listed in a fold under the field ("Keyboard shortcuts"). A button that has the focus (tab to it) is pressed by the space bar and enter as usual, and its focus is drawn with a thick outline. The text is dark enough to be read over its background (at least 4.5:1, and 3:1 for the colors of the teams, which are also told apart by the initial of each unit and by the legend), and when the system asks for reduced motion the units jump to their places and nothing flashes. The page can be printed: the controls and the log are left out, and the field and the metrics are not.

**The data.** The page is a template (`src/main/resources/replay-template.html`, which can be edited as any HTML file) with a single marker, where the compiler writes the battle as JSON, in a block `<script id="battle-data" type="application/json">`. The CMake build turns the template into a C array (`src/main/cmake/EmbedFile.cmake`), so the binary needs no file at run time. The JSON is compact, because a battle can have hundreds of thousands of events: the units and the events are arrays of numbers, and every text is written once in a table of `strings` that the events point to. For the example of this README (the full data is `src/test/c/html/01-one-on-one.data`):

```json
{
"seed":"1",
"maxRounds":1000,
"arena":null,
"teams":["Heroes","Goblins"],
"units":[
[0,"Hero",0,"Hero",0,0,30,30,8,2,6],
[1,"Goblin",1,"Goblin",0,0,12,12,5,1,4]
],
"strings":[
"Slash"
],
"events":[
[0,1,0,-1,-1,0,0,0,0,0,0,-1],
[1,1,0,0,1,0,0,0,0,0,0,0],
[2,1,1,0,1,7,5,0,0,0,0,-1],
...
],
"result":{"kind":"winner","team":0,"rounds":2},
"error":null
}
```

A unit is `[id, name, team, declaration, x, y, health, maxHealth, attack, defense, speed]`, and an event is `[type, round, depth, actor, target, amount, health, fromX, fromY, x, y, string]`, where the type is the position of the event in the `EventType` of `Trace.h` (0 for the start of a round, to 12 for the end of the battle) and `string` is an index of `strings` or -1. The seed is a string because it can be up to 2^64 - 1, more than a number of JavaScript can hold. The page rebuilds the state of the battle at any event by applying the values of the events to the units (it does not run anything), starting from a checkpoint (a copy of the state that it keeps the first time it goes past each stretch of events, with a memory bounded for any size of battle), so moving back and forth is immediate. Only the 200 lines of the log that are around the current event are in the page at any time, so a battle of any size is as light as a small one. The comment at the top of `src/main/c/backend/code-generation/HtmlGenerator.c` has the full format.

**Safety.** Everything that the program writes (names, tags, the messages of `log`) is data, and it can't run: the generator writes `<`, `>`, `&`, the line and paragraph separators (U+2028 and U+2029) and the control characters as `\uXXXX` and every byte that is not valid UTF-8 as U+FFFD, so no text can close the `<script>`, and the page puts all of it on the screen as text, never as HTML. The page also carries a content security policy that allows no network access, no frames and no scripts or styles other than its own (`default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:`).

**Size.** The page takes about 80 KB plus about 40 bytes for each event: a battle of 500 against 500 is about 0.5 MB, and the limit of events (500000) keeps any page under 20 MB. The player was checked with 10000 units and 125000 events: it loads in under 0.2 seconds, computes its metrics in about 40 milliseconds and keeps 60 frames per second.

### Metrics

The second tab of the panel of the page has the metrics of the battle. The page computes them when it loads, in a single pass over the events (`Replay.computeMetrics`, in the template), and for a battle that failed (status 101) it does it with what was reached. They are made only of the values that the events carry, with no rule of the game recomputed:

* **Damage and healing are the effective ones**: the health of the unit before the event minus its health after it, so a hit of 7 on a unit with 3 health counts for 3, and a heal of 5 on a unit with 2 missing counts for 2. The amount that the event says is not used.
* **Whoever acts is credited.** What happens in a turn or inside an ability is credited to the unit that acts. What happens inside the tick of an effect (the events deeper than it, up to the next event that is not) is credited to the unit that applied the effect, or renewed it for the last time, and it is counted apart as _effect damage_ or _effect healing_.
* **A kill** is a death credited like the damage that caused it, so the poison that finishes a unit is a kill of the unit that poisoned it. A unit that kills an ally, or itself, gets the kill: the friendly fire of the programs counts as any other damage.
* **Effects applied** counts each time an effect is applied or renewed.

The panel has the same size in both tabs, so the battlefield does not move when the tab changes: the metrics scroll inside the panel, and the tables scroll sideways when they are wider than it. The tab shows:

| Part | What it is |
| :--- | :--------- |
| Cards | The result, the rounds, the units alive out of the units at the start of each team, and the total damage and healing (with how much is from effects). They are about the whole battle. |
| Units alive, Total health | A line for each team, with the value at the end of each round (the round 0 is the start). The dashed line is how far the playback is (it moves inside the round, as the events go), the mouse tells the values of a round and a click goes to the start of that round. The total health is the sum of the health of the units alive. |
| Damage dealt by team | Bars of the damage of each team, with its abilities (solid) and its effects (striped) one over the other. |
| Teams | For each team: units at the start, alive and dead, damage, effect damage, healing, effect healing, kills and effects applied. |
| Units | For each unit: its team, whether it is alive or the round in which it died, damage, effect damage, damage taken, healing, effect healing and kills. It is sorted by damage, and a click on a header sorts by that column (and again, the other way). It shows the first 50 and a button shows up to 1000 (with a note if there are more). A click on a row goes to the first event of the unit. |

### Examples

The folder `examples` has three programs that serve as a demo, as the material of a report and as tests. Each one explains its idea in a comment at its start, has an `on turn` for every unit, and ends with a winner (never a draw) in less than 80 rounds, for each seed from 1 to 20. Between them they use every feature of the language: arena, positions, groups with `around`, tags and targets by tag, abilities with a parameter, three kinds of effect (damage over time, healing over time and one that makes its victim flee with `move away from`), `radius`, `nearest`, `for`, `if` / `else`, `while`, dice, `log` with placeholders, `heal`, `move toward` and `move away from` and a battle of three teams.

| File | Teams | What it shows |
| :--- | :---- | :------------ |
| `examples/01-skirmish.rpg` | 2 teams, 6 against 6 | A small battle, readable in full: a Cleric that mends, a Mage whose area attack also burns its friends, a Shaman whose Poison hurts every turn, and Archers that back off when they are hurt. |
| `examples/02-three-armies.rpg` | 3 teams, 5 + 5 + 4 | All against all: the Paladins smite the undead (an ability that only targets a tag), the Wraith makes its victims flee, the Priest heals over time and the Warlord keeps striking while the target is nearly dead (`while`). |
| `examples/03-siege.rpg` | 2 teams, 40 against 60 | A big battle in a big arena, with groups scattered `around` points. The volleys of the Archers hit an area and leave their victims bleeding. Good to see the metrics with some volume. |

To run one, as text and as a page (the seed makes the battle repeatable):

```bash
LOGGING_LEVEL=ERROR src/main/bash/run.sh examples/01-skirmish.rpg --seed 1
LOGGING_LEVEL=ERROR src/main/bash/run.sh examples/01-skirmish.rpg --seed 1 --format html > skirmish.html
```

The abilities have no range in the language, so the examples ask for the units around the nearest enemy (`for (neighbor in radius 2 of nearest(enemy)) { if (neighbor == self) ... }`): a unit acts only if it is among them, which keeps the log free of attempts that do not happen. The language has no way to ask for the tag of a unit in an expression either, so the Paladins of `02-three-armies` tell the undead apart by their speed.

### Known limitations

* The abilities have no range: a unit hits its `nearest` no matter how far it is. Where the units are matters through the areas of `radius`.
* A body runs to its end even if the unit that runs it dies halfway.
* The cost grows with the square of the number of units (`nearest`, `allies` and `enemies` go over all of them): a battle of 5000 against 5000 takes about 11 seconds with the sanitizers on.
* There is no timeout on the clock: a battle inside all the limits can still take long.
* `deal` and `heal` tell the amount that was evaluated, not the effective one: `heals 5 (health 10/10)` can be a heal of 3.
* Numbers above 2^53 are shown with lost precision in the HTML replay and in its metrics, because JavaScript has no exact integers beyond that (the engine works with 64 bits).
* A byte order mark (BOM, the bytes `EF BB BF`) at the start of the file is not accepted: it is an unrecognized character, like any other that is not ASCII outside a string or a comment.

### Tests

Three more categories under `src/test/c` run the whole compiler with `--seed 1`, and the options of the `NN-name.args` file next to a program, if it has one (for example, `--max-rounds 5`), and a fourth one runs the examples:

* `simulation`: the battle must end (exit status `0`) with no error, and write exactly what the `NN-name.stdout` file has.
* `simulation-reject`: the simulation must fail (exit status `101`) with the messages of the `NN-name.stderr` file, which are exhaustive, as in `semantic-reject`.
* `html`: the program is run with `--format html` (see [HTML replay](#html-replay)). The page must be complete (it starts with `<!DOCTYPE html>`, ends with `</html>` and has the content security policy), must load nothing from outside (no `http://`, no `https://`, no `<script src` and no `<link`), and the JSON block of its data must be exactly what the `NN-name.data` file has. The exit status is `0`, or `101` if the program has an `NN-name.stderr` file, which lists the error of the simulation (the page must have the battle until it failed and the error in its data).
* `examples` (the programs of the folder `examples`, see [Examples](#examples)): each one is run with `--seed 1`, as text and as a page. Its text must end with a winner (exit status `0`, no error and a `Winner:` line), for each team the alive plus the dead of the last lines must be the units of the header, and its page must be complete, as in `html`.

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description |
| :-------------------- | :-----: | :---------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`. |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log on the standard error. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. The default is `ALL` under Docker Compose, and `INFORMATION` when the binary runs on its own. |

The `SEED` variable (empty by default, which means no seed) sets the seed of the simulation when `--seed` is not given (see [Simulation](#simulation)).

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
src/main/bash/run.sh <program> [options]
```

where `<program>` is the path to the file that represents its entry-point. The options after it are forwarded to the compiler: `--syntax-only`, `--check`, `--seed N`, `--max-rounds N` and `--format text|html` (see [Simulation](#simulation)). A valid program with a `battle` is simulated, and the battle is written to the standard output, as text or, with `--format html`, as a page that replays it (`run.sh battle.rpg --format html > battle.html`, see [HTML replay](#html-replay)). The exit status is `0` when the program is accepted and its battle ends, `1` when it is rejected, `100` when the command line is invalid (for example, an unknown option) and `101` when the simulation fails, and a syntax error reports its line. Every log goes to the standard error, so the standard output is reserved for the output of the compiled program. To watch the AST being built, run with `LOGGING_LEVEL=DEBUGGING`: the semantic actions log themselves in the order they build it.

### Test

Executes every available test (the ones under `src/test/c` and the examples of `examples`):

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

The tests have eight categories, seven of them under `src/test/c` and the last one over the folder `examples`:

* `src/test/c/accept`: valid programs, which must be accepted by the syntactic analysis.
* `src/test/c/reject`: invalid programs, which must be rejected by it.
* `src/test/c/semantic-accept` and `src/test/c/semantic-reject`: the same for the semantic analysis (see [Semantic analysis](#semantic-analysis)).
* `src/test/c/simulation` and `src/test/c/simulation-reject`: battles that must end with a given text, and battles that must fail with a given error (see [Simulation](#simulation)).
* `src/test/c/html`: programs whose HTML replay must be well formed, self-contained and have a given data block (see [HTML replay](#html-replay)).
* `examples`: the example programs, which must end with a winner as text and as a page (see [Examples](#examples)).

Each test is as small as possible and covers a single feature, and a few of the rejections pin down restrictions that are intentional (for example, `around` only goes with a quantity, and `at` only without one). A few larger programs show the features working together.

`test.sh` runs the `accept` and `reject` programs with `--syntax-only`, the semantic ones with `--check`, the simulation ones with `--seed 1`, the `html` ones with `--seed 1 --format html`, and the examples with `--seed 1` in both formats. It requires the exact exit status: `0` for the `accept` categories, `simulation` and `html` (except the `html` programs that have a `.stderr` file, which are `101`), `1` for the `reject` ones and `101` for `simulation-reject`. It runs the compiler at the `WARNING` logging level, so that the standard error is the same everywhere. The build links AddressSanitizer and UndefinedBehaviorSanitizer, and a test also fails when its standard error holds a sanitizer report, so a crash can't pass as a rejection.

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
