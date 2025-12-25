

/*
 * This example creates an SDL window and renderer, and then draws some lines,
 * rectangles and points to it every frame.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define FPS 20.0
#define MAX_WIDTH 800
#define MAX_HEIGHT 800
#define GRID_SIZE_X 40
#define GRID_SIZE_Y 40
#define GRID_GAP 1
#define NUM_CELL_NEIGHBORS 8

#define CELL_WIDTH (((MAX_WIDTH) / (float)(GRID_SIZE_X)) - (GRID_GAP))
#define CELL_HEIGHT (((MAX_WIDTH) / (float)(GRID_SIZE_Y)) - (GRID_GAP))

#define WITH_RENDER_COLOR(renderer, color)                                     \
  for (Uint8 or, og, ob, oa,                                                   \
       flag = 0;                                                               \
       SDL_GetRenderDrawColor((renderer), &or, &og, &ob, &oa),                 \
       SDL_SetRenderDrawColor((renderer), (color).r, (color).g, (color).b,     \
                              (color).a),                                      \
       flag != 1;                                                              \
       SDL_SetRenderDrawColor((renderer), or, og, ob, oa), flag = 1)

typedef struct {
  int r, g, b, a;
} Color;

typedef struct Cell {
  bool isAlive;
  SDL_FRect *frect;                           // Associated rendered cell.
  struct Cell *neighbors[NUM_CELL_NEIGHBORS]; // The cells four neighbors
  Color color;                                // Alive cell color
  int x, y;                                   // Position
} Cell;

typedef struct {
  SDL_FRect cellDrawList[GRID_SIZE_X * GRID_SIZE_Y];
  size_t cellCount;
  Cell cellMap[GRID_SIZE_X][GRID_SIZE_Y];

  Cell *dragStartCell; // The starting cell of a drag event.
} MapSystem;

typedef struct {
  bool isAFixedUpdate; // Is true when a fixed-time update should trigger
  uint64_t timestamp;
  double fps;

  bool isPlaying;      // User selected with P
  bool shouldRunFrame; // User selected with .
} SimulationSystem;

static SDL_Window *g_window = nullptr;
static SDL_Renderer *g_renderer = nullptr;
static MapSystem g_map = {0};
static SimulationSystem g_sim = {0};

// Palette
static const Color deadCellColor = {
    .r = 56, .g = 59, .b = 64, .a = SDL_ALPHA_OPAQUE};
static const Color aliveCellColor = {
    .r = 195, .g = 199, .b = 205, .a = SDL_ALPHA_OPAQUE};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  SDL_SetAppMetadata("Conway's Game of Life", "1.0",
                     "com.risheit.game-of-life");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("Game of Life", MAX_WIDTH, MAX_HEIGHT, 0,
                                   &g_window, &g_renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(g_renderer, MAX_WIDTH, MAX_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // Initialize simulation system
  g_sim.fps = FPS;
  g_sim.timestamp = SDL_GetPerformanceCounter();
  g_sim.isPlaying = false;
  g_sim.isAFixedUpdate = false;

  // initialize map system
  g_map.cellCount = GRID_SIZE_X * GRID_SIZE_Y;
  for (int j = 0; j < GRID_SIZE_Y; j++) {
    for (int i = 0; i < GRID_SIZE_X; i++) {
      SDL_FRect *cellFRect = &g_map.cellDrawList[GRID_SIZE_X * j + i];

      // Rendered box sizes are smaller to account for margin and gap.
      cellFRect->x = (GRID_GAP / 2.0) + i * (CELL_WIDTH + GRID_GAP);
      cellFRect->y = (GRID_GAP / 2.0) + j * (CELL_HEIGHT + GRID_GAP);
      cellFRect->w = CELL_WIDTH;
      cellFRect->h = CELL_HEIGHT;

      Cell *cell = &g_map.cellMap[i][j];
      cell->frect = cellFRect;
      cell->isAlive = false;
      cell->color = aliveCellColor;
      cell->x = i;
      cell->y = j;

      // Initialize cell neighbors L -> T -> R -> B, wrap around screen
      int leftNeighborIndex = i - 1 < 0 ? GRID_SIZE_X - 1 : i - 1;
      int rightNeighborIndex = i + 1 >= GRID_SIZE_X ? 0 : i + 1;
      int topNeighborIndex = j - 1 < 0 ? GRID_SIZE_Y - 1 : j - 1;
      int bottomNeighborIndex = j + 1 >= GRID_SIZE_Y ? 0 : j + 1;
      cell->neighbors[0] = &g_map.cellMap[leftNeighborIndex][j];
      cell->neighbors[1] = &g_map.cellMap[leftNeighborIndex][topNeighborIndex];
      cell->neighbors[2] =
          &g_map.cellMap[leftNeighborIndex][bottomNeighborIndex];
      cell->neighbors[3] = &g_map.cellMap[rightNeighborIndex][j];
      cell->neighbors[4] = &g_map.cellMap[rightNeighborIndex][topNeighborIndex];
      cell->neighbors[5] =
          &g_map.cellMap[rightNeighborIndex][bottomNeighborIndex];
      cell->neighbors[6] = &g_map.cellMap[i][topNeighborIndex];
      cell->neighbors[7] = &g_map.cellMap[i][bottomNeighborIndex];
    }
  }
  return SDL_APP_CONTINUE;
}

// Draws the map as squares with a gap in between each
static void drawMap() {
  WITH_RENDER_COLOR(g_renderer, deadCellColor) {
    SDL_RenderFillRects(g_renderer, g_map.cellDrawList, g_map.cellCount);
  }
}

static void drawActiveCells() {
  for (int j = 0; j < GRID_SIZE_Y; j++) {
    for (int i = 0; i < GRID_SIZE_X; i++) {
      Cell *cell = &g_map.cellMap[i][j];
      if (cell->isAlive) {
        WITH_RENDER_COLOR(g_renderer, cell->color) {
          SDL_RenderFillRect(g_renderer, cell->frect);
        }
      }
    }
  }
}

typedef enum {
  CELL_SET_ALIVE,
  CELL_SET_DEAD,
  CELL_TOGGLE,
} CellSetAction;

Cell *getCellUnderPoint(float x, float y) {
  SDL_FPoint point = {.x = x, .y = y};

  for (int j = 0; j < GRID_SIZE_Y; j++) {
    for (int i = 0; i < GRID_SIZE_X; i++) {
      if (SDL_PointInRectFloat(&point, g_map.cellMap[i][j].frect)) {
        return &g_map.cellMap[i][j];
      }
    }
  }

  return nullptr;
}

void setCellUnderPoint(float x, float y, CellSetAction action) {
  Cell *cell = getCellUnderPoint(x, y);
  if (!cell)
    return;

  SDL_Log("Selected cell (%d, %d)", cell->x, cell->y);

  switch (action) {
  case CELL_SET_ALIVE:
    cell->isAlive = true;
    break;
  case CELL_SET_DEAD:
    cell->isAlive = false;
    break;
  case CELL_TOGGLE:
    cell->isAlive = !cell->isAlive;
    break;
  }
}

// Triggers on mouse button down. A regular mouse click is considered a
// drag with no motion.
void handleDragStart(SDL_MouseButtonEvent *button) {
  g_map.dragStartCell = getCellUnderPoint(button->x, button->y);
}

// On drag, set all dragged-over cells to the same state as the starting
// cell of the drag motion. Cells should only be updated once.
void handleDragMotion(SDL_MouseMotionEvent *motion) {
  if (!g_map.dragStartCell)
    return;

  CellSetAction action =
      g_map.dragStartCell->isAlive ? CELL_SET_ALIVE : CELL_SET_DEAD;
  setCellUnderPoint(motion->x, motion->y, action);
}

void handleSimulationReset() {
  // Reset all cells to dead.
  for (int j = 0; j < GRID_SIZE_Y; j++) {
    for (int i = 0; i < GRID_SIZE_X; i++) {
      g_map.cellMap[i][j].isAlive = false;
    }
  }
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    SDL_MouseButtonEvent *button = &event->button;
    if (button->button == SDL_BUTTON_LEFT) {
      g_sim.isPlaying = false;
      setCellUnderPoint(button->x, button->y, CELL_TOGGLE);
      handleDragStart(button);
    }
    break;
  case SDL_EVENT_MOUSE_MOTION:
    SDL_MouseMotionEvent *motion = &event->motion;
    if (motion->state == SDL_BUTTON_LMASK) {
      g_sim.isPlaying = false;
      handleDragMotion(motion);
    }
    break;
  case SDL_EVENT_KEY_DOWN:
    SDL_KeyboardEvent *key = &event->key;
    switch (key->key) {
    case SDLK_P: // P to play/pause
      g_sim.isPlaying = !g_sim.isPlaying;
      break;
    case SDLK_PERIOD: // "." to move forward one frame
      g_sim.shouldRunFrame = true;
      break;
    case SDLK_R: // R to reset
      handleSimulationReset();
      break;
    }
  }

  return SDL_APP_CONTINUE;
}

int getNumLiveNeighbors(Cell *cell) {
  int numLiveNeighbors = 0;
  for (int i = 0; i < NUM_CELL_NEIGHBORS; i++) {
    if (cell->neighbors[i]->isAlive)
      numLiveNeighbors++;
  }

  return numLiveNeighbors;
}

void simulateConwayIteration() {
  // Rules:
  // 1. Any live cell with fewer than two live neighbors dies, as if by
  // underpopulation.
  // 2. Any live cell with two or three live neighbors lives on to the next
  // generation.
  // 3. Any live cell with more than three live neighbors dies, as if by
  // overpopulation.
  // 4. Any dead cell with exactly three live neighbors becomes a live cell, as
  // if by reproduction.

  // TODO: Infinite board
  // Until then, wrap for edge cells.

  // Deaths and births happen after an iteration
  Cell *cellsToBirth[GRID_SIZE_X * GRID_SIZE_Y] = {0};
  int birthCount = 0;
  Cell *cellsToKill[GRID_SIZE_X * GRID_SIZE_Y] = {0};
  int deathCount = 0;

  // Test cells
  for (int j = 0; j < GRID_SIZE_Y; j++) {
    for (int i = 0; i < GRID_SIZE_X; i++) {
      Cell *cell = &g_map.cellMap[i][j];
      int liveNeighbors = getNumLiveNeighbors(cell);

      // Rule 1 and 3
      if (cell->isAlive && (liveNeighbors < 2 || liveNeighbors > 3)) {
        cellsToKill[deathCount] = cell;
        deathCount++;
      }
      // Rule 4
      else if (!cell->isAlive && liveNeighbors == 3) {
        cellsToBirth[birthCount] = cell;
        birthCount++;
      }
    }
  }

  // Update cells
  for (int i = 0; i < birthCount; i++) {
    cellsToBirth[i]->isAlive = true;
  }
  for (int i = 0; i < deathCount; i++) {
    cellsToKill[i]->isAlive = false;
  }
}

void tickSimulationTimer() {
  static double accumulatedSeconds = 0;
  double cycleTime = 1.0 / g_sim.fps;

  uint64_t lastTimestamp = g_sim.timestamp;
  g_sim.timestamp = SDL_GetPerformanceCounter();
  double delta = g_sim.timestamp - lastTimestamp;
  accumulatedSeconds += delta / SDL_GetPerformanceFrequency();

  // Update at a fixed rate according to specified FPS
  if (accumulatedSeconds > cycleTime) {
    accumulatedSeconds -= cycleTime;
    g_sim.isAFixedUpdate = true;
  } else {
    g_sim.isAFixedUpdate = false;
  }
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  SDL_SetRenderDrawColor(g_renderer, 33, 33, 33, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(g_renderer);

  // Move to next update step
  tickSimulationTimer();

  // Simulate next step if the time advanced last iteration
  if ((g_sim.isPlaying || g_sim.shouldRunFrame) && g_sim.isAFixedUpdate) {
    g_sim.shouldRunFrame = false;
    simulateConwayIteration();
  }

  // When the simulation is playing, automatically advance the time.
  if (g_sim.isPlaying) {
    g_sim.timestamp++;
  }

  drawMap();
  drawActiveCells();
  SDL_RenderPresent(g_renderer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}
