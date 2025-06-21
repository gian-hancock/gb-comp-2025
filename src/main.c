// TODO: #include <stdbool.h> and use bool rather than uint8_t for booleans

#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <stdint.h>
#include "../res/empty_factory.h"
#include "../res/tiles.h"
#include "util.h"

// Item tile index (3rd tile in our tileset, so index 2)
#define ITEM_TILE 130 // Single tile for item sprite (2 + 128)

// Maximum number of items that can exist at once
#define MAX_ITEMS 10

// Structure to store item data
typedef struct
{
    uint8_t x;
    uint8_t y;
    uint8_t sprite_id; // Track which sprite this item uses
} Item;

// TODO: Consider moving ItemQueue to another file
// Ring buffer queue for items
typedef struct
{
    Item buffer[MAX_ITEMS];
    uint8_t front; // Index of the front (oldest) item
    uint8_t back;  // Index of the back (newest) item
    uint8_t size;  // Number of items in the queue
} ItemQueue;

// Global queue instance
ItemQueue item_queue;
uint8_t next_sprite_id = 0; // Track next available sprite

// Queue operations
void queue_init(ItemQueue *queue)
{
    queue->front = 0;
    queue->back = 0;
    queue->size = 0;
}

uint8_t queue_is_empty(ItemQueue *queue)
{
    return queue->size == 0;
}

uint8_t queue_is_full(ItemQueue *queue)
{
    return queue->size == MAX_ITEMS;
}

uint8_t queue_enqueue(ItemQueue *queue, Item item)
{
    if (queue_is_full(queue))
    {
        return 0; // Queue is full
    }

    queue->buffer[queue->back] = item;
    queue->back = (queue->back + 1) % MAX_ITEMS;
    queue->size++;
    return 1; // Success
}

uint8_t queue_dequeue(ItemQueue *queue, Item *item)
{
    if (queue_is_empty(queue))
    {
        return 0; // Queue is empty
    }

    *item = queue->buffer[queue->front];
    queue->front = (queue->front + 1) % MAX_ITEMS;
    queue->size--;
    return 1; // Success
}

uint8_t queue_peek(ItemQueue *queue, Item *item)
{
    if (queue_is_empty(queue))
    {
        return 0; // Queue is empty
    }

    *item = queue->buffer[queue->front];
    return 1; // Success
}

// Get item at specific index in queue (0 = oldest, size-1 = newest)
uint8_t queue_get_at(ItemQueue *queue, uint8_t index, Item *item)
{
    if (index >= queue->size)
    {
        return 0; // Index out of bounds
    }

    uint8_t actual_index = (queue->front + index) % MAX_ITEMS;
    *item = queue->buffer[actual_index];
    return 1; // Success
}

// Set item at specific index in queue (0 = oldest, size-1 = newest)
uint8_t queue_set_at(ItemQueue *queue, uint8_t index, Item item)
{
    if (index >= queue->size)
    {
        return 0; // Index out of bounds
    }

    uint8_t actual_index = (queue->front + index) % MAX_ITEMS;
    queue->buffer[actual_index] = item;
    return 1; // Success
}

// Belt directions
typedef enum
{
    BELT_RIGHT = 0,
    BELT_LEFT = 1,
    BELT_DOWN = 2,
    BELT_UP = 3,
    NO_BELT = 4 // Special value to indicate no belt
} BeltDirection;

// Belt tile indexes
typedef enum
{
    BELT_UP_START = 208,    // Single tile for up-facing belt (80 + 128)
    BELT_DOWN_START = 209,  // Single tile for down-facing belt (81 + 128)
    BELT_LEFT_START = 210,  // Single tile for left-facing belt (82 + 128)
    BELT_RIGHT_START = 211, // Single tile for right-facing belt (83 + 128)
} BeltTile;

// TODO: is 16x16 the correct size now?
// Store belt data in RAM (16x16 grid)
BeltDirection belt_grid[16][16];

