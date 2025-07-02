// snake_game.cpp - 최종 통합 버전
// Compile: clang++ -std=c++17 -g snake_game.cpp -o snake_game -lncurses
// Run: ./snake_game

#include <algorithm> // for std::sort
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <locale.h>
#include <ncurses.h>
#include <random>
#include <string.h> // For strlen
#include <string>
#include <unistd.h>
#include <vector>

#define HEIGHT 21
#define WIDTH 21
#define ITEM_LIFESPAN 300
#define IMMUNE_WALL 9
#define STAGES 4
#define GATE_LIFESPAN_TICKS 100 // How long a gate remains active (in ticks)
#define GATE_COOLDOWN_TICKS 5   // Cooldown after using a gate (in ticks)

// PlayerInfo 구조체 정의
struct PlayerInfo {
    std::string name;
    int         score;

    // 랭킹 정렬을 위한 비교 연산자 (점수 내림차순)
    bool operator>(const PlayerInfo &other) const { return score > other.score; }
};

void initStage(int stage);
void initColors();
void inputPlayerName();
void loadHighScore();
void saveHighScore(int score);
int  showMenuScreen();
int  showRulesScreen();
void showGameOverScreen(int finalScore);
void drawScoreboard();
void drawMissionBoard();
void drawMap();
bool checkMissionClear();
void spawnGrowthItem();
void spawnPoisonItem();
void spawnGates();
void moveSnake();
void updateDirection(int ch);
void playGame();
void showRankingScreen();

// --- Globals (ensure these are consistent with your original file) ---

// --- Game State Variables ---
bool gameOver       = false; // Set to true when game ends
int  gameOverReason = 0; // 0: no reason, 1: U-turn, 2: wall, 3: self-collision, 4: score-out, 5:
                         // length<3, 6: gate cooldown
bool gameWon = false;    // Set to true when all stages are cleared

int currentStage = 0; // Tracks the current stage (0-indexed)

// Snake related variables
std::deque<std::pair<int, int>> snake;            // The snake's body segments
int                             headY, headX;     // Snake's head coordinates
int                             dirIndex     = 3; // Initial direction (3: RIGHT)
int                             prevDirIndex = 3; // Previous direction

// Map and Item related
int  map[HEIGHT][WIDTH];                  // The game map
int  itemFrame           = ITEM_LIFESPAN; // Timer for items
bool gateSpawned         = false;         // Flag to check if gates are on map
int  gateLifetimeCounter = 0;             // Tracks gate lifespan
int  gateEntryY = -1, gateEntryX = -1;    // Coordinates of the entry gate
int  gateExitY = -1, gateExitX = -1;      // Coordinates of the exit gate

double innerWallProbability[STAGES] = {1.5, 2.5, 3.5, 4.5};

// --- Score & Mission Progress Variables ---
int collected_growth_items = 0; // Number of growth items collected in current stage
int collected_poison_items = 0; // Number of poison items collected in current stage
int gates_used_count       = 0; // Number of gates used in current stage

// Mission status flags for display
bool missionStatusLength[STAGES] = {false};
bool missionStatusGrowth[STAGES] = {false};
bool missionStatusPoison[STAGES] = {false};
bool missionStatusGate[STAGES]   = {false};

// Player scores (these are reset in main() already, but ensure they are global)
int total_score_growth = 0;
int total_score_poison = 0;
int total_score_gate   = 0;
int maxLengthAchieved  = 3;

// Turn counter
int stageTurnCounter = 0;

int  length;
int  direction;
bool paused = false;

int DELAY = 300000;

int mission_length = 6;
int mission_growth = 5;
int mission_poison = 2;
int mission_gate   = 2;

int growthItemSpawnInterval = 20;
int poisonItemSpawnInterval = 35;
int frameCounter            = 0;

int gateRespawnIntervalPerStage[STAGES] = {100, 75, 50, 40};    // Adjusted for STAGES
int stageTurnLimitPerStage[STAGES]      = {500, 400, 300, 250}; // Adjusted for STAGES

int gateRespawnInterval = 50;

int gateLifespan = 100;

int mission_length_per_stage[STAGES] = {6, 9, 12, 15};
int mission_growth_per_stage[STAGES] = {5, 7, 9, 11};
int mission_poison_per_stage[STAGES] = {2, 4, 6, 8};
int mission_gate_per_stage[STAGES]   = {2, 3, 4, 5};
int delay_per_stage[STAGES]          = {220000, 180000, 120000, 60000};

// These variables are unused but declared for consistency
int currentGrowthItemsCollected = 0; // unused
int currentPoisonItemsCollected = 0; // unused

const int maxGrowthItems = 3; // spawned concurrently
const int maxPoisonItems = 3;

std::wstring playerName = L"";
int          highScore  = 0;

std::pair<int, int> gateA = {-1, -1};
std::pair<int, int> gateB = {-1, -1};

std::random_device rd;
std::mt19937       gen(rd());

enum Direction { UP = 0, DOWN, LEFT, RIGHT };

void initialize_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_YELLOW, COLOR_BLACK); // Snake head (original: COLOR_YELLOW)
        init_pair(2, COLOR_RED, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // For growth item and snake head
        init_pair(4, COLOR_BLUE, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLACK);
        init_pair(6, COLOR_CYAN, COLOR_BLACK);
        init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    }
}

void updateDirection(int ch) {
    if (paused && ch == 'p') {
        paused = false;
        return;
    }
    if (ch == 'p') {
        paused = true;
        return;
    }

    int newDir = dirIndex;

    switch (ch) {
    case KEY_UP:
        newDir = UP;
        break;
    case KEY_DOWN:
        newDir = DOWN;
        break;
    case KEY_LEFT:
        newDir = LEFT;
        break;
    case KEY_RIGHT:
        newDir = RIGHT;
        break;
    }

    bool isUturn = false;
    if ((dirIndex == UP && newDir == DOWN) || (dirIndex == DOWN && newDir == UP) ||
        (dirIndex == LEFT && newDir == RIGHT) || (dirIndex == RIGHT && newDir == LEFT)) {
        isUturn = true;
    }

    if (isUturn) {
        gameOverReason = 1; // U-턴으로 인한 게임 오버 이유 설정
    }

    if (newDir != dirIndex && !isUturn) { // Only change direction if not a U-turn
        prevDirIndex = dirIndex;
        dirIndex     = newDir;
    }
}

bool isBorderWall(int y, int x) {
    return (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) && map[y][x] == 1;
}

