# Mancala

A terminal implementation of Mancala (Kalah) in C, with a minimax + alpha-beta computer opponent.

The whole program is a single C file with no dependencies beyond the C standard library. It runs a text menu, draws the board with `printf`, and reads moves from stdin. Board geometry is configurable at the start of each game, so the same code plays a standard 6-pit board or anything from 1 to 10 pits per side. The computer opponent builds an explicit game tree each time it moves, scores the leaves with a positional heuristic, and picks a move by minimax with alpha-beta pruning.

## Features

- Player vs Computer, and local Player vs Player on one keyboard.
- Configurable board: 1-10 pits per side (default 6), 1-12 starting stones per
  pit (default 4).
- Three AI difficulty settings, which select the minimax search depth.
- Choice of who moves first in Player vs Computer.
- Save and load to three numbered slots (`slot1.txt`, `slot2.txt`,
  `slot3.txt`), written to the current working directory.
- In-game rules screen and credits screen.
- Input validation on every prompt: out-of-range numbers, non-numeric text, and
  moves from empty pits are rejected and re-prompted. Pressing Enter on a setup
  prompt takes the default shown in the prompt. Closing stdin (EOF) frees the
  board and exits rather than spinning.

## How the AI works

All of the AI lives in `evaluate`, `build_tree_alphabeta`, and `comp_move`.

### Explicit game tree

Unlike the usual implementation that recurses over a single mutable board, this
one materialises the search tree in memory. Every node is a `Node`:

```c
typedef struct Node
{
    int *current_state;   // private heap copy of the whole board
    int current_player;   // 1 or 2 - whose turn it is at this node
    int move;             // the pit (1..PITS) that produced this node
    int value;            // minimax value, filled in by the search
    int extra_turn;       // did the move into this node end in a store?
    int winner;           // -1 none yet, 0 draw, 1 or 2 a winner
    struct Node **children;
    int child_count;
} Node;
```

`create_node` allocates the node and a full copy of the board via `copy_state`, so each node owns its own position. `build_tree_alphabeta` expands a node by looping over pits `1..PITS`, copying the parent's board, and calling the same `move` function the human player's turns go through. That means the search sees exactly the rules the game enforces, including sowing direction, skipping the opponent's store, captures, and the end-of-game sweep. Moves from empty pits come back with `legal_move == 0` and are skipped, so a node has between 0 and `PITS` children.

### Minimax and whose node maximises

The computer is `COMP_PLAYER` (1 or 2). At each node:

```c
int is_max = (node->current_player == COMP_PLAYER);
```

so a node is a maximising node when it is the computer's turn to move there and
a minimising node otherwise. The important detail is that this is driven by
`current_player`, not by parity of depth, because Mancala grants extra turns.
When a move ends in the mover's own store, `move` reports `extra_turn`, and the
child is created with the *same* player to move:

```c
int next_player = extra_turn ? node->current_player : (3 - node->current_player);
```

A chain of extra turns therefore produces a run of consecutive maximising (or
consecutive minimising) nodes on the same path. Alternating min and max by
level would misvalue exactly the positions Mancala players care most about.

### Alpha-beta pruning

The search carries an `alpha` (best value the maximiser can already force) and
`beta` (best the minimiser can already force), seeded at the root with `INT_MIN`
and `INT_MAX`. A maximising node raises `alpha` as its children come back; a
minimising node lowers `beta`. As soon as `alpha >= beta`, the loop over the
remaining pits breaks:

```c
if (alpha >= beta)
    break;
```

The window has collapsed, so whatever the unexamined siblings are worth, the
parent will never choose this line: the opponent already has a reply at least as
good elsewhere. Those subtrees are never generated at all, which in this design
saves the allocation and copying as well as the evaluation. The saving compounds
with depth - in the best case alpha-beta examines roughly the square root of the
nodes plain minimax would, which is what makes the depth-7 setting usable on a
6-pit board.

Note that pruning also means a pruned child's stored `value` is a bound rather
than an exact score. `comp_move` only needs the argmax at the root, which the
search still gets right.

