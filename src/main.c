// TODO: #include <stdbool.h> and use bool rather than uint8_t for booleans

#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <stdint.h>
#include "../res/empty_factory.h"
#include "../res/tiles.h"
#include "util.h"
#include "tiles.h"

// Maximum number of items that can exist at once
#define MAX_ITEMS 4
#define FACTORY_GRID_WIDTH 16
#define FACTORY_GRID_HEIGHT 16

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
    EMPTY = 4, // Special value to indicate no belt,
    ASSEMBLY_MACHINE = 5
} FactoryTile;

// TODO: is 16x16 the correct size now?
// Store belt data in RAM (16x16 grid)
FactoryTile factory_tiles[FACTORY_GRID_HEIGHT][FACTORY_GRID_WIDTH];

// Initialize belt grid to empty
void init_belt_grid(void)
{
    for (uint8_t y = 0; y < 16; y++)
    {
        for (uint8_t x = 0; x < 16; x++)
        {
            factory_tiles[y][x] = EMPTY;
        }
    }
}

// Get belt type at position (returns NO_BELT if no belt)
FactoryTile get_factory_tile_at_pixel(uint8_t x, uint8_t y)
{
    if (x >= 16 || y >= 16)
        return EMPTY;
    return factory_tiles[y][x];
}

void place_belt(uint8_t x, uint8_t y, FactoryTile beltTile)
{
    ASSERT(x >= 0 && x < FACTORY_GRID_WIDTH && y >= 0 && y < FACTORY_GRID_HEIGHT, "place_belt: Out of bounds");
    ASSERT(beltTile == BELT_RIGHT || beltTile == BELT_LEFT || beltTile == BELT_DOWN || beltTile == BELT_UP,
           "place_belt: Invalid tile");

    uint8_t base_tile;
    switch (beltTile)
    {
    case BELT_RIGHT:
        base_tile = TILE_BELT_RIGHT;
        break;
    case BELT_LEFT:
        base_tile = TILE_BELT_LEFT;
        break;
    case BELT_DOWN:
        base_tile = TILE_BELT_DOWN;
        break;
    case BELT_UP:
        base_tile = TILE_BELT_UP;
        break;
    default:
        return;
    }

    // Store in RAM
    factory_tiles[y][x] = beltTile;

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

    // TODO: (Optimisation) Only check last item in queue
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
        set_sprite_tile(next_sprite_id, TILE_ITEM);
        move_sprite(next_sprite_id, x + 8, y + 16);

        // Move to next sprite (wrap around at 40 sprites)
        // TODO: How to handle when we have multiple different types of items?
        next_sprite_id = (next_sprite_id + 1) % 40;

        BGB_printf("Created item at: (%d, %d) with sprite %d", x, y, item.sprite_id);
    }
}

void create_assembly_machine(uint8_t grid_x, uint8_t grid_y)
{
    ASSERT(grid_x >= 0 && grid_x < FACTORY_GRID_WIDTH && grid_y >= 0 && grid_y < FACTORY_GRID_HEIGHT,
           "Assembly machine out of bounds");

    // Add assembly machine to the grid
    factory_tiles[grid_y][grid_x] = ASSEMBLY_MACHINE;
    factory_tiles[grid_y + 1][grid_x] = ASSEMBLY_MACHINE;
    factory_tiles[grid_y][grid_x + 1] = ASSEMBLY_MACHINE;
    factory_tiles[grid_y + 1][grid_x + 1] = ASSEMBLY_MACHINE;

    // Set background tiles
    set_bkg_tiles_2x2(grid_x, grid_y, TILE_ASSEMBLY_MACHINE);
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
        FactoryTile belt_dir = get_factory_tile_at_pixel((item.x + 4) / 8, (item.y + 4) / 8); // TODO: Use constants for 4 and 8
        if (belt_dir == EMPTY)
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

    // place assembly machine
    create_assembly_machine(1, 5);

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