int calculateExitDirection(int exitGateY, int exitGateX, int entryDirection) {
    int dy[4] = {-1, 1, 0, 0}; // UP, DOWN, LEFT, RIGHT
    int dx[4] = {0, 0, -1, 1};

    // Rule 1: Gate at map edge (border wall)
    if (exitGateY == 0 && map[exitGateY][exitGateX] == 1)
        return DOWN; // Top border -> Down
    if (exitGateY == HEIGHT - 1 && map[exitGateY][exitGateX] == 1)
        return UP; // Bottom border -> Up
    if (exitGateX == 0 && map[exitGateY][exitGateX] == 1)
        return RIGHT; // Left border -> Right
    if (exitGateX == WIDTH - 1 && map[exitGateY][exitGateX] == 1)
        return LEFT; // Right border -> Left

    // Rule 2: Gate in the middle of the map (not edge)
    int clockwise[4]        = {3, 2, 0, 1}; // UP->RIGHT, DOWN->LEFT, LEFT->UP, RIGHT->DOWN
    int counterClockwise[4] = {2, 3, 1, 0}; // UP->LEFT, DOWN->RIGHT, LEFT->DOWN, RIGHT->UP
    int opposite[4]         = {1, 0, 3, 2}; // UP->DOWN, DOWN->UP, LEFT->RIGHT, RIGHT->LEFT

    std::vector<int> possibleDirections;
    // Priority 1: Same as entry direction
    possibleDirections.push_back(entryDirection);
    // Priority 2: Clockwise rotation
    possibleDirections.push_back(clockwise[entryDirection]);
    // Priority 3: Counter-clockwise rotation
    possibleDirections.push_back(counterClockwise[entryDirection]);
    // Priority 4: Opposite direction
    possibleDirections.push_back(opposite[entryDirection]);

    for (int d : possibleDirections) {
        int nextY = exitGateY + dy[d];
        int nextX = exitGateX + dx[d];

        if (nextY >= 0 && nextY < HEIGHT && nextX >= 0 && nextX < WIDTH &&
            (map[nextY][nextX] == 0 || map[nextY][nextX] == 4 || map[nextY][nextX] == 2)) {
            return d;
        }
    }
    // Fallback (should be rare if map gen is good)
    for (int d = 0; d < 4; ++d) { // Check all four directions as a last resort
        int nextY = exitGateY + dy[d];
        int nextX = exitGateX + dx[d];
        if (nextY >= 0 && nextY < HEIGHT && nextX >= 0 && nextX < WIDTH &&
            (map[nextY][nextX] == 0 || map[nextY][nextX] == 4 || map[nextY][nextX] == 2)) {
            return d;
        }
    }
    return entryDirection; // Last resort: stick to entry if all else fails
}

void teleportThroughGate(int &y, int &x, int entryDirection) {
    int dy[4] = {-1, 1, 0, 0}; // UP, DOWN, LEFT, RIGHT
    int dx[4] = {0, 0, -1, 1};

    std::pair<int, int> otherGate;
    int                 exitGateY, exitGateX;

    if (y == gateA.first && x == gateA.second) {
        otherGate = gateB;
    } else {
        otherGate = gateA;
    }

    exitGateY = otherGate.first;
    exitGateX = otherGate.second;

    int newExitDirection = calculateExitDirection(exitGateY, exitGateX, entryDirection);

    // Place the snake head *outside* the exit gate, in the new direction of travel
    y = exitGateY + dy[newExitDirection];
    x = exitGateX + dx[newExitDirection];

    dirIndex = newExitDirection; // Update snake's global direction
}


void moveSnake() {
    static int gateCooldown = 0;
    int        dy[4]        = {-1, 1, 0, 0};
    int        dx[4]        = {0, 0, -1, 1};

    int ny = headY + dy[dirIndex];
    int nx = headX + dx[dirIndex];

    // Item expiration
    if (itemFrame > 0) {
        if (--itemFrame == 0) {
            for (int y = 0; y < HEIGHT; ++y)
                for (int x = 0; x < WIDTH; ++x)
                    if (map[y][x] == 4 || map[y][x] == 2)
                        map[y][x] = 0;
        }
    }

    // Boundary collision
    if (ny < 0 || ny >= HEIGHT || nx < 0 || nx >= WIDTH) {
        gameOverReason = 2;
        return;
    }

    int tgt = map[ny][nx];
    // Wall or self collision
    if (tgt != 5 && (tgt == 1 || tgt == IMMUNE_WALL || tgt == 3)) {
        gameOverReason = (tgt == 3 ? 3 : 2);
        return;
    }

    bool grew = false;

    if (tgt == 4) { // Growth
        collected_growth_items++;
        total_score_growth += 10;
        grew        = true; // skip tail removal
        map[ny][nx] = 0;
        spawnGrowthItem();
    } else if (tgt == 2) { // Poison
        collected_poison_items++;
        total_score_poison -= 5;
        if (!snake.empty()) {
            auto tail                    = snake.back();
            map[tail.first][tail.second] = 0;
            snake.pop_back(); // remove exactly 1 segment
            if (snake.size() < 3) {
                gameOverReason = 5;
                return;
            }
        }
        // grew remains false after eating poison (do not set grew = true here)
        map[ny][nx] = 0;
        spawnPoisonItem();
    } else if (tgt == 5) { // Gate
        if (gateCooldown > 0) {
            gameOverReason = 6;
            return;
        }
        gates_used_count++;
        total_score_gate += 20;
        int entry = dirIndex;
        teleportThroughGate(ny, nx, entry);
        if (ny < 0 || ny >= HEIGHT || nx < 0 || nx >= WIDTH || map[ny][nx] == 1 ||
            map[ny][nx] == IMMUNE_WALL || map[ny][nx] == 3) {
            gameOverReason = (map[ny][nx] == 3 ? 3 : 2);
            return;
        }
        gateCooldown = GATE_COOLDOWN_TICKS;
    }

    // Normal move tail removal
    if (!grew) {
        if (!snake.empty()) {
            auto tail                    = snake.back();
            map[tail.first][tail.second] = 0;
            snake.pop_back();
        }
    }

    // Advance head
    headY = ny;
    headX = nx;
    snake.push_front({headY, headX});
    map[headY][headX] = 3;

    // Turn counter & limits
    if (++stageTurnCounter > stageTurnLimitPerStage[currentStage]) {
        gameOverReason = 4;
        return;
    }

    // Track max length
    maxLengthAchieved = std::max(maxLengthAchieved, (int)snake.size());

    // Gate cooldown tick
    if (gateCooldown > 0)
        --gateCooldown;

    prevDirIndex = dirIndex;
}

void spawnGrowthItem() {
    int count = 0;
    for (int i = 0; i < HEIGHT; ++i)
        for (int j = 0; j < WIDTH; ++j)
            if (map[i][j] == 4)
                count++;
    if (count >= maxGrowthItems)
        return;

    int y, x;
    do {
        y = rand() % HEIGHT;
        x = rand() % WIDTH;
    } while (map[y][x] != 0); // Ensure empty spot
    map[y][x] = 4;             // Growth item
    itemFrame = ITEM_LIFESPAN; // Reset item lifespan
}

