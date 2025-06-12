#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <stdint.h>
#include "../res/empty_factory.h"
#include "../res/tiles.h"

// Item tile index (3rd tile in our tileset, so index 2)
#define ITEM_TILE 130 // Single tile for item sprite (2 + 128)

// Belt directions
typedef enum
{
    BELT_RIGHT = 0,
    BELT_LEFT = 1,
    BELT_DOWN = 2,
    BELT_UP = 3,
    NO_BELT = 4 // Special value to indicate no belt
} BeltDirection;

// Belt tile indexes (2x2 tiles per belt)
typedef enum
{
    BELT_RIGHT_START = 144, // Base tile for right-facing belt (16 + 128)
    BELT_LEFT_START = 148,  // Base tile for left-facing belt (20 + 128)
    BELT_DOWN_START = 152,  // Base tile for down-facing belt (24 + 128)
    BELT_UP_START = 156     // Base tile for up-facing belt (28 + 128)
} BeltTile;

// Store belt data in RAM (16x16 grid)
BeltDirection belt_grid[16][16];

// Initialize belt grid to empty
void init_belt_grid(void)
{
    for (uint8_t y = 0; y < 16; y++)
    {
        for (uint8_t x = 0; x < 16; x++)
        {
            belt_grid[y][x] = NO_BELT;
        }
    }
}

// Get belt type at position (returns NO_BELT if no belt)
BeltDirection get_belt_at(uint8_t x, uint8_t y)
{
    if (x >= 16 || y >= 16)
        return NO_BELT;
    return belt_grid[y][x];
}

// Place a 2x2 belt at game tile coordinates (x,y)
void place_belt(uint8_t x, uint8_t y, BeltDirection direction)
{
    if (x >= 16 || y >= 16)
        return;

    BeltTile base_tile;
    switch (direction)
    {
    case BELT_RIGHT:
        base_tile = BELT_RIGHT_START;
        break;
    case BELT_LEFT:
        base_tile = BELT_LEFT_START;
        break;
    case BELT_DOWN:
        base_tile = BELT_DOWN_START;
        break;
    case BELT_UP:
        base_tile = BELT_UP_START;
        break;
    default:
        return;
    }

    // Store in RAM
    belt_grid[y][x] = direction;

    // Update VRAM
    uint8_t gb_x = x * 2;
    uint8_t gb_y = y * 2;
    uint8_t tiles[4] = {
        base_tile, base_tile + 2,
        base_tile + 1, base_tile + 3};
    set_bkg_tiles(gb_x, gb_y, 2, 2, tiles);
}

// Create a sprite item at game tile coordinates (x,y)
void create_item(uint8_t x, uint8_t y)
{
    set_sprite_tile(0, ITEM_TILE);
    move_sprite(0, x + 8, y + 16);
    SHOW_SPRITES;

    BGB_printf("Created item at: (%d, %d)", x, y);
}

void init_gfx(void)
{
    // Load Background tiles and then map
    set_bkg_data(128u, 48u, tiles); // load into "block 1" (128-255) which is shared by sprites and background"
    set_bkg_tiles(0, 0, 32u, 32u, empty_factory);

    // Initialize belt grid
    init_belt_grid();

    // Place some example belts
    // Create a simple loop
    for (uint8_t x = 2; x < 6; x++)
    {
        place_belt(x, 2, BELT_RIGHT); // Right belts
    }
    for (uint8_t y = 2; y < 6; y++)
    {
        place_belt(6, y, BELT_DOWN); // Down belts
    }
    for (uint8_t x = 6; x > 2; x--)
    {
        place_belt(x, 6, BELT_LEFT); // Left belts
    }
    for (uint8_t y = 6; y > 2; y--)
    {
        place_belt(2, y, BELT_UP); // Up belts
    }

    // Create an item at the start of the conveyor loop
    create_item(0, 0);

    // Turn the background map on to make it visible
    SHOW_BKG;
}

void main(void)
{
    BGB_MESSAGE("==== Init ====");
    init_gfx();

    // Loop forever
    while (1)
    {
        // Done processing, yield CPU and wait for start of next frame
        vsync();
    }
}
