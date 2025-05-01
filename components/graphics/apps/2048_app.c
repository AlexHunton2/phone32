#include "apps.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GRID_SIZE 4
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 720
#define BOARD_SIZE 400

static lv_obj_t *tiles[GRID_SIZE][GRID_SIZE];
static int board[GRID_SIZE][GRID_SIZE];

static lv_style_t style_tile;

static lv_point_t touch_start;

static void update_tiles(void);
static void reset_game(void);
static void handle_swipe(lv_dir_t dir);
static void spawn_random_tile(void);

static void gesture_fallback_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_get_act();
  lv_point_t p;

  if (code == LV_EVENT_PRESSED) {
    lv_indev_get_point(indev, &touch_start);
  } else if (code == LV_EVENT_RELEASED) {
    lv_indev_get_point(indev, &p);
    int dx = p.x - touch_start.x;
    int dy = p.y - touch_start.y;

    if (abs(dx) > 15 || abs(dy) > 15) {
      if (abs(dx) > abs(dy)) {
        handle_swipe(dx > 0 ? LV_DIR_RIGHT : LV_DIR_LEFT);
      } else {
        handle_swipe(dy > 0 ? LV_DIR_BOTTOM : LV_DIR_TOP);
      }
    }
  }
}

static void reset_cb(lv_event_t *e) { reset_game(); }

static bool slide_left() {
  bool changed = false;
  for (int r = 0; r < GRID_SIZE; r++) {
    int tmp[GRID_SIZE] = {0};
    int idx = 0;
    for (int c = 0; c < GRID_SIZE; c++) {
      if (board[r][c] != 0) {
        tmp[idx++] = board[r][c];
      }
    }
    for (int c = 0; c < idx - 1; c++) {
      if (tmp[c] != 0 && tmp[c] == tmp[c + 1]) {
        tmp[c] *= 2;
        tmp[c + 1] = 0;
        c++;
      }
    }
    int merged[GRID_SIZE] = {0};
    idx = 0;
    for (int c = 0; c < GRID_SIZE; c++) {
      if (tmp[c] != 0)
        merged[idx++] = tmp[c];
    }
    for (int c = 0; c < GRID_SIZE; c++) {
      if (board[r][c] != merged[c]) {
        board[r][c] = merged[c];
        changed = true;
      }
    }
  }
  return changed;
}

static void rotate_board() {
  uint16_t tmp[GRID_SIZE][GRID_SIZE];
  for (int r = 0; r < GRID_SIZE; r++)
    for (int c = 0; c < GRID_SIZE; c++)
      tmp[c][GRID_SIZE - 1 - r] = board[r][c];
  for (int r = 0; r < GRID_SIZE; r++)
    for (int c = 0; c < GRID_SIZE; c++)
      board[r][c] = tmp[r][c];
}

static void handle_swipe(lv_dir_t dir) {
  bool changed = false;
  switch (dir) {
  case LV_DIR_LEFT:
    changed = slide_left();
    break;
  case LV_DIR_RIGHT:
    rotate_board();
    rotate_board();
    changed = slide_left();
    rotate_board();
    rotate_board();
    break;
  case LV_DIR_TOP:
    rotate_board();
    rotate_board();
    rotate_board();
    changed = slide_left();
    rotate_board();
    break;
  case LV_DIR_BOTTOM:
    rotate_board();
    changed = slide_left();
    rotate_board();
    rotate_board();
    rotate_board();
    break;
  default:
    break;
  }
  if (changed) {
    spawn_random_tile();
    update_tiles();
  }
}

static lv_color_t get_tile_color(int value) {
  switch (value) {
  case 2:
    return lv_palette_lighten(LV_PALETTE_RED, 3);
  case 4:
    return lv_palette_lighten(LV_PALETTE_ORANGE, 3);
  case 8:
    return lv_palette_lighten(LV_PALETTE_YELLOW, 3);
  case 16:
    return lv_palette_lighten(LV_PALETTE_GREEN, 3);
  case 32:
    return lv_palette_lighten(LV_PALETTE_BLUE, 3);
  case 64:
    return lv_palette_lighten(LV_PALETTE_PURPLE, 3);
  case 128:
    return lv_palette_lighten(LV_PALETTE_RED, 2);
  case 256:
    return lv_palette_lighten(LV_PALETTE_ORANGE, 2);
  case 512:
    return lv_palette_lighten(LV_PALETTE_YELLOW, 2);
  case 1024:
    return lv_palette_lighten(LV_PALETTE_GREEN, 2);
  case 2048:
    return lv_palette_lighten(LV_PALETTE_BLUE, 2);
  default:
    return lv_palette_lighten(LV_PALETTE_GREY, 3);
  }
}