void spawnPoisonItem() {
    int count = 0;
    for (int i = 0; i < HEIGHT; ++i)
        for (int j = 0; j < WIDTH; ++j)
            if (map[i][j] == 2)
                count++;
    if (count >= maxPoisonItems)
        return;

    int y, x;
    do {
        y = rand() % HEIGHT;
        x = rand() % WIDTH;
    } while (map[y][x] != 0); // Ensure empty spot
    map[y][x] = 2;             // Poison item
    itemFrame = ITEM_LIFESPAN; // Reset item lifespan
}

void spawnGates() {
    // Clear old gates first
    if (gateA.first != -1)
        map[gateA.first][gateA.second] = 1; // Revert to wall
    if (gateB.first != -1)
        map[gateB.first][gateB.second] = 1; // Revert to wall
    gateA = {-1, -1};
    gateB = {-1, -1};

    std::vector<std::pair<int, int>> wallCandidates;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (map[y][x] == 1) { // Only normal walls can become gates
                // Check if there's an adjacent empty space for snake to exit *into*
                bool hasValidExitSpace = false;
                int  dy[]              = {-1, 1, 0, 0};
                int  dx[]              = {0, 0, -1, 1};
                for (int i = 0; i < 4; ++i) {
                    int adjY = y + dy[i];
                    int adjX = x + dx[i];
                    if (adjY >= 0 && adjY < HEIGHT && adjX >= 0 && adjX < WIDTH &&
                        map[adjY][adjX] == 0) {
                        hasValidExitSpace = true;
                        break;
                    }
                }
                if (hasValidExitSpace) {
                    wallCandidates.push_back({y, x});
                }
            }
        }
    }

    if (wallCandidates.size() < 2)
        return; // Not enough walls to form a pair of gates

    std::shuffle(wallCandidates.begin(), wallCandidates.end(), gen);

    gateA = wallCandidates[0];
    gateB = wallCandidates[1];

    map[gateA.first][gateA.second] = 5; // Mark as gate
    map[gateB.first][gateB.second] = 5; // Mark as gate
    gateLifetimeCounter            = 0; // Reset gate lifespan timer
}

// void initStage(int stage) {
//     // 1. Clear any old snake body
//     snake.clear();

//     // 2. Compute center
//     headY = HEIGHT / 2;
//     headX = WIDTH / 2;

//     // 3. Place exactly 3 segments: head + 2 body
//     // Place initial snake of length 3 at center (head + 2 body)
//     snake.push_front({headY, headX});    // head
//     snake.push_back({headY, headX - 1}); // body segment 1
//     snake.push_back({headY, headX - 2}); // body segment 2

//     // 4. (Optional) Reset your length variable if you use it
//     length = 3;

//     // 5. Mark them on the map
//     for (auto &seg : snake) {
//         map[seg.first][seg.second] = 3;
//     }


//     stageTurnCounter = 0;

//     // Reset gate related variables for the new stage
//     gateSpawned         = false;
//     gateLifetimeCounter = 0;
//     gateEntryY          = -1;
//     gateEntryX          = -1;
//     gateExitY           = -1;
//     gateExitX           = -1;

//     // Reset item frame for new stage
//     itemFrame = ITEM_LIFESPAN;

//     // Generate map with edge walls and inner walls based on stage
//     for (int i = 0; i < HEIGHT; ++i) {
//         for (int j = 0; j < WIDTH; ++j) {
//             // Corners are immune walls
//             if ((i == 0 || i == HEIGHT - 1) && (j == 0 || j == WIDTH - 1)) {
//                 map[i][j] = IMMUNE_WALL;
//             }
//             // Edges (excluding corners) are normal walls
//             else if (i == 0 || i == HEIGHT - 1 || j == 0 || j == WIDTH - 1) {
//                 map[i][j] = 1;
//             }
//             // Interior: empty for now
//             else {
//                 map[i][j] = 0;
//             }
//         }
//     }

//     // Place inner walls at stage-specific probability
//     double prob = innerWallProbability[stage]; // percentage chance for a wall
//     for (int i = 1; i < HEIGHT - 1; ++i) {
//         for (int j = 1; j < WIDTH - 1; ++j) {
//             if (map[i][j] == 0) {
//                 if ((rand() / (double)RAND_MAX) * 100.0 < prob) {
//                     map[i][j] = 1;
//                 }
//             }
//         }
//     }

//     // Ensure center area is clear for snake placement
//     headY                 = HEIGHT / 2;
//     headX                 = WIDTH / 2;
//     map[headY][headX]     = 0;
//     map[headY][headX - 1] = 0;
//     map[headY][headX - 2] = 0;

//     // Place initial snake of length 3 at center
//     snake.push_front({headY, headX});
//     snake.push_front({headY, headX - 1});
//     snake.push_front({headY, headX - 2});
//     length = 3; // Initial snake length fixed to 3 (head + 2 body segments)
//     for (const auto &segment : snake) {
//         map[segment.first][segment.second] = 3;
//     }
//     // Clear the cell immediately to the right of the head
//     map[headY][headX + 1] = 0;

//     dirIndex     = RIGHT;
//     prevDirIndex = RIGHT;

//     // Spawn initial items and gates for the new stage
//     spawnGrowthItem();
//     spawnPoisonItem();
//     spawnGates();
// }

void initStage(int stage) {
    // Clear any existing snake segments
    snake.clear();
    // Reset turn counter
    stageTurnCounter = 0;
    // Reset gates
    gateSpawned         = false;
    gateLifetimeCounter = 0;
    gateEntryY = gateEntryX = gateExitY = gateExitX = -1;
    // Reset items
    itemFrame = ITEM_LIFESPAN;

    // Build walls
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if ((y == 0 || y == HEIGHT - 1) && (x == 0 || x == WIDTH - 1))
                map[y][x] = IMMUNE_WALL;
            else if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1)
                map[y][x] = 1;
            else
                map[y][x] = 0;
        }
    }
    // Place inner walls
    double prob = innerWallProbability[stage];
    for (int y = 1; y < HEIGHT - 1; ++y) {
        for (int x = 1; x < WIDTH - 1; ++x) {
            if (map[y][x] == 0 && (rand() / (double)RAND_MAX) * 100.0 < prob) {
                map[y][x] = 1;
            }
        }
    }

    // Center start
    headY = HEIGHT / 2;
    headX = WIDTH / 2;
    // Clear center cells
    map[headY][headX]     = 0;
    map[headY][headX - 1] = 0;
    map[headY][headX - 2] = 0;

    // Place snake: head + 2 body segments
    snake.push_front({headY, headX});     // head
    snake.push_front({headY, headX - 1}); // body 1
    snake.push_front({headY, headX - 2}); // body 2
    length = 3;                           // enforce length = 3
    for (auto &seg : snake) {
        map[seg.first][seg.second] = 3;
    }

    // Initialize direction
    dirIndex     = RIGHT;
    prevDirIndex = RIGHT;

    // Spawn first items/gates
    spawnGrowthItem();
    spawnPoisonItem();
    spawnGates();
}

