/*
 * selfplay.c - head-to-head harness: the fixed evaluate() against the pre-fix
 * one, both driven by the same minimax search at the same depth.
 *
 * Not part of the game. It includes mancala.c so it can call the real move(),
 * check_win() and evaluate() instead of a reimplementation; mancala.c's main()
 * is renamed out of the way by the #define below and never runs, so the
 * interactive program is untouched.
 *
 * Build: gcc -Wall -Wextra -O2 -o selfplay selfplay.c
 * Run:   ./selfplay [openings_per_depth]      (default 100 = 200 games)
 */

/* Renaming main() makes it an ordinary function, so GCC no longer applies
   its special case for falling off the end of main. That warning is an
   artefact of this harness, not of the game (mancala.c builds clean on its
   own), so it is suppressed just across the include. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main mancala_main_unused
#include "mancala.c"
#undef main
#pragma GCC diagnostic pop

#include <time.h>
#include <math.h>

#define OPENING_PLIES 4
#define PLY_CAP 2000

typedef int (*eval_fn)(const Node *node);

/* ------------------------------------------------------------------
   The evaluation function exactly as it stood before the extra-turn
   fix: bonus magnitude INITIAL_STONES, added regardless of who earned
   the extra turn. Kept here so the old behaviour can be played against
   the new one without touching mancala.c.
   ------------------------------------------------------------------ */
static int evaluate_prefix(const Node *node)
{
    int human_player = 3 - COMP_PLAYER;
    int computer_store_idx = (COMP_PLAYER - 1) * (PITS + 1);
    int human_store_idx = (human_player - 1) * (PITS + 1);

    int computer_store = node->current_state[computer_store_idx];
    int human_store = node->current_state[human_store_idx];

    if (node->winner == COMP_PLAYER)
        return 100000;
    if (node->winner == human_player)
        return -100000;
    if (node->winner == 0)
        return 0;

    int val = computer_store - human_store;
    if (node->extra_turn)
        val += INITIAL_STONES;

    return val;
}

/* ------------------------------------------------------------------
   Mirror of build_tree_alphabeta() with the evaluator as a parameter.
   Same move ordering (pits 1..PITS), same fail-soft window init, same
   alpha/beta updates, same pruning test, same "no legal children"
   fallback - so for a given evaluator it returns the same values the
   real search does. The tree is not materialised, which changes
   nothing about the values; selfcheck() below verifies the agreement
   against the real comp_move().
   ------------------------------------------------------------------ */
static int search(int *board, int cur_player, int extra_turn, int winner,
                  int depth, int alpha, int beta, int me, eval_fn ev)
{
    Node n;
    n.current_state = board;
    n.current_player = cur_player;
    n.move = 0;
    n.value = 0;
    n.extra_turn = extra_turn;
    n.winner = winner;
    n.children = NULL;
    n.child_count = 0;

    if (depth <= 0 || winner != -1)
        return ev(&n);

    int is_max = (cur_player == me);
    int value = is_max ? INT_MIN : INT_MAX;
    int child_count = 0;

    for (int i = 0; i < PITS; i++)
    {
        int *tmp = copy_state(board);

        int legal_move = 1;
        int extra = 0;
        int w = -1;
        int captured = 0;

        move(tmp, i + 1, &legal_move, cur_player, &extra, &w, &captured);

        if (legal_move)
        {
            int next_player = extra ? cur_player : (3 - cur_player);
            child_count++;

            int child_val = search(tmp, next_player, extra, w,
                                   depth - 1, alpha, beta, me, ev);

            if (is_max)
            {
                if (child_val > value)
                    value = child_val;
                if (value > alpha)
                    alpha = value;
            }
            else
            {
                if (child_val < value)
                    value = child_val;
                if (value < beta)
                    beta = value;
            }

            free(tmp);

            if (alpha >= beta)
                break;
        }
        else
            free(tmp);
    }

    if (child_count == 0)
        return ev(&n);

    return value;
}