### Evaluation function

Leaves - nodes at depth 0, nodes where `winner != -1`, and nodes with no legal
moves - are scored by `evaluate`:

```c
if (node->winner == COMP_PLAYER) return  100000;
if (node->winner == human_player) return -100000;
if (node->winner == 0)            return  0;

int val = computer_store - human_store;
if (node->extra_turn)
{
    if (node->current_player == COMP_PLAYER)
        val += EXTRA_TURN_BONUS;
    else
        val -= EXTRA_TURN_BONUS;
}
```

Three things are being approximated:

- **Terminal scores.** A finished game is worth `+/-100000`, far outside the
  range any heuristic score can reach (a board holds at most
  `2 * PITS * INITIAL_STONES` stones, 48 on the default board), so a forced win
  always dominates any amount of material and a forced loss is never chosen over
  a playable line. A draw is exactly 0.
- **Store differential.** `computer_store - human_store` is the only material
  term. Stones in a store are permanent and are the actual win condition, so
  this measures locked-in progress and ignores stones still in play, which can
  still change hands. It is a deliberately cheap heuristic: no term for pit
  distribution, mobility, or capture threats.
- **Extra-turn bonus.** A move ending in your own store is worth roughly a
  tempo, valued here at the fixed `EXTRA_TURN_BONUS` of 2 - about half a pit on
  the default board. The bonus is signed by who earned it: because
  `build_tree_alphabeta` leaves `current_player` unchanged on an extra turn, the
  node's `current_player` is the player who earned it, so the bonus is added for
  the computer and subtracted for the human. The constant is deliberately not
  tied to `INITIAL_STONES`, which the player can set as high as 12; a bonus that
  large would let a single tempo outweigh a substantial store lead.

### Depth and difficulty

`select_difficulty` sets the global `GAME_TREE_DEPTH`:

| Menu choice | Depth |
| --- | --- |
| 1. Easy | 1 |
| 2. Medium (default) | 4 |
| 3. Hard | 7 |

Depth is counted in plies of the tree, and because extra turns do not switch the
player, a ply is not always a change of side. At depth 1 the computer just picks
the move with the best immediate store differential. The depth is saved and
restored with the game.

`comp_move` builds a fresh tree from the live board on every computer turn,
reads the values off the root's children, takes the highest, frees the whole
tree, and returns that pit number. Nothing is cached between moves.

## Building

The game:

```
gcc -Wall -Wextra -O2 -o mancala mancala.c
```