// Function prototypes
uint8_t boxes_overap_8x8(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

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
    uint8_t gb_x = x;
    uint8_t gb_y = y;
    uint8_t tiles[1] = {base_tile};
    set_bkg_tiles(gb_x, gb_y, 1, 1, tiles);
}

// Check if a position would collide with any existing item
uint8_t would_collide_at_position(uint8_t x, uint8_t y)
{
    // TODO: understand +8 and +16
    // Convert to pixel coordinates
    uint8_t pixel_x = x + 8;
    uint8_t pixel_y = y + 16;

    // Check collision with every existing item
    for (uint8_t i = 0; i < item_queue.size; i++)
    {
        Item item;
        if (queue_get_at(&item_queue, i, &item))
        {
            uint8_t other_x = item.x + 8;
            uint8_t other_y = item.y + 16;
            if (boxes_overap_8x8(pixel_x, pixel_y, other_x, other_y))
            {
                return 1; // Collision detected
            }
        }
    }
    return 0; // No collision
}

// Create a sprite item at game tile coordinates (x,y)
void create_item(uint8_t x, uint8_t y)
{
    BGB_printf("create_item(%d, %d)", x, y);
    if (queue_is_full(&item_queue))
    {
        ASSERT(0, "Queue is full in create_item");
        BGB_printf("Failed to create item - max items reached");
        return;
    }

    // Check if the position would collide with any existing item
    if (would_collide_at_position(x, y))
    {
        BGB_printf("Failed to create item - collision detected at (%d, %d)", x, y);
        return;
    }

    // Store item data
    Item item;
    item.x = x;
    item.y = y;
    item.sprite_id = next_sprite_id;
    if (queue_enqueue(&item_queue, item))
    {
        // Set up sprite with unique sprite ID
        set_sprite_tile(next_sprite_id, ITEM_TILE);
        move_sprite(next_sprite_id, x + 8, y + 16);

        // Move to next sprite (wrap around at 40 sprites)
        // TODO: How to handle when we have multiple different types of items?
        next_sprite_id = (next_sprite_id + 1) % 40;

        BGB_printf("Created item at: (%d, %d) with sprite %d", x, y, item.sprite_id);
    }
}

// Initialize items array
void init_items(void)
{
    queue_init(&item_queue);
    next_sprite_id = 0;
}

// Delete the oldest item from the queue
void delete_oldest_item(void)
{
    Item item;
    if (queue_dequeue(&item_queue, &item))
    {
        // Hide the sprite by moving it off screen
        move_sprite(item.sprite_id, 0, 0);
        BGB_printf("Deleted oldest item at: (%d, %d) with sprite %d", item.x, item.y, item.sprite_id);
    }
    else
    {
        BGB_printf("No items to delete");
    }
}

// Check if an 8x8 box at (x,y) would be fully on screen
uint8_t is_on_screen(uint8_t x, uint8_t y)
{
    // Game Boy screen is 160x144 pixels
    // Check if any part of 8x8 box would be off screen
    return (x < 152 && y < 136); // 160-8 = 152, 144-8 = 136
}

// Check if two 8x8 boxes overlap
uint8_t boxes_overap_8x8(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    return !(x1 + 8 <= x2 || x2 + 8 <= x1 || y1 + 8 <= y2 || y2 + 8 <= y1);
}

// Check if moving an item would collide with any other item
uint8_t would_collide_with_any_item(uint8_t item_index, uint8_t new_x, uint8_t new_y)
{
    // Convert to pixel coordinates
    uint8_t pixel_x = new_x + 8;
    uint8_t pixel_y = new_y + 16;

    // Check collision with every other item
    for (uint8_t i = 0; i < item_queue.size; i++)
    {
        Item item;
        if (queue_get_at(&item_queue, i, &item))
        {
            if (i != item_index)
            { // Don't check collision with self
                uint8_t other_x = item.x + 8;
                uint8_t other_y = item.y + 16;
                if (boxes_overap_8x8(pixel_x, pixel_y, other_x, other_y))
                {
                    return 1; // Collision detected
                }
            }
        }
    }
    return 0; // No collision
}