static void update_tiles(void) {
  char buf[8];
  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      int value = board[r][c];
      lv_obj_t *tile = tiles[r][c];
      lv_obj_t *label = lv_obj_get_child(tile, 0);
      if (value) {
        snprintf(buf, sizeof(buf), "%d", value);
        lv_label_set_text(label, buf);
      } else {
        lv_label_set_text(label, "");
      }
      lv_obj_set_style_bg_color(tile, get_tile_color(value), 0);
    }
  }
}

static void spawn_random_tile(void) {
  int empty[GRID_SIZE * GRID_SIZE][2];
  int n = 0;
  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      if (board[r][c] == 0) {
        empty[n][0] = r;
        empty[n][1] = c;
        n++;
      }
    }
  }
  if (n > 0) {
    int idx = rand() % n;
    int r = empty[idx][0];
    int c = empty[idx][1];
    board[r][c] = (rand() % 10 == 0) ? 4 : 2;
  }
}

static void reset_game(void) {
  memset(board, 0, sizeof(board));
  spawn_random_tile();
  spawn_random_tile();
  update_tiles();
}

void _2048_init(lv_obj_t *parent) {
  srand((unsigned int)time(NULL));

  lv_style_init(&style_tile);
  lv_style_set_border_width(&style_tile, 2);
  lv_style_set_border_color(&style_tile, lv_palette_main(LV_PALETTE_GREY));

  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *main_cont = lv_obj_create(parent);
  lv_obj_set_size(main_cont, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_center(main_cont);
  lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(main_cont, gesture_fallback_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(main_cont, gesture_fallback_cb, LV_EVENT_RELEASED, NULL);

  lv_obj_t *reset_btn = lv_btn_create(main_cont);
  lv_obj_set_size(reset_btn, 100, 40);
  lv_obj_align(reset_btn, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_add_event_cb(reset_btn, reset_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *label = lv_label_create(reset_btn);
  lv_label_set_text(label, "Reset");
  lv_obj_center(label);

  lv_obj_t *grid = lv_obj_create(main_cont);
  lv_obj_set_size(grid, BOARD_SIZE, BOARD_SIZE);
  lv_obj_align(grid, LV_ALIGN_CENTER, 0, 80);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

  static lv_coord_t col_dsc[] = {LV_PCT(25), LV_PCT(25), LV_PCT(25), LV_PCT(25),
                                 LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {LV_PCT(25), LV_PCT(25), LV_PCT(25), LV_PCT(25),
                                 LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);

  lv_obj_add_event_cb(grid, gesture_fallback_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(grid, gesture_fallback_cb, LV_EVENT_RELEASED, NULL);

  for (int r = 0; r < GRID_SIZE; r++) {
    for (int c = 0; c < GRID_SIZE; c++) {
      tiles[r][c] = lv_obj_create(grid);
      lv_obj_add_style(tiles[r][c], &style_tile, 0);
      lv_obj_clear_flag(tiles[r][c], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scrollbar_mode(tiles[r][c], LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_grid_cell(tiles[r][c], LV_GRID_ALIGN_STRETCH, c, 1,
                           LV_GRID_ALIGN_STRETCH, r, 1);
      lv_obj_t *label = lv_label_create(tiles[r][c]);
      lv_label_set_text(label, "");
      lv_obj_center(label);
    }
  }

  reset_game();
}

app_t _2048_app = {.name = "2048",
                   .icon = LV_SYMBOL_IMAGE,
                   .status = READY,
                   .has_init = false,
                   .should_suspend_on_close = false,
                   .on_init = _2048_init};