/* Root search: mirrors comp_move(), including the argmax over children
   with a strict >, so ties go to the lowest-numbered pit. */
static int engine_move(const int *board, int cur_player, int depth, eval_fn ev)
{
    /* evaluate() reads COMP_PLAYER to decide which side it is scoring for */
    COMP_PLAYER = cur_player;

    int best_move = 1;
    int best_val = INT_MIN;
    int alpha = INT_MIN;
    int beta = INT_MAX;

    for (int i = 0; i < PITS; i++)
    {
        int *tmp = copy_state(board);

        int legal_move = 1;
        int extra = 0;
        int w = -1;
        int captured = 0;

        move(tmp, i + 1, &legal_move, cur_player, &extra, &w, &captured);

        if (legal_move)
        {
            int next_player = extra ? cur_player : (3 - cur_player);
            int v = search(tmp, next_player, extra, w,
                           depth - 1, alpha, beta, cur_player, ev);

            if (v > best_val)
            {
                best_val = v;
                best_move = i + 1;
            }
            if (v > alpha)
                alpha = v;
        }

        free(tmp);

        if (alpha >= beta)
            break;
    }

    return best_move;
}

/* Play to the end. Returns 0 draw, 1 player 1, 2 player 2, or -2 if the
   ply cap was hit. */
static int play_game(const int *start_board, int start_player,
                     int depth_p1, int depth_p2,
                     eval_fn ev_p1, eval_fn ev_p2, int *plies_out)
{
    int *board = copy_state(start_board);
    int cur = start_player;
    int winner = -1;
    int plies = 0;

    while (winner == -1 && plies < PLY_CAP)
    {
        eval_fn ev = (cur == 1) ? ev_p1 : ev_p2;
        int depth = (cur == 1) ? depth_p1 : depth_p2;
        int pit = engine_move(board, cur, depth, ev);

        int legal_move = 1;
        int extra = 0;
        int w = -1;
        int captured = 0;

        move(board, pit, &legal_move, cur, &extra, &w, &captured);

        if (!legal_move)
        {
            free(board);
            *plies_out = plies;
            return -2;
        }

        plies++;
        winner = w;

        if (winner == -1 && !extra)
            cur = 3 - cur;
    }

    free(board);
    *plies_out = plies;
    return (winner == -1) ? -2 : winner;
}

/* Play OPENING_PLIES random legal moves to get a distinct starting
   position. Returns the winner if the game somehow ended (it should
   not this early), else -1. */
static int random_opening(int *board, int *cur_player)
{
    for (int p = 0; p < OPENING_PLIES; p++)
    {
        int legal_pits[16];
        int n = 0;

        for (int i = 0; i < PITS; i++)
        {
            int *tmp = copy_state(board);
            int legal_move = 1, extra = 0, w = -1, captured = 0;
            move(tmp, i + 1, &legal_move, *cur_player, &extra, &w, &captured);
            free(tmp);
            if (legal_move)
                legal_pits[n++] = i + 1;
        }

        if (n == 0)
            return -2;

        int pit = legal_pits[rand() % n];

        int legal_move = 1, extra = 0, w = -1, captured = 0;
        move(board, pit, &legal_move, *cur_player, &extra, &w, &captured);

        if (w != -1)
            return w;
        if (!extra)
            *cur_player = 3 - *cur_player;
    }

    return -1;
}

static void make_initial_board(int *board)
{
    for (int i = 0; i < STATE_SIZE; i++)
        board[i] = INITIAL_STONES;
    board[0] = 0;
    board[PITS + 1] = 0;
}

/* Confirm the harness search picks the same move the shipped comp_move()
   does, for the fixed evaluator, across a spread of random positions. */