// Update all items' positions
void update_items(void)
{
    // Process items in order (oldest first)
    for (uint8_t i = 0; i < item_queue.size; i++)
    {
        Item item;
        uint8_t success = queue_get_at(&item_queue, i, &item);
        ASSERT(success, "Failed to get item from queue");

        // Check if item is on belt
        BeltDirection belt_dir = get_belt_at((item.x + 4) / 8, (item.y + 4) / 8); // TODO: Use constants for 4 and 8
        if (belt_dir == NO_BELT)
        {
            continue; // Item is not on belt, so don't move it
        }

        // Convert from tile to pixel coordinates
        // TODO: explain magic 8 & 16
        uint8_t pixel_x = item.x + 8;
        uint8_t pixel_y = item.y + 16;

        // Calculate new position based on belt direction
        uint8_t new_x = item.x;
        uint8_t new_y = item.y;
        uint8_t new_pixel_x = pixel_x;
        uint8_t new_pixel_y = pixel_y;

        switch (belt_dir)
        {
        case BELT_RIGHT:
            new_x = item.x + 1;
            new_pixel_x = pixel_x + 1;
            break;
        case BELT_LEFT:
            new_x = item.x - 1;
            new_pixel_x = pixel_x - 1;
            break;
        case BELT_DOWN:
            new_y = item.y + 1;
            new_pixel_y = pixel_y + 1;
            break;
        case BELT_UP:
            new_y = item.y - 1;
            new_pixel_y = pixel_y - 1;
            break;
        default:
            continue; // Should not happen since we already checked for NO_BELT
        }

        // Check if moving would keep item on screen and not collide with other items
        Item collision_candidate;
        uint8_t has_collision_candidate = queue_get_at(&item_queue, i - 1, &collision_candidate);
        uint8_t would_collide = has_collision_candidate &&
                                boxes_overap_8x8(collision_candidate.x, collision_candidate.y, new_x, new_y);
        uint8_t on_screen = is_on_screen(new_pixel_x, new_pixel_y);
        if (on_screen && !would_collide)
        {
            // TODO: Consider using index in underlying queue buffer as sprite_id?
            // Move sprite using this item's unique sprite
            move_sprite(item.sprite_id, new_pixel_x, new_pixel_y);

            // Update stored position (in tile coordinates)
            item.x = new_x;
            item.y = new_y;

            // Store the updated item back to the queue
            queue_set_at(&item_queue, i, item);
        }
    }
}

void init_gfx(void)
{
    // Load Background tiles and then map
    // TODO: Loading 128, but not actually using that many
    set_bkg_data(128u, 128u, tiles); // load into "block 1" (128-255) which is shared by sprites and background"
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
    // for (uint8_t y = 6; y > 2; y--)
    // {
    //     place_belt(2, y, BELT_UP); // Up belts
    // }

    // Turn the background map on to make it visible
    SHOW_BKG;
    SHOW_SPRITES;
}

void main(void)
{
    BGB_MESSAGE("==== Init ====");
#ifdef BGB_DEBUG
    BGB_printf("BGB_DEBUG: %d", BGB_DEBUG);
#endif
    init_gfx();

    uint8_t frame_counter = 0; // Count frames for item spawning
    uint8_t prev_buttons = 0;  // Track previous button state

    // Loop forever
    while (1)
    {
        // Handle input
        uint8_t buttons = joypad();
        uint8_t a_pressed = (buttons & J_A) && !(prev_buttons & J_A);
        if (a_pressed)
        {
            delete_oldest_item();
        }
        prev_buttons = buttons;

        // Update frame counter and spawn items
        frame_counter = (frame_counter + 1) % 32;
        if (frame_counter == 0)
        {
            // Spawn new item at the start of the conveyor loop
            create_item(16, 16);
        }

        update_items();

        // Done processing, yield CPU and wait for start of next frame
        vsync();
    }
}