void initColors() {
    start_color();
    // Pair 1: Snake Body (Green on Black)
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    // Pair 2: Poison Item / Game Over Message (Red on Black)
    init_pair(2, COLOR_RED, COLOR_BLACK);
    // Pair 3: Snake Head / Growth Item (Yellow on Black)
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    // Pair 4: Gate (Blue on Black)
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    // Pair 5: Wall (White on Black)
    init_pair(5, COLOR_WHITE, COLOR_BLACK);
    // Pair 6: Immune Wall (Cyan on Black)
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    // Pair 7: Highlight / Selected Menu Item (Black on White or other contrast)
    init_pair(7, COLOR_BLACK, COLOR_WHITE); // Example for selected menu
    // Pair 8: General Text (White on Black - default, but can be explicit)
    init_pair(8, COLOR_WHITE, COLOR_BLACK);
}

void inputPlayerName() {
    nodelay(stdscr, FALSE); // Wait for input
    echo();                 // Show typed characters
    curs_set(1);            // Show cursor
    clear();
    box(stdscr, 0, 0); // Draw a box around the window

    // --- Calculate the common left-alignment point ---
    const char *welcome_message     = "Welcome to 🐍 Snake Game! 🐍";
    int         welcome_message_len = (int)strlen(welcome_message);
    // This 'alignment_x' will be the starting X-coordinate for both messages
    // to make them left-aligned relative to the centered welcome message.
    int alignment_x = (COLS - welcome_message_len) / 2;

    // Print the welcome message using the calculated alignment_x
    mvprintw(LINES / 2 - 2, alignment_x, "%s", welcome_message);

    // --- Print the "Enter your name:" prompt, left-aligned with the welcome message ---
    const char *prompt_text   = "Enter your name: ";
    int         prompt_length = (int)strlen(prompt_text);
    int prompt_y = LINES / 2; // This places it 2 rows below the welcome message (LINES/2 - 2 + 2)
    // Use the same 'alignment_x' for the prompt's starting position
    mvprintw(prompt_y, alignment_x, "%s", prompt_text);

    // --- Move the cursor for input directly after the prompt, on the same line ---
    // The Y-coordinate is the same as the prompt: prompt_y
    // The X-coordinate is: alignment_x (start of prompt) + prompt_length (end of prompt) + 1 (for
    // one space)
    int cursor_start_x = alignment_x + prompt_length + 1; // +1 for a single space after the colon
    int cursor_y       = prompt_y;                        // Same line as the prompt

    move(cursor_y, cursor_start_x); // Place cursor after the prompt and one space
    refresh();                      // Refresh to show both messages and the cursor

    char name_buffer[31];     // Buffer for C-style string input (max 30 chars + null terminator)
    getnstr(name_buffer, 30); // Read up to 30 chars

    // Convert to wstring (assuming playerName is std::wstring as in your code)
    playerName = L""; // Clear previous name
    for (int i = 0; name_buffer[i] != '\0'; ++i) {
        playerName += (wchar_t)name_buffer[i];
    }

    noecho();              // Don't show typed characters anymore
    curs_set(0);           // Hide cursor
    nodelay(stdscr, TRUE); // Don't wait for input for game loop
}

void loadHighScore() {
    std::ifstream file("highscore.txt");
    if (file.is_open()) {
        file >> highScore;
        file.close();
    } else {
        highScore = 0; // Default if file doesn't exist
    }
}

void saveHighScore(int current_game_score) {
    int           currentHigh = 0;
    std::ifstream infile("highscore.txt");
    if (infile.is_open()) {
        infile >> currentHigh;
        infile.close();
    }
    if (current_game_score > currentHigh) {
        std::ofstream outfile("highscore.txt");
        if (outfile.is_open()) {
            outfile << current_game_score;
            outfile.close();
        }
        highScore = current_game_score;
    }
}

// Helper to convert wstring to string for file output if needed
std::string wstring_to_string(const std::wstring &ws) {
    std::string s(ws.begin(), ws.end());
    return s;
}

void saveRanking(const std::wstring &name, int score) {
    // 1) Append the new entry
    {
        std::ofstream file("ranking.txt", std::ios::app);
        if (file.is_open()) {
            file << wstring_to_string(name) << " " << score << "\n";
        }
    }

    // 2) Load all entries into memory
    std::vector<PlayerInfo> entries;
    {
        std::ifstream infile("ranking.txt");
        std::string   line;
        while (std::getline(infile, line)) {
            if (line.empty())
                continue;
            size_t pos = line.find_last_of(' ');
            if (pos == std::string::npos)
                continue;
            std::string n = line.substr(0, pos);
            int         s = std::stoi(line.substr(pos + 1));
            entries.push_back({n, s});
        }
    }

    // 3) Sort descending by score
    std::sort(entries.begin(), entries.end(), std::greater<PlayerInfo>());

    // 4) Trim to top 100
    if (entries.size() > 100) {
        entries.resize(100);
    }

    // 5) Rewrite file with only top 100
    {
        std::ofstream outfile("ranking.txt", std::ios::trunc);
        for (const auto &p : entries) {
            outfile << p.name << " " << p.score << "\n";
        }
    }
}

void showRankingScreen() {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x); // Get screen dimensions

    const char *title = "👑 TOP RANKING 👑";
    mvprintw(8, (max_x - (int)strlen(title)) / 2, "%s", title);

    std::vector<PlayerInfo> ranking;
    std::ifstream           rankFile("ranking.txt");

    if (rankFile.is_open()) {
        std::string line;
        while (std::getline(rankFile, line)) {
            if (line.empty())
                continue;
            // Find last space to separate name and score
            size_t pos = line.find_last_of(' ');
            if (pos == std::string::npos)
                continue;
            std::string name_str = line.substr(0, pos);
            int         score    = std::stoi(line.substr(pos + 1));
            ranking.push_back({name_str, score});
        }
        rankFile.close();
    } else {
        const char *error_msg = "Unable to read the ranking file.";
        mvprintw(max_y / 2, (max_x - (int)strlen(error_msg)) / 2, "%s", error_msg);
        const char *return_msg = "(Press Space to return)";
        mvprintw(max_y / 2 + 1, (max_x - (int)strlen(return_msg)) / 2, "%s", return_msg);
        refresh();
        while (getch() != ' ') {
        }
        return;
    }

    // Sort ranking by score in descending order
    std::sort(ranking.begin(), ranking.end(), std::greater<PlayerInfo>());

    int start_y = 10; // Starting Y position for the ranks

    // Compute max entry length among entries to be shown (up to 20)
    int maxEntryLen  = 0;
    int displayCount = std::min((int)ranking.size(), 20);
    for (int i = 0; i < displayCount; ++i) {
        std::string rank_entry_str = std::to_string(i + 1) + ". " + ranking[i].name + ": " +
                                     std::to_string(ranking[i].score);
        if ((int)rank_entry_str.length() > maxEntryLen) {
            maxEntryLen = rank_entry_str.length();
        }
    }
    int start_x = (max_x - maxEntryLen) / 2;

    for (int i = 0; i < displayCount; ++i) {
        std::string rank_entry_str = std::to_string(i + 1) + ". " + ranking[i].name + ": " +
                                     std::to_string(ranking[i].score);
        mvprintw(start_y + i, start_x, "%s", rank_entry_str.c_str());
    }

    if (ranking.empty()) {
        const char *no_ranks_msg = "No ranking yet.";
        int         noRanksLen   = strlen("No ranking yet.");
        int         noRanksX     = (max_x - noRanksLen) / 2;
        mvprintw(start_y, noRanksX, "%s", no_ranks_msg);
    }

    const char *return_prompt = "Press [spacebar] to return to the menu.";
    mvprintw(max_y - 16, (max_x - (int)strlen(return_prompt)) / 2, "%s", return_prompt);
    refresh();

    while (getch() != ' ') {
    } // Wait for spacebar press
}