static int selfcheck(int depth, int positions)
{
    int mismatches = 0;
    int *board = malloc((size_t)STATE_SIZE * sizeof(int));
    if (board == NULL)
        return -1;

    for (int k = 0; k < positions; k++)
    {
        srand(90000 + k);
        make_initial_board(board);
        int cur = 1;
        if (random_opening(board, &cur) != -1)
            continue;

        int mine = engine_move(board, cur, depth, evaluate);

        /* comp_move() reads the globals */
        if (state != NULL)
            free(state);
        state = copy_state(board);
        PLAYER = cur;
        COMP_PLAYER = cur;
        GAME_TREE_DEPTH = depth;

        int theirs = comp_move();

        if (mine != theirs)
        {
            printf("  MISMATCH at position %d: harness=%d comp_move=%d\n",
                   k, mine, theirs);
            mismatches++;
        }
    }

    if (state != NULL)
    {
        free(state);
        state = NULL;
    }
    free(board);
    return mismatches;
}

/* How often does the fix actually change the move chosen from the same
   position? If this is near zero the match result cannot mean much. */
static void disagreement_rate(int depth, int positions)
{
    int differing = 0, counted = 0;
    int *board = malloc((size_t)STATE_SIZE * sizeof(int));
    if (board == NULL)
        return;

    for (int k = 0; k < positions; k++)
    {
        srand(70000 + k);
        make_initial_board(board);
        int cur = 1;
        if (random_opening(board, &cur) != -1)
            continue;

        int a = engine_move(board, cur, depth, evaluate);
        int b = engine_move(board, cur, depth, evaluate_prefix);
        counted++;
        if (a != b)
            differing++;
    }

    free(board);
    printf("  depth %d: fixed and pre-fix choose a different move in "
           "%d of %d positions (%.1f%%)\n",
           depth, differing, counted,
           counted ? 100.0 * differing / counted : 0.0);
}

/* Which branch of the new signed bonus actually fires, by depth. Wraps the
   real evaluate() and counts non-terminal leaves carrying an extra turn. */
static long g_bonus_own, g_bonus_opp;

static int evaluate_counting(const Node *node)
{
    if (node->winner == -1 && node->extra_turn)
    {
        if (node->current_player == COMP_PLAYER)
            g_bonus_own++;
        else
            g_bonus_opp++;
    }
    return evaluate(node);
}

static void bonus_branch_census(int depth, int positions)
{
    g_bonus_own = 0;
    g_bonus_opp = 0;

    int *board = malloc((size_t)STATE_SIZE * sizeof(int));
    if (board == NULL)
        return;

    for (int k = 0; k < positions; k++)
    {
        srand(70000 + k);
        make_initial_board(board);
        int cur = 1;
        if (random_opening(board, &cur) != -1)
            continue;
        engine_move(board, cur, depth, evaluate_counting);
    }

    free(board);
    printf("  depth %d: bonus added (own extra turn) %ld times, "
           "subtracted (opponent extra turn) %ld times\n",
           depth, g_bonus_own, g_bonus_opp);
}

/* Plays side A against side B from paired openings and reports A's score.
   Each opening is played twice with the seats swapped, so first-move
   advantage and opening bias cancel. The confidence interval is computed
   over opening PAIRS, not individual games, because the two games in a
   pair share an opening and are not independent. */
