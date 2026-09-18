#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

int INITIAL_STONES = 4;
int PITS = 6;
int PLAYER = 1;
int VS_COMP = 1;
int COMP_PLAYER = 1;
const char *PLAYER1_TEXT = "Player 1";
const char *PLAYER2_TEXT = "Player 2";
int STATE_SIZE;
int GAME_TREE_DEPTH = 4;
// fixed, not tied to INITIAL_STONES, so a configurable board cannot let
// the tempo bonus outweigh a real store lead
const int EXTRA_TURN_BONUS = 2;

int *state = NULL;
int game_over = 0;

// user input validator function
static int get_valid_int_input(const char *prompt, int min, int max, int default_val);
void game(void);

// stdin has closed. Retrying the read would spin forever, so quit instead.
static void handle_eof(void)
{
    printf("\nNo more input. Exiting game...\n");
    if (state != NULL)
        free(state);
    exit(EXIT_SUCCESS);
}

// function to draw the board on the terminal after each turn.
void draw_board(int current_state[])
{
    printf("\n");
    printf("                 %s's Pits\n\n", PLAYER1_TEXT);

    // Pit numbers
    printf("Pit Number:       ");
    for (int i = 1; i < PITS + 1; i++)
    {
        printf("%2d", i);

        if (i < PITS)
            printf("    ");
    }

    printf("\n");

    // Number of stones
    printf("Stones:           ");
    for (int i = 1; i < PITS + 1; i++)
    {
        printf("%2d", current_state[i]);

        if (i < PITS)
            printf(" <- ");
    }

    printf("\n\n");

    // decide store text to show
    const char *store1_txt = "P1";
    const char *store2_txt = "P2";
    if (VS_COMP == 1)
    {
        if (COMP_PLAYER == 1)
        {
            store1_txt = "CO";
            store2_txt = "PL";
        }
        else
        {
            store1_txt = "PL";
            store2_txt = "CO";
        }
    }

    // print stores
    printf("Stores:        [%s] %2d                       %2d [%s]\n\n",
           store1_txt, current_state[0], current_state[PITS + 1], store2_txt);

    // Number of stones
    printf("Stones:           ");
    for (int i = 2 * PITS + 1; i >= PITS + 2; i--)
    {
        printf("%2d", current_state[i]);

        if (i > PITS + 2)
            printf(" -> ");
    }

    printf("\n");

    // Pit numbers
    printf("Pit Number:       ");
    for (int i = 1; i < PITS + 1; i++)
    {
        printf("%2d", i);

        if (i < PITS)
            printf("    ");
    }

    printf("\n\n");

    printf("                 %s's Pits\n", PLAYER2_TEXT);

    printf("\n----------------------------------------------------\n");
}

// declare the winner and end the game
void end_game(const int winner)
{
    // draw final borad state for players to see
    printf("----------FINAL BOARD STATE----------");
    draw_board(state);

    // game over
    game_over = 1;

    // print final scores
    printf("---FINAL SCORES---\n");
    printf("%s : %d\n", PLAYER1_TEXT, state[0]);
    printf("%s : %d\n", PLAYER2_TEXT, state[PITS + 1]);

    // declare winner
    if (winner == 0)
        printf("The game ended in a draw!\n");
    else if (winner == 1)
        printf("%s HAS WON THE GAME!\n", PLAYER1_TEXT);
    else
        printf("%s HAS WON THE GAME!\n", PLAYER2_TEXT);

    printf("--- GAME OVER ---\n\n");
}

// check winner
// -1 for no winner yet
// 0 for draw
// 1 for Player 1 win
// 2 for Player 2 win
int check_win(int current_state[])
{
    // sum all stones on each side
    int player1_sum = 0;
    int player2_sum = 0;
    for (int i = 1; i < PITS + 1; i++)
        player1_sum += current_state[i];
    for (int i = PITS + 2; i < STATE_SIZE; i++)
        player2_sum += current_state[i];

    // find number of stones in each store
    int player1_store = current_state[0];
    int player2_store = current_state[PITS + 1];

    // score = store + leftover stones a/c to rules
    int player1_score = player1_store + player1_sum;
    int player2_score = player2_store + player2_sum;

    // if sum of one side is empty => that side has no stones
    // because number of stones cannot be negative
    if (player1_sum == 0 || player2_sum == 0)
    {
        // pickup leftover stones and put it respective stores.
        for (int i = 1; i < PITS + 1; i++)
            current_state[i] = 0;
        for (int i = PITS + 2; i < STATE_SIZE; i++)
            current_state[i] = 0;
        current_state[0] += player1_sum;
        current_state[PITS + 1] += player2_sum;

        // decide winner or draw
        if (player1_score > player2_score)
            return 1;
        else if (player2_score > player1_score)
            return 2;
        else
            return 0;
    }

    // now winner if one side is not empty
    return -1;
}