int showRulesScreen() {
    int         choice            = 0; // 0: previous, 1: game start
    const char *option_prev_text  = "< Previous";
    const char *option_start_text = "Game Start >";
    int         max_y, max_x;

    while (true) {
        getmaxyx(stdscr, max_y, max_x); // Get current screen dimensions each iteration
        clear();

        // --- Title ---
        const char *title = "< Game Rules >";
        mvprintw(1, (max_x - strlen(title)) / 2, "%s", title);

        // --- Rules Text (Shortened) ---
        int current_y   = 3; // Starting Y for rules
        int main_indent = 2;
        int sub_indent  = 5;

        // Rule Group 1: Snake Movement
        mvprintw(current_y++, main_indent, "🐍 Snake Movement:");
        mvprintw(current_y++, sub_indent, "-> Use arrow keys to move.");
        mvprintw(current_y++, sub_indent, "   Forbidden: U-turns, hitting walls or your own body.");
        mvprintw(current_y++, sub_indent, "   Snake moves automatically each tick.");
        mvprintw(current_y++, sub_indent,
                 "-> Press 'P' during gameplay to pause; press again to resume.");
        mvprintw(current_y++, sub_indent,
                 "-> Press 'Q' anytime during gameplay to quit immediately.");

        current_y++; // Space

        // Rule Group 2: Items
        mvprintw(current_y++, main_indent, "✨ Items:");
        mvprintw(current_y++, sub_indent, "-> 🍎 Growth Item: +1 Length.");
        mvprintw(current_y++, sub_indent, "-> ☠️  Poison Item: -1 Length.");
        mvprintw(current_y++, sub_indent, "   Length below 3 = Game Over.");
        mvprintw(current_y++, sub_indent,
                 "   Items vanish after %d ticks; max %d Growth, %d Poison.", ITEM_LIFESPAN,
                 maxGrowthItems, maxPoisonItems);

        current_y++; // Space

        // Rule Group 3: Gates
        mvprintw(current_y++, main_indent, "🚪 Gates:");
        mvprintw(current_y++, sub_indent, "-> Pairs appear on walls (not corners).");
        mvprintw(current_y++, sub_indent, "   Enter one to teleport to the other.");
        mvprintw(current_y++, sub_indent, "   Gates regenerate after %d ticks.",
                 GATE_LIFESPAN_TICKS);
        mvprintw(current_y++, sub_indent,
                 "   Cooldown: %d ticks. Using gate during cooldown = Game Over.",
                 GATE_COOLDOWN_TICKS);

        current_y++; // Space

        // Rule Group 4: Gate Exit Logic (Simplified)
        mvprintw(current_y++, main_indent, "🔄 Gate Exit:");
        mvprintw(current_y++, sub_indent, "-> Edge Gates: Exit into map interior.");
        mvprintw(current_y++, sub_indent, "-> Inner Gates: Prioritize continuing direction.");

        current_y++; // Space

        // Rule Group 5: Game Over Conditions
        mvprintw(current_y++, main_indent, "💀 Game Over:");
        mvprintw(current_y++, sub_indent, "-> Wall/Body collision, U-turn.");
        mvprintw(current_y++, sub_indent, "-> Length < 3 (from poison).");
        mvprintw(current_y++, sub_indent, "-> Gate cooldown violation.");
        mvprintw(current_y++, sub_indent, "-> Stage turn limit exceeded (if applicable).");

        current_y++; // Space

        // Rule Group 6: Mission & Stages
        mvprintw(current_y++, main_indent, "🎯 Missions & Stages:");
        mvprintw(current_y++, sub_indent, "-> Clear stage goals (length, items, gates).");
        mvprintw(current_y++, sub_indent, "   Complete missions to advance. %d unique stages.",
                 STAGES);

        // --- Navigation Buttons ---
        int button_y = max_y - 12;
        if (current_y + 2 > button_y) {
            button_y = current_y + 1;
        }
        if (button_y >= max_y - 1)
            button_y = max_y - 2;

        int prev_len       = strlen(option_prev_text);
        int start_len      = strlen(option_start_text);
        int button_spacing = 5;

        int total_button_block_width = prev_len + button_spacing + start_len;
        int option1_x                = (max_x - total_button_block_width) / 2;
        int option2_x                = option1_x + prev_len + button_spacing;

        if (choice == 0)
            attron(A_REVERSE);
        mvprintw(button_y, option1_x, "%s", option_prev_text);
        if (choice == 0)
            attroff(A_REVERSE);

        if (choice == 1)
            attron(A_REVERSE);
        mvprintw(button_y, option2_x, "%s", option_start_text);
        if (choice == 1)
            attroff(A_REVERSE);

        mvprintw(max_y - 8, (max_x - strlen("Use Left/Right arrows and Enter to navigate.")) / 2,
                 "Use Left/Right arrows and Enter to navigate.");

        refresh();

        int ch = getch();
        switch (ch) {
        case KEY_LEFT:
        case KEY_UP:
            choice = 0;
            break;
        case KEY_RIGHT:
        case KEY_DOWN:
            choice = 1;
            break;
        case '\n':
            return choice;
        }
    }
}

