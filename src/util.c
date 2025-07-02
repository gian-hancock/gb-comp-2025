#include "util.h"
#include <gb/gb.h>

uint8_t boxes_overap_8x8(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  return !(x1 + 8 <= x2 || x2 + 8 <= x1 || y1 + 8 <= y2 || y2 + 8 <= y1);
}

void set_bkg_tiles_2x2(uint8_t x, uint8_t y, uint8_t base_tile) {
  uint8_t tiles[4] = {
      base_tile,     // Top-left
      base_tile + 2, // Top-right
      base_tile + 1, // Bottom-left
      base_tile + 3  // Bottom-right
  };

  // Set the 2x2 tile area
  set_bkg_tiles(x, y, 2, 2, tiles);
}

void set_bkg_tiles_4x4(uint8_t x, uint8_t y, uint8_t base_tile) {
  uint8_t tiles[16] = {
      base_tile,     base_tile + 2, base_tile + 8,  base_tile + 10,
      base_tile + 1, base_tile + 3, base_tile + 9,  base_tile + 11,
      base_tile + 4, base_tile + 6, base_tile + 12, base_tile + 14,
      base_tile + 5, base_tile + 7, base_tile + 13, base_tile + 15};

  // Set the 4x4 tile area
  set_bkg_tiles(x, y, 4, 4, tiles);
}

uint8_t aabb_overlap(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1, uint8_t x2,
                     uint8_t y2, uint8_t w2, uint8_t h2) {
  return !(x1 + w1 <= x2 || x2 + w2 <= x1 || y1 + h1 <= y2 || y2 + h2 <= y1);
}