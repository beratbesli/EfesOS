#include "games.h"
#include "pit.h"
#include "vga.h"

#define SNAKE_WIDTH 20
#define SNAKE_HEIGHT 12
#define SNAKE_MAX_LENGTH 64

enum game_mode {
    GAME_NONE,
    GAME_SNAKE,
    GAME_SLOT
};

struct snake_point {
    int x;
    int y;
};

static enum game_mode active_game;
static struct snake_point snake[SNAKE_MAX_LENGTH];
static struct snake_point food;
static unsigned int snake_length;
static int snake_dx;
static int snake_dy;
static unsigned int snake_score;
static unsigned int snake_tick_count;
static unsigned char snake_finished;
static unsigned int random_state;
static unsigned int slot_credits;
static unsigned int slot_reels[3];
static unsigned int slot_spins;
static unsigned int last_score;

static unsigned int next_random(void)
{
    if (random_state == 0) {
        random_state = pit_ticks() + 0xBEEFU;
    }

    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static int snake_contains(int x, int y)
{
    unsigned int index;

    for (index = 0; index < snake_length; index++) {
        if (snake[index].x == x && snake[index].y == y) {
            return 1;
        }
    }

    return 0;
}

static void snake_place_food(void)
{
    do {
        food.x = (int)(next_random() % SNAKE_WIDTH);
        food.y = (int)(next_random() % SNAKE_HEIGHT);
    } while (snake_contains(food.x, food.y));
}

static void snake_draw(void)
{
    int x;
    int y;
    unsigned int index;

    vga_clear();
    vga_write("AYRANOS SNAKE  score=");
    vga_write_unsigned(snake_score);
    vga_write("  WASD: move  Q: exit\n");
    vga_write("+");
    for (x = 0; x < SNAKE_WIDTH; x++) {
        vga_write("-");
    }
    vga_write("+\n");

    for (y = 0; y < SNAKE_HEIGHT; y++) {
        vga_write("|");
        for (x = 0; x < SNAKE_WIDTH; x++) {
            char cell = ' ';

            if (x == food.x && y == food.y) {
                cell = '*';
            }
            for (index = snake_length; index != 0; index--) {
                if (snake[index - 1].x == x && snake[index - 1].y == y) {
                    cell = index == 1 ? '@' : 'o';
                }
            }
            vga_write_char(cell);
        }
        vga_write("|\n");
    }

    vga_write("+");
    for (x = 0; x < SNAKE_WIDTH; x++) {
        vga_write("-");
    }
    vga_write("+\n");

    if (snake_finished != 0) {
        vga_write("Game over. Press Q to return to the shell.\n");
    }
}

static void snake_move(void)
{
    struct snake_point next;
    unsigned int index;

    next.x = snake[0].x + snake_dx;
    next.y = snake[0].y + snake_dy;

    if (next.x < 0 || next.x >= SNAKE_WIDTH || next.y < 0 || next.y >= SNAKE_HEIGHT || snake_contains(next.x, next.y)) {
        snake_finished = 1;
        snake_draw();
        return;
    }

    for (index = snake_length; index > 1; index--) {
        snake[index - 1] = snake[index - 2];
    }
    snake[0] = next;

    if (next.x == food.x && next.y == food.y) {
        if (snake_length < SNAKE_MAX_LENGTH) {
            snake[snake_length] = snake[snake_length - 1];
            snake_length++;
        }
        snake_score += 10;
        snake_place_food();
    }

    snake_draw();
}

static const char *slot_symbol(unsigned int reel)
{
    static const char *symbols[] = { "B", "E", "E", "R", "$" };

    return symbols[reel % 5U];
}

static void slot_draw(const char *message)
{
    vga_clear();
    vga_write("=== AYRANOS SLOTS ===\n");
    vga_write("+---+---+---+\n");
    vga_write("| ");
    vga_write(slot_symbol(slot_reels[0]));
    vga_write(" | ");
    vga_write(slot_symbol(slot_reels[1]));
    vga_write(" | ");
    vga_write(slot_symbol(slot_reels[2]));
    vga_write(" |\n");
    vga_write("+---+---+---+\n");
    vga_write("Credits: ");
    vga_write_unsigned(slot_credits);
    vga_write("  Spins: ");
    vga_write_unsigned(slot_spins);
    vga_write("\n");
    vga_write(message);
    vga_write("\nSpace: spin  Q: exit\n");
}

static void slot_spin(void)
{
    if (slot_credits == 0) {
        slot_draw("No credits left. Q exits the game.");
        return;
    }

    slot_credits--;
    slot_spins++;
    slot_reels[0] = next_random() % 5U;
    slot_reels[1] = next_random() % 5U;
    slot_reels[2] = next_random() % 5U;

    if (slot_reels[0] == slot_reels[1] && slot_reels[1] == slot_reels[2]) {
        slot_credits += 10;
        slot_draw("JACKPOT! +10 credits");
    } else if (slot_reels[0] == slot_reels[1] || slot_reels[1] == slot_reels[2] || slot_reels[0] == slot_reels[2]) {
        slot_credits += 3;
        slot_draw("Pair! +3 credits");
    } else {
        slot_draw("No match. Try again.");
    }
}

int games_is_active(void)
{
    return active_game != GAME_NONE;
}

void games_start_snake(void)
{
    active_game = GAME_SNAKE;
    snake_length = 3;
    snake[0].x = 10;
    snake[0].y = 6;
    snake[1].x = 9;
    snake[1].y = 6;
    snake[2].x = 8;
    snake[2].y = 6;
    snake_dx = 1;
    snake_dy = 0;
    snake_score = 0;
    snake_tick_count = 0;
    snake_finished = 0;
    snake_place_food();
    snake_draw();
}

void games_start_slot(void)
{
    active_game = GAME_SLOT;
    slot_credits = 10;
    slot_spins = 0;
    slot_reels[0] = 0;
    slot_reels[1] = 1;
    slot_reels[2] = 3;
    slot_draw("Good luck!");
}

int games_handle_char(unsigned char character)
{
    if (character == 'q' || character == 'Q') {
        if (active_game == GAME_SLOT) {
            last_score = slot_credits;
        } else {
            last_score = snake_score;
        }
        active_game = GAME_NONE;
        return 1;
    }

    if (active_game == GAME_SLOT) {
        if (character == ' ') {
            slot_spin();
        }
        return 0;
    }

    if (active_game == GAME_SNAKE && snake_finished == 0) {
        if ((character == 'w' || character == 'W') && snake_dy != 1) {
            snake_dx = 0;
            snake_dy = -1;
        } else if ((character == 's' || character == 'S') && snake_dy != -1) {
            snake_dx = 0;
            snake_dy = 1;
        } else if ((character == 'a' || character == 'A') && snake_dx != 1) {
            snake_dx = -1;
            snake_dy = 0;
        } else if ((character == 'd' || character == 'D') && snake_dx != -1) {
            snake_dx = 1;
            snake_dy = 0;
        }
    }

    return 0;
}

void games_tick(void)
{
    if (active_game != GAME_SNAKE || snake_finished != 0) {
        return;
    }

    snake_tick_count++;
    if (snake_tick_count == 7) {
        snake_tick_count = 0;
        snake_move();
    }
}

unsigned int games_last_score(void)
{
    return last_score;
}