int showMenuScreen() {
    int current_height, current_width;

    // Terminal size check loop
    while (true) {
        getmaxyx(stdscr, current_height, current_width); // Use getmaxyx

        // Adjusted minimum size requirements (example)
        // The game board itself is WIDTH*3 wide, plus scoreboard/mission board.
        // Let's say scoreboard needs 30 chars.
        int required_width  = WIDTH * 3 + 40; // Game map (21*3=63) + buffer for scoreboard/mission
        int required_height = HEIGHT + 5;     // Game map (21) + buffer for messages/scoreboard

        if (current_height >= required_height && current_width >= required_width) {
            break; // Size is adequate
        }

        clear();
        const char *warning_msg1 = "Terminal is too small.";
        const char *warning_msg2 = "Please enlarge the window! Recommended: width %d, height %d";

        mvprintw(current_height / 2 - 1, (current_width - (int)strlen(warning_msg1)) / 2, "%s",
                 warning_msg1);
        mvprintw(current_height / 2, (current_width - (int)strlen(warning_msg2) - 6) / 2,
                 warning_msg2, required_width, required_height);
        refresh();
        napms(100); // Short delay before re-checking
        if (getch() == 'q')
            return 0; // Allow quitting if stuck here
    }

    int         selected     = 0; // 0: Game Start, 1: Game Rules, 2: Ranking, 3: Exit
    const char *menu_items[] = {"🚀 Game Start", "📜 Game Rules", "👑 Ranking", "🚪 Exit"};
    int         num_items    = sizeof(menu_items) / sizeof(menu_items[0]);
    int         max_y, max_x;

    while (true) {
        getmaxyx(stdscr, max_y, max_x); // Get current screen dimensions
        clear();

        // Game Title - Centered
        const char *game_title = "🐍 S N A K E   G A M E 🐍";
        mvprintw(max_y / 2 - 6, (max_x - (int)strlen(game_title)) / 2, "%s", game_title);
        mvprintw(max_y / 2 - 5, (max_x - (int)strlen(game_title)) / 2,
                 "--------------------------");

        // Menu Items - Centered
        for (int i = 0; i < num_items; ++i) {
            int y_pos = max_y / 2 - 2 + i * 2; // Spacing out menu items
            int x_pos = (max_x - (int)strlen(menu_items[i])) / 2;
            if (i == selected) {
                attron(A_REVERSE); // Highlight selected item
            }
            mvprintw(y_pos, x_pos, "%s", menu_items[i]);
            if (i == selected) {
                attroff(A_REVERSE);
            }
        }

        // Footer hint
        const char *hint = "Use UP/DOWN arrows and Enter to select.";
        mvprintw(max_y - 8, (max_x - (int)strlen(hint)) / 2, "%s", hint);

        refresh();

        int ch = getch();
        switch (ch) {
        case KEY_UP:
            selected = (selected - 1 + num_items) % num_items;
            break;
        case KEY_DOWN:
            selected = (selected + 1) % num_items;
            break;
        case '\n':                      // Enter key
            if (selected == 0) {        // Game Start
                return 1;               // Signal to start the game
            } else if (selected == 1) { // Game Rules
                int rule_choice = showRulesScreen();
                if (rule_choice == 1)
                    return 1; // Start game from rules screen
                // if 0, it will loop back to menu screen
            } else if (selected == 2) { // Ranking
                showRankingScreen();
                // After ranking, it will loop back to menu screen
            } else if (selected == 3) { // Exit
                return 0;               // Signal to exit the program
            }
            break;
        case 'q': // Allow 'q' to quit from menu as well
            return 0;
        }
    }
}

void drawScoreboard() {
    int board_x_start = WIDTH * 3 + 5;
    int current_y     = 1;

    mvprintw(current_y++, board_x_start, "----- SCOREBOARD -----");
    mvprintw(current_y++, board_x_start, "🍎 Growth Items: %d pts", total_score_growth);
    mvprintw(current_y++, board_x_start, "☠️  Poison Items: %d pts", total_score_poison);
    mvprintw(current_y++, board_x_start, "🚪 Gates Used  : %d pts", total_score_gate);

    int currentTotalScore = total_score_growth + total_score_poison + total_score_gate;
    mvprintw(current_y++, board_x_start, "----------------------");
    mvprintw(current_y++, board_x_start, "🏆 Current Score: %d", currentTotalScore);
    mvprintw(current_y++, board_x_start, "⭐ High Score   : %d", highScore);
    mvprintw(current_y++, board_x_start, "----------------------");

    if (itemFrame > 0) {
        mvprintw(current_y++, board_x_start, "⏳ Items Despawn: %d ticks", itemFrame);
    } else {
        mvprintw(current_y++, board_x_start, "⏳ Items Despawn: N/A");
    }
    if (gateA.first != -1) {
        mvprintw(current_y++, board_x_start, "⏳ Gates Despawn: %d ticks",
                 gateLifespan - gateLifetimeCounter);
    } else {
        mvprintw(current_y++, board_x_start, "⏳ Gates Despawn: N/A");
    }
}

void drawMissionBoard() {
    int board_x_start = WIDTH * 3 + 5;
    int current_y     = 1; // Start Y position
    // Calculate current_y based on drawScoreboard's height more accurately
    // Title + 3 scores + separator + 2 totals + separator + 2 lifespans + spacing
    current_y = 1 + 3 + 1 + 2 + 1 + 2 + 2;

    mvprintw(current_y++, board_x_start, "-------- MISSION (Stage %d) --------", currentStage + 1);
    std::string mission_status_char_str; // Use string for status

    // Length Mission
    int req_len = mission_length_per_stage[currentStage];
    mission_status_char_str =
        (snake.size() >= req_len) ? "✅" : "  "; // Use string with spaces for alignment
    mvprintw(current_y++, board_x_start, "🐍 Length: %ld/%d (%s) (Max: %d)", snake.size(), req_len,
             mission_status_char_str.c_str(), maxLengthAchieved);

    // Growth Item Mission
    int req_growth          = mission_growth_per_stage[currentStage];
    mission_status_char_str = (collected_growth_items >= req_growth) ? "✅" : "  ";
    mvprintw(current_y++, board_x_start, "🍎 Growth: %d/%d (%s)", collected_growth_items,
             req_growth, mission_status_char_str.c_str());

    // Poison Item Mission
    int req_poison          = mission_poison_per_stage[currentStage];
    mission_status_char_str = (collected_poison_items >= req_poison) ? "✅" : "  ";
    mvprintw(current_y++, board_x_start, "☠️  Poison: %d/%d (%s)", collected_poison_items,
             req_poison, mission_status_char_str.c_str());

    // Gate Usage Mission
    int req_gate            = mission_gate_per_stage[currentStage];
    mission_status_char_str = (gates_used_count >= req_gate) ? "✅" : "  ";
    mvprintw(current_y++, board_x_start, "🚪 Gates : %d/%d (%s)", gates_used_count, req_gate,
             mission_status_char_str.c_str());
    mvprintw(current_y++, board_x_start, "----------------------------------");

    int turns_remaining = stageTurnLimitPerStage[currentStage % STAGES] - stageTurnCounter;
    mvprintw(current_y++, board_x_start, "⏱️  Turns Left: %d", turns_remaining);

    if (checkMissionClear()) {
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(current_y++, board_x_start, "🎉 MISSION COMPLETE! 🎉");
        attroff(COLOR_PAIR(1) | A_BOLD);
    } else {
        mvprintw(current_y++, board_x_start, "   (Keep Going!)");
    }
    mvprintw(current_y++, board_x_start, "----------------------------------");
}

