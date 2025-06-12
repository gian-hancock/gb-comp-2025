#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <stdint.h>
#include "../res/empty_factory.h"
#include "../res/tiles.h"

// Item tile index (3rd tile in our tileset, so index 2)
#define ITEM_TILE 130 // Single tile for item sprite (2 + 128)

// Maximum number of items that can exist at once
#define MAX_ITEMS 16

// Structure to store item data
typedef struct
{
    uint8_t x;
    uint8_t y;
    uint8_t sprite_id; // Track which sprite this item uses
} Item;

// Array to store all items
Item items[MAX_ITEMS];
uint8_t num_items = 0;      // Track number of active items
uint8_t next_sprite_id = 0; // Track next available sprite

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
    if (num_items >= MAX_ITEMS)
    {
        BGB_printf("Failed to create item - max items reached");
        return;
    }

    // Store item data
    items[num_items].x = x;
    items[num_items].y = y;
    items[num_items].sprite_id = next_sprite_id;
    num_items++;

    // Set up sprite with unique sprite ID
    set_sprite_tile(next_sprite_id, ITEM_TILE);
    move_sprite(next_sprite_id, x + 8, y + 16);

    // Move to next sprite (wrap around at 40 sprites)
    // TODO: How to handle when we have multiple different types of items?
    next_sprite_id = (next_sprite_id + 1) % 40;

    BGB_printf("Created item at: (%d, %d) with sprite %d", x, y, items[num_items - 1].sprite_id);
    SHOW_SPRITES;
}

// Initialize items array
void init_items(void)
{
    num_items = 0;
    next_sprite_id = 0;
}

// Check if an 8x8 box at (x,y) would be fully on screen
uint8_t is_on_screen(uint8_t x, uint8_t y)
{
    // Game Boy screen is 160x144 pixels
    // Check if any part of 8x8 box would be off screen
    return (x < 152 && y < 136); // 160-8 = 152, 144-8 = 136
}

// Check if two 8x8 boxes overlap
uint8_t boxes_overlap(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    // Check if boxes overlap by seeing if one box is completely to the left, right, above, or below the other
    return !(x1 + 8 <= x2 || x2 + 8 <= x1 || y1 + 8 <= y2 || y2 + 8 <= y1);
}

// Check if moving an item would collide with any other item
uint8_t would_collide_with_any_item(uint8_t item_index, uint8_t new_x, uint8_t new_y)
{
    // Convert to pixel coordinates
    uint8_t pixel_x = new_x + 8;
    uint8_t pixel_y = new_y + 16;

    // TODO: Can maybe only check for collision with next item due to fifo constraints?
    // Check collision with every other item
    for (uint8_t i = 0; i < num_items; i++)
    {
        if (i != item_index)
        { // Don't check collision with self
            uint8_t other_x = items[i].x + 8;
            uint8_t other_y = items[i].y + 16;
            if (boxes_overlap(pixel_x, pixel_y, other_x, other_y))
            {
                return 1; // Collision detected
            }
        }
    }
    return 0; // No collision
}

// Update all items' positions
void update_items(void)
{
    // Process items in order (oldest first)
    for (uint8_t i = 0; i < num_items; i++)
    {
        // Convert from tile to pixel coordinates
        uint8_t pixel_x = items[i].x + 8;
        uint8_t pixel_y = items[i].y + 16;

        // Check if moving down would keep item on screen and not collide with other items
        if (is_on_screen(pixel_x, pixel_y + 1) &&
            !would_collide_with_any_item(i, items[i].x, items[i].y + 1))
        {
            // Move sprite down by 1 pixel using this item's unique sprite
            move_sprite(items[i].sprite_id, pixel_x, pixel_y + 1);

            // Update stored position (in tile coordinates)
            items[i].y = (pixel_y + 1 - 16);
        }
    }
}

void init_gfx(void)
{
    // Load Background tiles and then map
    set_bkg_data(128u, 48u, tiles); // load into "block 1" (128-255) which is shared by sprites and background"
    set_bkg_tiles(0, 0, 32u, 32u, empty_factory);

    // Initialize belt grid and items
    init_belt_grid();
    init_items();

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

    uint8_t frame_counter = 0; // Count frames for item spawning

    // Loop forever
    while (1)
    {
        // Update frame counter and spawn items
        frame_counter = (frame_counter + 1) % 32;
        if (frame_counter == 0 && num_items < 5)
        {
            // Spawn new item at the start of the conveyor loop
            create_item(0, 0);
        }

        update_items();

        // Done processing, yield CPU and wait for start of next frame
        vsync();
    }
}