static void run_match(const char *label, int depth_a, eval_fn ev_a,
                      int depth_b, eval_fn ev_b, int n_openings)
{
    int wins = 0, losses = 0, draws = 0, unfinished = 0, skipped = 0;
    int games = 0;
    long total_plies = 0;

    double pair_sum = 0.0, pair_sq = 0.0;
    int pairs = 0;

    int *opening = malloc((size_t)STATE_SIZE * sizeof(int));
    if (opening == NULL)
    {
        printf("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    clock_t t0 = clock();

    for (int o = 0; o < n_openings; o++)
    {
        srand(1000 + o);
        make_initial_board(opening);
        int cur = 1;

        if (random_opening(opening, &cur) != -1)
        {
            skipped++;
            continue;
        }

        double pair_score = 0.0;
        int pair_ok = 1;

        for (int swap = 0; swap < 2; swap++)
        {
            int plies = 0;
            eval_fn p1 = swap ? ev_b : ev_a;
            eval_fn p2 = swap ? ev_a : ev_b;
            int d1 = swap ? depth_b : depth_a;
            int d2 = swap ? depth_a : depth_b;
            int a_seat = swap ? 2 : 1;

            int r = play_game(opening, cur, d1, d2, p1, p2, &plies);
            games++;
            total_plies += plies;

            if (r == -2)
            {
                unfinished++;
                pair_ok = 0;
            }
            else if (r == 0)
            {
                draws++;
                pair_score += 0.5;
            }
            else if (r == a_seat)
            {
                wins++;
                pair_score += 1.0;
            }
            else
                losses++;
        }

        if (pair_ok)
        {
            double ps = pair_score / 2.0;
            pair_sum += ps;
            pair_sq += ps * ps;
            pairs++;
        }
    }

    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    free(opening);

    double mean = pairs ? pair_sum / pairs : 0.0;
    double ci = 0.0;
    if (pairs > 1)
    {
        double var = (pair_sq - pairs * mean * mean) / (pairs - 1);
        if (var < 0.0)
            var = 0.0;
        ci = 1.96 * sqrt(var / pairs);
    }

    printf("%s\n", label);
    printf("  %d games (%d pairs)   W/L/D = %d/%d/%d",
           games, pairs, wins, losses, draws);
    if (unfinished)
        printf("   %d hit ply cap", unfinished);
    if (skipped)
        printf("   %d openings skipped", skipped);
    printf("\n");
    /* a normal-approximation interval can fall outside [0,1] on tiny samples */
    double lo = 100.0 * (mean - ci);
    double hi = 100.0 * (mean + ci);
    if (lo < 0.0)
        lo = 0.0;
    if (hi > 100.0)
        hi = 100.0;

    printf("  score %.1f%%  95%% CI [%.1f%%, %.1f%%]   avg plies %.1f   %.1fs\n\n",
           100.0 * mean, lo, hi,
           games ? (double)total_plies / games : 0.0, secs);
}

int main(int argc, char **argv)
{
    int n_openings = 100;
    if (argc > 1)
        n_openings = atoi(argv[1]);
    if (n_openings < 1)
        n_openings = 1;

    /* Standard board. INITIAL_STONES matters: it is the old evaluator's
       extra-turn bonus magnitude. */
    PITS = 6;
    INITIAL_STONES = 4;
    STATE_SIZE = 2 * (PITS + 1);
    VS_COMP = 1;

    printf("Self-play: fixed evaluate() vs pre-fix evaluate()\n");
    printf("Board %d pits / %d stones, %d random opening plies, "
           "%d openings per depth (x2 seats)\n\n",
           PITS, INITIAL_STONES, OPENING_PLIES, n_openings);

    printf("Search agreement check against the shipped comp_move():\n");
    int mm = selfcheck(4, 40);
    printf("  %d mismatches over 40 positions at depth 4\n\n", mm);

    printf("Move-choice divergence between the two evaluators:\n");
    disagreement_rate(1, 200);
    disagreement_rate(4, 200);
    disagreement_rate(7, 200);
    printf("\n");

    printf("Signed extra-turn bonus, which branch fires (200 positions):\n");
    bonus_branch_census(1, 200);
    bonus_branch_census(4, 200);
    bonus_branch_census(7, 200);
    printf("\n");

    printf("=== FIXED vs PRE-FIX (same depth both sides) ===\n\n");
    int depths[3] = {1, 4, 7};
    for (int i = 0; i < 3; i++)
    {
        char label[80];
        sprintf(label, "Fixed vs pre-fix, depth %d:", depths[i]);
        run_match(label, depths[i], evaluate, depths[i], evaluate_prefix,
                  n_openings);
    }

    printf("=== CONTROLS: same evaluator, different depth ===\n");
    printf("(a harness that cannot detect these cannot detect anything)\n\n");
    run_match("Fixed depth 4 vs fixed depth 1:",
              4, evaluate, 1, evaluate, n_openings);
    run_match("Fixed depth 7 vs fixed depth 4:",
              7, evaluate, 4, evaluate, n_openings);

    return 0;
}