// This function checks if all mission objectives for the current stage are met.
bool checkMissionClear() {
    int  stageIdx    = currentStage;
    bool lengthClear = ((int)snake.size() >= mission_length_per_stage[stageIdx]);
    bool growthClear = (collected_growth_items >= mission_growth_per_stage[stageIdx]);
    bool poisonClear = (collected_poison_items >= mission_poison_per_stage[stageIdx]);
    bool gateClear   = (gates_used_count >= mission_gate_per_stage[stageIdx]);

    missionStatusLength[stageIdx] = lengthClear;
    missionStatusGrowth[stageIdx] = growthClear;
    missionStatusPoison[stageIdx] = poisonClear;
    missionStatusGate[stageIdx]   = gateClear;

    return lengthClear && growthClear && poisonClear && gateClear;
}

void showGameOverScreen(int finalScore) {
    clear();
    std::string title =
        gameWon ? "🎉 CONGRATULATIONS! ALL STAGES CLEARED! 🎉" : " G A M E   O V E R ";
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(LINES / 2 - 5, (COLS - title.length()) / 2, "%s", title.c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);

    std::string reasonMessage = "";
    if (!gameWon) {
        switch (gameOverReason) {
        case 1:
            reasonMessage = "Reason: U-turn attempted (꼬리 방향 이동).";
            break;
        case 2:
            reasonMessage = "Reason: Collided with a wall (벽 충돌).";
            break;
        case 3:
            reasonMessage = "Reason: Collided with self (몸통 충돌).";
            break;
        case 4:
            reasonMessage = "Reason: Stage turn limit exceeded (제한 턴 초과).";
            break;
        case 5:
            reasonMessage = "Reason: Snake length too short (<3) (길이 부족).";
            break;
        case 6:
            reasonMessage = "Reason: Entered gate during cooldown (게이트 쿨다운 위반).";
            break;
        case 7:
            reasonMessage = "Reason: Pressed 'Q' to quit ('Q'를 눌러 종료).";
            break;
        default:
            reasonMessage = "Reason: Unknown mishap on the snake trail!";
            break;
        }
        mvprintw(LINES / 2 - 3, (COLS - (int)reasonMessage.length()) / 2, "%s",
                 reasonMessage.c_str());
    }

    std::string score_str =
        "Final Score for " + wstring_to_string(playerName) + ": " + std::to_string(finalScore);
    mvprintw(LINES / 2 - 1, (COLS - (int)score_str.length()) / 2, "%s", score_str.c_str());

    std::string stats_title = "------ Final Stats ------";
    mvprintw(LINES / 2 + 1, (COLS - (int)stats_title.length()) / 2, "%s", stats_title.c_str());

    std::string length_stat = "Max Length Achieved: " + std::to_string(maxLengthAchieved);
    mvprintw(LINES / 2 + 2, (COLS - (int)length_stat.length()) / 2, "%s", length_stat.c_str());

    std::string growth_stat = "Growth Items Collected: " + std::to_string(collected_growth_items);
    mvprintw(LINES / 2 + 3, (COLS - (int)growth_stat.length()) / 2, "%s", growth_stat.c_str());

    std::string poison_stat = "Poison Items Touched: " + std::to_string(collected_poison_items);
    mvprintw(LINES / 2 + 4, (COLS - (int)poison_stat.length()) / 2, "%s", poison_stat.c_str());

    std::string gate_stat = "Gates Used: " + std::to_string(gates_used_count);
    mvprintw(LINES / 2 + 5, (COLS - (int)gate_stat.length()) / 2, "%s", gate_stat.c_str());

    // Ranking display logic
    // Only after stats, before prompt
    // Load ranking.txt, parse, sort, and find player position
    int                     playerRank = 0;
    std::vector<PlayerInfo> rankingList;
    std::ifstream           rankFile("ranking.txt");
    if (rankFile.is_open()) {
        std::string line;
        while (std::getline(rankFile, line)) {
            if (line.empty())
                continue;
            size_t pos = line.find_last_of(' ');
            if (pos == std::string::npos)
                continue;
            std::string name_str = line.substr(0, pos);
            int         score    = std::stoi(line.substr(pos + 1));
            rankingList.push_back({name_str, score});
        }
        rankFile.close();
    }
    std::sort(rankingList.begin(), rankingList.end(), std::greater<PlayerInfo>());
    for (int i = 0; i < (int)rankingList.size(); ++i) {
        if (rankingList[i].name == wstring_to_string(playerName)) {
            playerRank = i + 1;
            break;
        }
    }
    std::string rankLine = "Your Ranking: " + std::to_string(playerRank) + " / 100";
    mvprintw(LINES / 2 + 7, (COLS - (int)rankLine.length()) / 2, "%s", rankLine.c_str());

    const char *return_prompt = "Press [spacebar] to return to the menu.";
    mvprintw(LINES - 8, (COLS - (int)strlen(return_prompt)) / 2, "%s", return_prompt);
    refresh();

    // Wait for spacebar before returning to menu
    while (getch() != ' ') {
    }
}

void showVictoryScreen(int finalScore) {
    clear();
    std::string title = "🏆 YOU HAVE CLEARED ALL STAGES! 🏆";
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(LINES / 2 - 5, (COLS - (int)title.length()) / 2, "%s", title.c_str());
    attroff(COLOR_PAIR(1) | A_BOLD);

    std::string message = "Thank you for playing Snake Game!";
    mvprintw(LINES / 2 - 3, (COLS - (int)message.length()) / 2, "%s", message.c_str());

    std::string score_str =
        "Final Score for " + wstring_to_string(playerName) + ": " + std::to_string(finalScore);
    mvprintw(LINES / 2 - 1, (COLS - (int)score_str.length()) / 2, "%s", score_str.c_str());

    std::string prompt = "Press [spacebar] to return to the main menu.";
    mvprintw(LINES / 2 + 2, (COLS - (int)prompt.length()) / 2, "%s", prompt.c_str());

    refresh();
    while (getch() != ' ') {
    }
}

void drawMap() {
    erase();

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            move(y, x * 3);
            int cell_type = map[y][x];

            switch (cell_type) {
            case 0:
                addstr("   ");
                break;
            case 1:
                attron(COLOR_PAIR(5));
                addstr("███");
                attroff(COLOR_PAIR(5));
                break;
            case IMMUNE_WALL:
                attron(COLOR_PAIR(6));
                addstr("▣▣▣");
                attroff(COLOR_PAIR(6));
                break;
            case 2: // Poison Item
                attron(COLOR_PAIR(2));
                addstr("☠️  ");
                attroff(COLOR_PAIR(2)); // Ensure space for multi-byte char + space
                break;
            case 3: // Snake Body part
                if (!snake.empty() && y == snake.front().first &&
                    x == snake.front().second) { // Head
                    attron(COLOR_PAIR(3));
                    addstr("🟨 ");
                    attroff(COLOR_PAIR(3));
                } else { // Body
                    attron(COLOR_PAIR(1));
                    addstr("🟩 ");
                    attroff(COLOR_PAIR(1));
                }
                break;
            case 4: // Growth Item
                attron(COLOR_PAIR(3));
                addstr("🍎 ");
                attroff(COLOR_PAIR(3));
                break;
            case 5: // Gate
                attron(COLOR_PAIR(4));
                addstr(" 🚪 ");
                attroff(COLOR_PAIR(4));
                break;
            default:
                addstr(" ? ");
                break;
            }
        }
    }

    drawScoreboard();
    drawMissionBoard();

    int term_height, term_width;
    getmaxyx(stdscr, term_height, term_width);
    int required_width  = WIDTH * 3 + 40;
    int required_height = HEIGHT + 5;

    if (term_width < required_width || term_height < required_height) {
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(LINES - 1, 0, "WARNING: Terminal too small! UI may be broken. Resize to %dx%d.",
                 required_width, required_height);
        attroff(COLOR_PAIR(2) | A_BOLD);
    }

    refresh();
}