// make the move
void move(int current_state[], int chosen_pit, int *legal_move, int current_player, int *extra_turn, int *winner, int *captured)
{
    // map choosen pit to index of state
    int idx = (current_player == 1) ? chosen_pit : (STATE_SIZE)-chosen_pit;

    // find number of stones in the pit.
    int stones = current_state[idx];

    // verify if move is legal
    if (stones == 0)
    {
        *legal_move = 0;
        return;
    }
    else
        *legal_move = 1;

    // counter clockwise distribution
    current_state[idx] = 0;
    *extra_turn = 0;
    *captured = 0;
    int player_store_idx = (current_player - 1) * (PITS + 1);
    int opponent_store_idx = (2 - current_player) * (PITS + 1);
    while (stones > 0)
    {
        idx--;

        // wrap around if going out of bounds
        if (idx == -1)
            idx = (2 * PITS) + 1;

        // skip opponent's store
        if (idx == opponent_store_idx)
            continue;

        // check for extra turn
        if (stones == 1 && idx == player_store_idx)
            *extra_turn = 1;

        // check for capture
        if (stones == 1 && current_state[idx] == 0 && idx >= (player_store_idx + 1) && idx <= (player_store_idx + PITS))
        {
            current_state[player_store_idx] += current_state[(STATE_SIZE)-idx] + 1;
            current_state[STATE_SIZE - idx] = 0;
            *captured = 1;
        }
        else
            current_state[idx]++;

        // next
        stones--;
    }

    *winner = check_win(current_state);
}

// node for game tree (for determination of computer move)
typedef struct Node
{
    int *current_state;
    int current_player;
    int move;
    int value;
    int extra_turn;
    int winner;
    struct Node **children;
    int child_count;
} Node;