The self-play harness described under [Evaluating the AI](#evaluating-the-ai),
which is optional and not needed to play:

```
gcc -Wall -Wextra -O2 -o selfplay selfplay.c -lm
```

The harness needs `-lm` because it computes confidence intervals with `sqrt`.

Both compile with no warnings under those flags. One thing that looks alarming
but is not: `selfplay.c` includes `mancala.c` with `main` renamed via `#define`,
so that `main` becomes an ordinary function, and GCC then stops applying its
special case for falling off the end of `main` and emits a
`control reaches end of non-void function` warning. That warning is an artefact
of the harness, not a defect in the game, so `selfplay.c` suppresses it with a
`#pragma GCC diagnostic` pair scoped to just that `#include`. Nothing in
`mancala.c` was changed to accommodate it, and the game on its own is clean
under `-Wall -Wextra`.

## Running and playing

```
./mancala
```

### Menu flow

The main menu offers:

```
1. New Game
2. Load Game
3. Rules
4. Credits
5. Exit
```

Choosing **New Game** walks through setup in this order: pits per side, starting
stones per pit, game mode (vs Computer or local two-player), and - in vs
Computer only - who moves first and the difficulty. Every prompt shows its
default in parentheses; pressing Enter with no input takes it.

**Load Game** asks for a slot number and, if the file loads, resumes that game
immediately - board, side to move, mode, and AI depth all come from the file.

### Taking a turn

On a human turn the prompt is:

```
Player, choose a pit (1-6, or enter 0/'S' to Save):
```

Enter the number of one of your own pits. Pits are numbered `1..PITS` from each
player's own point of view, so both players use the same numbers; the display
labels both rows accordingly. Choosing an empty pit prints a message and
re-prompts without consuming your turn.

Entering `0`, `S`, or `s` at this prompt opens the save menu, asks for a slot,
writes the file, and returns you to the same prompt with the same position and
the same player to move. Saving is only available on a human turn; loading is
only available from the main menu.

After each move the program reports what happened - a capture, an extra turn, or
a plain move - and then redraws the board.

### Reading the board

```
                 Player's Pits

Pit Number:        1     2     3     4     5     6
Stones:            4 <-  4 <-  4 <-  4 <-  4 <-  4

Stores:        [PL]  0                        0 [CO]

Stones:            4 ->  4 ->  4 ->  4 ->  4 ->  4
Pit Number:        1     2     3     4     5     6

                 Computer's Pits

----------------------------------------------------
```

- The top row is Player 1's side, the bottom row is Player 2's side, each
  labelled with the owner's name above or below it.
- Each side is printed with its own pit numbers `1..PITS`, directly under (or
  over) the stone counts.
- The arrows show the direction stones travel for that row: the top row sows
  leftward into the left store, the bottom row rightward into the right store.
  Sowing is counterclockwise around the board.
- The stores line shows Player 1's store on the left and Player 2's store on the
  right. In vs Computer, they are tagged `[CO]` for the computer and `[PL]` for
  you, on whichever sides those players occupy. In local two-player they are
  tagged `[P1]` and `[P2]`.

When the game ends, the final board is printed under a `FINAL BOARD STATE`
header, followed by both scores and the result, and control returns to the main
menu.

## Game rules

As stated by the in-game rules screen:

1. Each player has 6 pits (by default) and one store.
2. Each pit starts with 4 stones (by default).
3. On your turn, choose a non-empty pit on your side.
4. Distribute the stones counterclockwise, one at a time.
5. Skip the opponent's store while distributing.
6. If the last stone lands in your store, you get another turn.
7. If the last stone lands in an empty pit on your side, you capture that stone
   and all stones in the opposite pit.
8. The game ends when all pits on one side are empty.
9. The remaining stones on the board are added to their respective stores.
10. The player with the most stones in their store wins.

Two details of how the code implements rule 7 and rule 8 are worth spelling out.
A capture triggers on any empty pit on your own side, including when the pit
directly opposite is also empty - in that case you capture just the landing
stone. And the end-of-game sweep in `check_win` runs after every move: as soon as
either side's pits are all empty, both sides' leftovers go to their owners'
stores and the totals are compared.

## Evaluating the AI

`selfplay.c` is a harness for answering a specific question: does a change to
the evaluation function actually make the AI play better? It was written to test
the signed extra-turn bonus described above, and it is kept in the repo because
the answer turned out to be interesting.

### What the harness does

It includes `mancala.c` with `main` renamed out of the way, so it drives the
real `move()`, `check_win()` and `evaluate()` rather than a reimplementation.
The previous, sign-blind evaluator is reproduced alongside as
`evaluate_prefix()`, so the two can be played head to head.

`build_tree_alphabeta()` calls `evaluate()` directly and cannot be given a
different evaluator without editing the game, so the harness carries its own
`search()` that mirrors it: same move ordering over pits `1..PITS`, same
fail-soft window initialisation, same alpha/beta updates, same pruning test, and
the same argmax with a strict `>` so ties go to the lowest-numbered pit. Both
sides of every match run that same search, so the only variable in a match is
the evaluation function.

Four things make the numbers mean something:

- **`selfcheck()` verifies the mirrored search.** It compares the harness's
  chosen move against the shipped `comp_move()` across 40 random positions at
  depth 4 and reports mismatches. It finds 0, so the harness is searching the
  way the game searches.
- **Random opening plies break determinism.** The engine has no randomness, so
  at a fixed depth every game from the standard start is the same game. Each
  match therefore begins with 4 random legal plies, which yields distinct
  positions to play from.
- **Paired openings with seat swapping.** Every opening is played twice, once
  with the evaluator under test as Player 1 and once as Player 2. That cancels
  both the first-move advantage and any bias in the particular opening.
- **Confidence intervals over opening pairs, not games.** The two games in a
  pair share an opening and are not independent, so treating them as 2N
  independent samples would understate the interval. The harness scores each
  pair in `[0, 1]` and computes the interval across pairs.

Run it with:

```
./selfplay [openings_per_match]
```

Each opening produces two games, so `./selfplay 1000` is a 2000-game match per
configuration. The results below are from that run.

### Depth controls

Before trusting a null result, the harness has to be shown capable of detecting
a strength difference it should detect. These matches hold the evaluator fixed
and vary only the search depth:

| Control | W/L/D | Score | 95% CI |
| --- | --- | --- | --- |
| Depth 4 vs depth 1 | 1325/553/122 | 69.3% | [67.6%, 71.0%] |
| Depth 7 vs depth 4 | 1415/475/110 | 73.5% | [71.9%, 75.1%] |

An extra three plies is worth about 19 points of score, and three more on top of
that about 24. The harness resolves depth differences clearly.

### Fixed vs pre-fix evaluator

The same setup, now holding depth equal on both sides and varying only the
evaluation function. Scores are from the fixed evaluator's point of view:

| Match | W/L/D | Score | 95% CI |
| --- | --- | --- | --- |
| Depth 1 | 898/951/151 | 48.7% | [48.1%, 49.3%] |
| Depth 4 | 933/939/128 | 49.9% | [48.0%, 51.7%] |
| Depth 7 | 944/938/118 | 50.1% | [48.6%, 51.7%] |

**The fix measured at parity.** At depths 4 and 7 the interval contains 50%, so
the fixed evaluator is statistically indistinguishable from the one it replaced.
At depth 1 the interval excludes 50%, making the fixed version very slightly
worse there, by about 1.3 points.

This is not a case of the change never running. Over 200 sampled positions the
two evaluators choose a different move in 0% of positions at depth 1, 10.5% at
depth 4, and 17.5% at depth 7. Instrumenting which branch of the signed bonus
fires over the same positions gives:

| Depth | Bonus added (own extra turn) | Bonus subtracted (opponent's) |
| --- | --- | --- |
| 1 | 141 | 0 |
| 4 | 1552 | 2413 |
| 7 | 55473 | 40922 |

The zero at depth 1 is structural rather than a sampling artefact. At depth 1
the root's children are leaves, and a child only carries `extra_turn` when the
searching side's own move ended in its own store, which leaves `current_player`
equal to the searching side. The subtract branch is therefore unreachable at
depth 1, and the only thing in play there is the change of bonus magnitude from
`INITIAL_STONES` to 2 - which is what that 48.7% is measuring.

At depths 4 and 7 the fix changes roughly one move in six and still produces no
net change in strength.

### Why the fix was kept

The fix is in the code because it is correct, not because it won more games. A
position where the opponent has just earned a free turn is bad for the engine,
and the old code scored it as good; that is a defect in the evaluation function
whether or not it shows up in a match result. The honest summary is that
correcting it did not make this AI stronger against this opponent on this board,
and the measurement above is the evidence for that rather than against it.

Two limits on what was measured. The match bundles both changes made at the same
time - the sign guard and the magnitude change from `INITIAL_STONES` to a fixed
2 - and does not separate their effects. And it compares the fixed evaluator only
against the pre-fix one, which is not the same as measuring it against a human or
against a stronger engine.

## Implementation notes

### Board representation

The board is one flat `int` array on the heap, pointed to by the global `state`,
of length `STATE_SIZE = 2 * (PITS + 1)`. Index 0 is Player 1's store, then
Player 1's pits, then Player 2's store, then Player 2's pits:

| Index | Contents |
| --- | --- |
| `0` | Player 1's store |
| `1 .. PITS` | Player 1's pits, pit `n` at index `n` |
| `PITS + 1` | Player 2's store |
| `PITS + 2 .. 2*PITS + 1` | Player 2's pits, pit `n` at index `STATE_SIZE - n` |

On the default 6-pit board that is 14 slots: store 0, pits 1-6, store 7, pits
8-13.

The layout makes the arithmetic in `move` fall out cleanly:

- A player's own store is at `(current_player - 1) * (PITS + 1)` and the
  opponent's at `(2 - current_player) * (PITS + 1)`.
- Sowing walks the array with `idx--`, wrapping from `-1` back to
  `2 * PITS + 1`. Decreasing index is counterclockwise for both players, so one
  loop serves both sides. The opponent's store index is skipped with `continue`,
  which does not consume a stone.
- The pit opposite index `idx` is `STATE_SIZE - idx`, which is what the capture
  rule uses.

`move` reports back through out-parameters - `legal_move`, `extra_turn`,
`winner`, `captured` - rather than a return value, and the same function is used
by the interactive game loop and by the search.

### Memory ownership

- `state` is malloc'd in `start_new_game` (freeing any previous board first),
  replaced wholesale by `load_game` on a successful load, and freed when Exit is
  chosen from the menu or when `handle_eof` runs on closed stdin.
- `load_game` reads the header and the board into locals and validates them
  before committing anything to the globals, so a truncated or corrupt slot file
  cannot leave `PITS` and `state` inconsistent with each other. It rejects pit
  counts outside 1-10, stone counts outside 1-12, a bad side-to-move, an
  implausible mode, a depth below 1, and any negative stone count.
- Each `Node` owns its own board copy, allocated by `copy_state`. Inside
  `build_tree_alphabeta` the scratch board passed to `move` is freed as soon as
  the child node has copied it, on both the legal and illegal paths.
- `free_tree` walks the tree depth-first, freeing each node's children array,
  board copy, and the node itself. `comp_move` calls it on the root once the
  best move has been read off, so nothing survives a computer turn.
- Every allocation is checked; failure prints a message and calls
  `exit(EXIT_FAILURE)`.

### Global configuration

Game settings live in globals - `PITS`, `INITIAL_STONES`, `PLAYER` (side to
move), `VS_COMP`, `COMP_PLAYER`, `GAME_TREE_DEPTH`, `STATE_SIZE` - plus the two
display-name pointers `PLAYER1_TEXT` and `PLAYER2_TEXT`, which are repointed at
`"Computer"`/`"Player"` or `"Player 1"`/`"Player 2"` when a game starts or
loads. The file-scope initialisers on `INITIAL_STONES`, `PITS`, and
`GAME_TREE_DEPTH` are 4, 6, and 4 - the same defaults the setup prompts and the
rules screen advertise - though in practice `start_new_game` or `load_game`
always overwrites all three before a board exists. `EXTRA_TURN_BONUS` sits
beside them and is the one value here that is genuinely constant. In local
two-player mode `COMP_PLAYER` is set to 0, a value no real player has, so the
`PLAYER == COMP_PLAYER` test never fires and the AI is never consulted.

### Save file format

Two lines of plain text: a header of six integers
(`INITIAL_STONES PITS PLAYER VS_COMP COMP_PLAYER GAME_TREE_DEPTH`), then the
`STATE_SIZE` board values in index order.

## Project structure

```
mancala.c    entire program
selfplay.c   test harness, not part of the game (see Evaluating the AI)
README.md    this file
```

`selfplay.c` is a development tool, not something the game needs. It includes
`mancala.c` so it can drive the real `move()` and `evaluate()` through a minimax
search with the evaluator as a parameter, which lets two evaluation functions
play each other head to head with no human input. It builds separately and does
not affect the game:

```
gcc -Wall -Wextra -O2 -o selfplay selfplay.c -lm
./selfplay [openings_per_match]
```