void playGame() {
    // --- Comprehensive Game State Reset for a NEW GAME ---
    gameOver       = false;
    gameOverReason = 0; // Crucial: Reset any previous game over reason
    gameWon        = false;
    currentStage   = 0; // Start from the first stage (0-indexed)

    // Reset scores for the new game session (already done in main(), but harmless here)
    total_score_growth = 0;
    total_score_poison = 0;
    total_score_gate   = 0;
    maxLengthAchieved  = 3; // Snake starts at length 3
    length             = 3; // Ensure global length variable is reset to 3

    // Reset current stage mission progress
    collected_growth_items = 0;
    collected_poison_items = 0;
    gates_used_count       = 0;

    // Reset mission status flags for display (all false initially)
    for (int i = 0; i < STAGES; ++i) {
        missionStatusLength[i] = false;
        missionStatusGrowth[i] = false;
        missionStatusPoison[i] = false;
        missionStatusGate[i]   = false;
    }

    length = 3; // Reset snake length to 3 for new game
    // Initialize the first stage (this will handle snake placement, map, etc.)
    initStage(currentStage);

    // Initial game speed (from the first stage's delay)
    int DELAY = delay_per_stage[currentStage];

    bool isPaused = false; // Pause state variable

    // Ncurses setup for game input
    keypad(stdscr, TRUE);  // Enable arrow keys
    nodelay(stdscr, TRUE); // Make getch() non-blocking
    curs_set(0);           // Hide cursor
    timeout(DELAY / 1000); // Set initial input timeout

    // Main game loop
    while (!gameOver) {
        int ch = getch();

        if (ch == 'q' || ch == 'Q') {
            // Display quitting message
            const char *quit_msg = "Quitting game... Press any key to exit.";
            mvprintw(HEIGHT / 2, (WIDTH * 3 + 5 - (int)strlen(quit_msg)) / 2, "%s", quit_msg);
            refresh();
            // Wait for any key
            nodelay(stdscr, FALSE);
            getch();
            // Immediately end game loop and exit
            gameOver = true;
            return;
        }

        if (ch == 'p' || ch == 'P') {
            isPaused = !isPaused;
            if (isPaused) {
                timeout(-1); // Blocking input when paused
            } else {
                timeout(DELAY / 1000); // Non-blocking with game delay when unpaused
            }
        }

        if (!isPaused) {         // --- Only update game logic if NOT paused ---
            updateDirection(ch); // Update snake direction based on input

            moveSnake(); // Update snake position and handle collisions/items

            // After moveSnake(), check for game over conditions
            if (gameOverReason != 0 || snake.size() < 3) {
                gameOver = true;
            }

            // --- Insert stage advancement logic here ---
            if (!gameOver && checkMissionClear()) {
                // Advance to next stage immediately
                currentStage++;
                if (currentStage >= STAGES) {
                    gameOver = true;
                    gameWon  = true;
                    break;
                }
                // Reset stage progress before initializing
                collected_growth_items = 0;
                collected_poison_items = 0;
                gates_used_count       = 0;
                initStage(currentStage);
                DELAY = delay_per_stage[currentStage];
                timeout(DELAY / 1000);
                {
                    clear();
                    int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);
                    std::string msg = currentStage < STAGES
                                          ? "🎉 STAGE " + std::to_string(currentStage) +
                                                " CLEARED! NEXT STAGE! 🎉"
                                          : "🎉 CONGRATULATIONS! ALL STAGES CLEARED! 🎉";
                    mvprintw(max_y / 2, (max_x - (int)msg.length()) / 2, "%s", msg.c_str());
                    refresh();
                    usleep(3000000); // Pause for 3 seconds before next stage
                }
                continue; // Skip rendering this frame to start next stage cleanly
            }
        }

        // Only proceed if game is not over
        if (!gameOver) {
            drawMap();          // Draw the game map
            drawScoreboard();   // Draw player scores
            drawMissionBoard(); // Draw mission objectives

            // --- Display PAUSED message if applicable ---
            if (isPaused) {
                const char *pause_msg = "PAUSED - Press 'P' to resume";
                mvprintw(HEIGHT / 2, (WIDTH / 2) - (strlen(pause_msg) / 2), "%s", pause_msg);
            }

            // (Stage advancement logic moved above, after moveSnake)
            refresh(); // Update the physical screen
        }
    }

    // Game loop exited (game over or game won)
    nodelay(stdscr, FALSE); // Make getch() blocking again for game over screen
    curs_set(1);            // Show cursor

    int finalScore =
        (int)snake.size() * 100 + total_score_growth - total_score_poison + total_score_gate;
    saveHighScore(finalScore);
    saveRanking(playerName, finalScore);

    if (gameWon)
        showVictoryScreen(finalScore);
    else
        showGameOverScreen(finalScore);
}

int main() {
    setlocale(LC_ALL, ""); // For Unicode characters
    srand(time(0));        // Seed random number generator

    initscr();            // Initialize ncurses
    cbreak();             // Disable line buffering
    noecho();             // Don't echo input characters
    keypad(stdscr, TRUE); // Enable function keys (arrows, F1, etc.)
    curs_set(0);          // Hide the cursor
    timeout(100);         // Set a default timeout for getch() (e.g., 100ms)

    if (has_colors() == FALSE) {
        endwin();
        printf("Your terminal does not support color\n");
        return 1;
    }
    initColors(); // Initialize color pairs

    loadHighScore(); // Load high score from file

    while (true) {
        int menu_choice = showMenuScreen();

        if (menu_choice == 1) { // "Game Start" was chosen
            if (playerName.empty()) {
                inputPlayerName();
                if (playerName.empty())
                    playerName = L"Player"; // Default if empty
            }
            // Reset game-wide scores before starting a new game session
            total_score_growth     = 0;
            total_score_poison     = 0;
            total_score_gate       = 0;
            maxLengthAchieved      = 3; // Initial length
            currentStage           = 0;
            collected_growth_items = 0;
            collected_poison_items = 0;
            gates_used_count       = 0;

            playGame();                // Start game loop
        } else if (menu_choice == 0) { // "Exit" was chosen
            break;                     // Exit the main menu loop and terminate
        }
        // If "Game Rules" or "Ranking" was chosen, showMenuScreen handles them and returns to menu
    }

    endwin(); // De-initialize ncurses
    return 0;
}