static int *copy_state(const int source[])
{
    int *copy = malloc((size_t)STATE_SIZE * sizeof(int));
    if (copy == NULL)
    {
        printf("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, source, (size_t)STATE_SIZE * sizeof(int));
    return copy;
}

static Node *create_node(const int current_state[], int current_player, int move_number)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        printf("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    node->current_state = copy_state(current_state);
    node->current_player = current_player;
    node->move = move_number;
    node->value = 0;
    node->extra_turn = 0;
    node->winner = -1;
    node->children = NULL;
    node->child_count = 0;
    return node;
}

static void free_tree(Node *node)
{
    if (node == NULL)
        return;
    for (int i = 0; i < node->child_count; i++)
    {
        free_tree(node->children[i]);
    }
    free(node->children);
    free(node->current_state);
    free(node);
}

static int evaluate(const Node *node)
{
    int human_player = 3 - COMP_PLAYER;
    int computer_store_idx = (COMP_PLAYER - 1) * (PITS + 1);
    int human_store_idx = (human_player - 1) * (PITS + 1);

    int computer_store = node->current_state[computer_store_idx];
    int human_store = node->current_state[human_store_idx];

    // Terminal states
    if (node->winner == COMP_PLAYER)
        return 100000;
    if (node->winner == human_player)
        return -100000;
    if (node->winner == 0)
        return 0;

    // Heuristic: Store differential + extra turn bonus
    int val = computer_store - human_store;
    if (node->extra_turn)
    {
        // build_tree_alphabeta leaves current_player unchanged on an extra
        // turn, so the player to move here is the one who earned it
        if (node->current_player == COMP_PLAYER)
            val += EXTRA_TURN_BONUS;
        else
            val -= EXTRA_TURN_BONUS;
    }

    return val;
}

static int build_tree_alphabeta(Node *node, int depth, int alpha, int beta)
{
    // terminal condition check
    if (depth <= 0 || node->winner != -1)
    {
        node->value = evaluate(node);
        return node->value;
    }

    node->children = malloc((size_t)PITS * sizeof(Node *));
    node->child_count = 0;

    int is_max = (node->current_player == COMP_PLAYER);
    node->value = is_max ? INT_MIN : INT_MAX;

    for (int i = 0; i < PITS; i++)
    {
        int *temp_state = copy_state(node->current_state);

        int legal_move = 1;
        int extra_turn = 0;
        int winner = -1;
        int captured = 0;

        move(temp_state, i + 1, &legal_move, node->current_player, &extra_turn, &winner, &captured);

        if (legal_move)
        {
            int next_player = extra_turn ? node->current_player : (3 - node->current_player);

            Node *child = create_node(temp_state, next_player, i + 1);
            child->extra_turn = extra_turn;
            child->winner = winner;

            node->children[node->child_count++] = child;

            int child_val = build_tree_alphabeta(child, depth - 1, alpha, beta);

            if (is_max)
            {
                if (child_val > node->value)
                    node->value = child_val;

                if (node->value > alpha)
                    alpha = node->value;
            }
            else
            {
                if (child_val < node->value)
                    node->value = child_val;

                if (node->value < beta)
                    beta = node->value;
            }

            free(temp_state);

            // pruning
            if (alpha >= beta)
                break;
        }
        else
            free(temp_state);
    }

    if (node->child_count == 0)
        node->value = evaluate(node);

    return node->value;
}

// computer move
int comp_move()
{
    int final_move = 1;

    Node *root = create_node(state, PLAYER, 0);

    build_tree_alphabeta(root, GAME_TREE_DEPTH, INT_MIN, INT_MAX);

    int max_val = INT_MIN;
    for (int i = 0; i < root->child_count; i++)
        if (root->children[i]->value > max_val)
        {
            max_val = root->children[i]->value;
            final_move = root->children[i]->move;
        }

    free_tree(root);

    return final_move;
}

/* ============================================================
   SAVE / LOAD GAME FUNCTIONALITY
   ============================================================ */

static void save_game()
{
    printf("\n----- SAVE GAME -----\n");
    printf("Select a save slot (1-3):\n");
    int slot = get_valid_int_input("Choose Slot (1-3): ", 1, 3, 1);

    char filename[20];
    sprintf(filename, "slot%d.txt", slot);

    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        printf("Error: Could not save game to %s\n", filename);
        return;
    }

    fprintf(fp, "%d %d %d %d %d %d\n", INITIAL_STONES, PITS, PLAYER, VS_COMP, COMP_PLAYER, GAME_TREE_DEPTH);
    for (int i = 0; i < STATE_SIZE; i++)
    {
        fprintf(fp, "%d ", state[i]);
    }
    fprintf(fp, "\n");

    fclose(fp);
    printf("Game saved successfully to Slot %d!\n\n", slot);
}

void load_game()
{
    printf("\n----- LOAD GAME -----\n");
    printf("Select a slot to load (1-3):\n");
    int slot = get_valid_int_input("Choose Slot (1-3): ", 1, 3, 1);

    char filename[20];
    sprintf(filename, "slot%d.txt", slot);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("No saved game found in Slot %d!\n\n", slot);
        return;
    }

    // read into locals first, so a corrupted file cannot leave the globals
    // half-updated and out of step with the board array
    int stones, pits, player, vs_comp, comp_player, depth;
    if (fscanf(fp, "%d %d %d %d %d %d", &stones, &pits, &player, &vs_comp, &comp_player, &depth) != 6)
    {
        printf("Error: Save file in Slot %d is corrupted!\n", slot);
        fclose(fp);
        return;
    }

    // reject anything a real game could never have produced
    if (pits < 1 || pits > 10 || stones < 1 || stones > 12 ||
        player < 1 || player > 2 || vs_comp < 0 || vs_comp > 1 ||
        comp_player < 0 || comp_player > 2 || depth < 1)
    {
        printf("Error: Save file in Slot %d is corrupted!\n", slot);
        fclose(fp);
        return;
    }

    int state_size = 2 * (pits + 1);

    int *loaded = malloc((size_t)state_size * sizeof(int));
    if (loaded == NULL)
    {
        printf("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < state_size; i++)
    {
        if (fscanf(fp, "%d", &loaded[i]) != 1 || loaded[i] < 0)
        {
            printf("Error reading board state from Slot %d!\n", slot);
            free(loaded);
            fclose(fp);
            return;
        }
    }

    fclose(fp);

    // whole file read successfully, so commit it
    INITIAL_STONES = stones;
    PITS = pits;
    PLAYER = player;
    VS_COMP = vs_comp;
    COMP_PLAYER = comp_player;
    GAME_TREE_DEPTH = depth;
    STATE_SIZE = state_size;

    if (state != NULL)
        free(state);
    state = loaded;

    game_over = 0;

    // restore display texts
    if (VS_COMP == 1)
    {
        if (COMP_PLAYER == 1)
        {
            PLAYER1_TEXT = "Computer";
            PLAYER2_TEXT = "Player";
        }
        else
        {
            PLAYER1_TEXT = "Player";
            PLAYER2_TEXT = "Computer";
        }
    }
    else
    {
        PLAYER1_TEXT = "Player 1";
        PLAYER2_TEXT = "Player 2";
    }

    printf("Game loaded successfully from Slot %d!\n", slot);
    game();
}

// main game loop
void game()
{
    while (!game_over)
    {
        draw_board(state);

        int pit = 0;

        // check if it's computer's turn
        if (VS_COMP && PLAYER == COMP_PLAYER)
        {
            pit = comp_move();
        }
        else
        {
            // user input loop
            if (PLAYER == 1)
                printf("%s, choose a pit (1-%d, or enter 0/'S' to Save): ", PLAYER1_TEXT, PITS);
            else
                printf("%s, choose a pit (1-%d, or enter 0/'S' to Save): ", PLAYER2_TEXT, PITS);

            char input_buf[100];
            if (fgets(input_buf, sizeof(input_buf), stdin) == NULL)
                handle_eof();

            // Handle Save Request
            if (input_buf[0] == 'S' || input_buf[0] == 's' || input_buf[0] == '0')
            {
                save_game();
                continue;
            }

            if (sscanf(input_buf, "%d", &pit) != 1)
            {
                printf("Invalid input. Please enter a valid pit number or 'S' to save.\n");
                continue;
            }
        }

        // verify chosen pit is actually valid
        if (pit < 1 || pit > PITS)
        {
            printf("Invalid pit number. Choose again.\n");
            continue;
        }

        // print for info
        if (PLAYER == 1)
            printf("%s chose pit %d.\n", PLAYER1_TEXT, pit);
        else
            printf("%s chose pit %d.\n", PLAYER2_TEXT, pit);

        // make the move
        int legal_move = 1;
        int extra_turn = 0;
        int winner = -1;
        int captured = 0;
        move(state, pit, &legal_move, PLAYER, &extra_turn, &winner, &captured);

        if (!legal_move)
        {
            printf("No stones in this pit. Choose another pit.\n");
            continue;
        }
        else if (winner != -1)
        {
            end_game(winner);
            return; // Returns control to menu after game ends
        }
        else if (!extra_turn)
        {
            if (captured)
            {
                if (PLAYER == 1)
                    printf("Last stone landed on an empty pit on %s's side!\n", PLAYER1_TEXT);
                else
                    printf("Last stone landed on an empty pit on %s's side!\n", PLAYER2_TEXT);
                printf("Opposite pit and last stone were captured!\n");
            }
            PLAYER = 3 - PLAYER;
        }
        else
        {
            if (PLAYER == 1)
            {
                printf("Last stone landed in %s's store!\n", PLAYER1_TEXT);
                printf("%s gets an extra turn!\n", PLAYER1_TEXT);
            }
            else
            {
                printf("Last stone landed in %s's store!\n", PLAYER2_TEXT);
                printf("%s gets an extra turn!\n", PLAYER2_TEXT);
            }
        }
        printf("----------------------------------------------------\n");
    }
}

// helper function to handle integer inputs with custom range validation
static int get_valid_int_input(const char *prompt, int min, int max, int default_val)
{
    int val;
    while (1)
    {
        printf("%s", prompt);

        // Read line to safely capture empty inputs (pressing Enter defaults)
        char line[100];
        if (fgets(line, sizeof(line), stdin) == NULL)
            handle_eof();

        // If user presses Enter without typing, use default
        if (line[0] == '\n')
            return default_val;

        if (sscanf(line, "%d", &val) == 1)
        {
            if (val >= min && val <= max)
                return val;
        }

        printf("Invalid choice! Please enter a number between %d and %d.\n", min, max);
    }
}

// Menu for choosing game mode (PvE or PvP)
static int select_game_mode()
{
    printf("\n----- SELECT GAME MODE -----\n");
    printf("1. Player vs Computer\n");
    printf("2. Player vs Player (Local)\n");
    return get_valid_int_input("Choose mode (Default 1): ", 1, 2, 1);
}

// Menu for deciding who goes first
static int select_first_player()
{
    printf("\n----- WHO GOES FIRST? -----\n");
    printf("1. Computer goes first\n");
    printf("2. You go first\n");
    return get_valid_int_input("Choose option (Default 2): ", 1, 2, 2);
}

// Menu for AI difficulty (sets Minimax depth)
static int select_difficulty()
{
    printf("\n----- SELECT DIFFICULTY -----\n");
    printf("1. Easy   (Depth 1 - Quick/Basic Moves)\n");
    printf("2. Medium (Depth 4 - Balanced)\n");
    printf("3. Hard   (Depth 7 - Deep Lookahead)\n");

    int choice = get_valid_int_input("Choose difficulty (Default 2): ", 1, 3, 2);

    switch (choice)
    {
    case 1:
        return 1;
    case 2:
        return 4;
    case 3:
        return 7;
    default:
        return 4;
    }
}

// Setup custom board settings
static void configure_board()
{
    printf("\n----- BOARD SETUP -----\n");
    PITS = get_valid_int_input("Enter number of pits per side (1-10, Default 6): ", 1, 10, 6);
    INITIAL_STONES = get_valid_int_input("Enter initial stones per pit (1-12, Default 4): ", 1, 12, 4);
}

// start new game
static void start_new_game()
{
    // configure Board Pits and Stones
    configure_board();

    // configure Game Mode (VS Computer or Player)
    int mode = select_game_mode();
    if (mode == 1)
    {
        VS_COMP = 1;

        // ask who goes first
        int first = select_first_player();
        COMP_PLAYER = (first == 1) ? 1 : 2;

        // ask for difficulty
        GAME_TREE_DEPTH = select_difficulty();
    }
    else
    {
        VS_COMP = 0;
        COMP_PLAYER = 0;
    }

    // free previously allocated state array if playing multiple games
    if (state != NULL)
        free(state);

    // initialize board array memory based on chosen pit count
    STATE_SIZE = 2 * (PITS + 1);
    state = malloc((size_t)STATE_SIZE * sizeof(int));
    if (state == NULL)
    {
        printf("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    // initialise game state
    for (int i = 0; i < STATE_SIZE; i++)
        state[i] = INITIAL_STONES;
    state[0] = 0;
    state[PITS + 1] = 0;
    game_over = 0;
    PLAYER = 1;

    // set player display texts
    if (VS_COMP == 1)
    {
        if (COMP_PLAYER == 1)
        {
            PLAYER1_TEXT = "Computer";
            PLAYER2_TEXT = "Player";
        }
        else
        {
            PLAYER1_TEXT = "Player";
            PLAYER2_TEXT = "Computer";
        }
    }
    else
    {
        PLAYER1_TEXT = "Player 1";
        PLAYER2_TEXT = "Player 2";
    }

    // start turn loop
    game();
}

void show_rules()
{
    printf("\n");
    printf("====================== RULES ======================\n\n");

    printf("1. Each player has 6 pits (by default) and one store.\n");
    printf("2. Each pit starts with 4 stones (by default).\n");
    printf("3. On your turn, choose a non-empty pit on your side.\n");
    printf("4. Distribute the stones counterclockwise, one at a time.\n");
    printf("5. Skip the opponent's store while distributing.\n");
    printf("6. If the last stone lands in your store, you get another turn.\n");
    printf("7. If the last stone lands in an empty pit on your side,\n");
    printf("   you capture that stone and all stones in the opposite pit.\n");
    printf("8. The game ends when all pits on one side are empty.\n");
    printf("9. The remaining stones on the board are added to their\n");
    printf("   respective stores.\n");
    printf("10. The player with the most stones in their store wins.\n");

    printf("\n===================================================\n");
}

void show_credits()
{
    printf("\n");
    printf("=============== CREDITS ===============\n\n");

    printf("Mancala - C Programming Assignment\n\n");
    printf("Developed by: Siddharth Narayan Mishra\n");
    printf("Roll No. : 123CS0189\n\n");

    printf("GitHub: https://github.com/sidd233\n");

    printf("\n========================================\n");
}

// let user choose options from the menu
static int menu_options()
{
    printf("1. New Game\n");
    printf("2. Load Game\n");
    printf("3. Rules\n");
    printf("4. Credits\n");
    printf("5. Exit\n\n");

    return get_valid_int_input("Choose option: ", 1, 5, 1);
}

// init function
static void init()
{
    printf("\n");
    printf(" __  __    _    _   _  ____    _    _        _    \n");
    printf("|  \\/  |  / \\  | \\ | |/ ___|  / \\  | |      / \\   \n");
    printf("| |\\/| | / _ \\ |  \\| | |     / _ \\ | |     / _ \\  \n");
    printf("| |  | |/ ___ \\| |\\  | |___ / ___ \\| |___ / ___ \\ \n");
    printf("|_|  |_/_/   \\_\\_| \\_|\\____/_/   \\_\\____//_/   \\_\\\n");
    printf("\n\n");

    while (1)
    {
        int option = menu_options();

        switch (option)
        {
        case 1:
            start_new_game();
            break;
        case 2:
            load_game();
            break;
        case 3:
            show_rules();
            printf("\n");
            break;
        case 4:
            show_credits();
            printf("\n");
            break;
        case 5:
            printf("Exiting game...\n");
            if (state != NULL)
                free(state);
            return;
        default:
            break;
        }
    }
}

int main()
{
    init();
}