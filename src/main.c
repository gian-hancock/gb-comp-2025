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

typedef enum
{
    ITEM_TYPE_COG,
    ITEM_TYPE_CHIP,
    ITEM_TYPE_MOTOR,
} ItemType;

// Structure to store item data
typedef struct
{
    // Coords of top left corner. (0, 0) represents the top left of the screen.
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
    ItemType item_type;
} ItemQueue;

typedef struct
{
    uint8_t item_capacity;
    uint8_t cog_count;
    uint8_t chip_count;
    // Coords of top left corner (in pixels). (0, 0) represents the top left of the screen.
    uint8_t x;
    uint8_t y;
} AssemblyMachine;

typedef struct
{
    uint8_t counter;
    uint8_t frequency;
    ItemQueue *queue;
    ItemType item_type;
    // Coords to place item at.
    uint8_t x;
    uint8_t y;
} ItemSpawner;

// State
ItemQueue queue_chips;
ItemQueue queue_cogs;
ItemQueue queue_motors;

// TODO: Use index in underlying queue buffer as sprite_id?
uint8_t next_sprite_id = 0; // Track next available sprite
AssemblyMachine assembly_machine;
ItemSpawner spawner_cog = {
    .counter = 0,
    .frequency = 32,
    .queue = &queue_cogs, // TODO: Rename to chip_queue
    .item_type = ITEM_TYPE_COG,
    .x = 8,
    .y = 10 * 8};
ItemSpawner spawner_chip = {
    .counter = 0,
    .frequency = 32,
    .queue = &queue_chips, // TODO: Rename to chip_queue
    .item_type = ITEM_TYPE_CHIP,
    .x = 16,
    .y = 16};

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

    // TODO: duplicated code
    // Check collision with every existing item in chips queue
    for (uint8_t i = 0; i < queue_chips.size; i++)
    {
        Item item;
        if (queue_get_at(&queue_chips, i, &item))
        {
            uint8_t other_x = item.x + 8;
            uint8_t other_y = item.y + 16;
            if (boxes_overap_8x8(pixel_x, pixel_y, other_x, other_y))
            {
                return 1; // Collision detected
            }
        }
    }

    // Check collision with every existing item in cogs queue
    for (uint8_t i = 0; i < queue_cogs.size; i++)
    {
        Item item;
        if (queue_get_at(&queue_cogs, i, &item))
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
void create_item(uint8_t x, uint8_t y, ItemQueue *queue)
{
    BGB_printf("create_item(%d, %d)", x, y);
    if (queue_is_full(queue))
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
    if (queue_enqueue(queue, item))
    {
        // Set up sprite with unique sprite ID
        set_sprite_tile(next_sprite_id, queue->item_type == ITEM_TYPE_COG ? TILE_COG : TILE_CHIP);
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

    // Initialize assembly machine
    assembly_machine.item_capacity = 5;
    assembly_machine.cog_count = 0;
    assembly_machine.chip_count = 0;
    assembly_machine.x = grid_x * 8;
    assembly_machine.y = grid_y * 8;
}

// Initialize items array
void init_items(void)
{
    queue_chips.front = 0;
    queue_chips.back = 0;
    queue_chips.size = 0;
    queue_chips.item_type = ITEM_TYPE_CHIP;

    queue_cogs.front = 0;
    queue_cogs.back = 0;
    queue_cogs.size = 0;
    queue_cogs.item_type = ITEM_TYPE_COG;

    queue_motors.front = 0;
    queue_motors.back = 0;
    queue_motors.size = 0;
    queue_motors.item_type = ITEM_TYPE_MOTOR;

    next_sprite_id = 0;
}

// Delete the oldest item from the queue
void delete_oldest_item(ItemQueue *queue)
{
    Item item;
    if (queue_dequeue(queue, &item))
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
void update_items(ItemQueue *queue)
{
    // TODO: Consistent terminology use "assembler" over "assembly machine"
    // TODO: The first item should really be moved before checking for assembler touch
    // Check if oldest item touches assembler
    Item oldest_item;
    uint8_t item_count = queue->item_type == ITEM_TYPE_COG ? assembly_machine.cog_count : assembly_machine.chip_count;
    if (queue_peek(queue, &oldest_item) && item_count < assembly_machine.item_capacity)
    {
        if (aabb_overlap(oldest_item.x, oldest_item.y, 8, 8, assembly_machine.x, assembly_machine.y, 16, 16))
        {
            delete_oldest_item(queue);
            // Determine which counter to increment based on which queue this is
            if (queue == &queue_cogs)
            {
                assembly_machine.cog_count++;
            }
            else if (queue == &queue_chips)
            {
                assembly_machine.chip_count++;
            }
        }
    }

    // Process items in order (oldest first)
    for (uint8_t i = 0; i < queue->size; i++)
    {
        Item item;
        uint8_t success = queue_get_at(queue, i, &item);
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
        uint8_t has_collision_candidate = queue_get_at(queue, i - 1, &collision_candidate);
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
            queue_set_at(queue, i, item);
        }
    }
}

void update_spawner(ItemSpawner *spawner)
{
    spawner->counter++;
    if (spawner->counter >= spawner->frequency)
    {
        spawner->counter = 0;
        // Only spawn if queue is not at limit
        if (!queue_is_full(spawner->queue))
        {
            create_item(spawner->x, spawner->y, spawner->queue);
        }
    }
}

void update_assembly_machine(AssemblyMachine *assembly_machine)
{
    // Check if there's space to spawn a motor at the assemblers spawn point.
    if (assembly_machine->cog_count == assembly_machine->item_capacity &&
        assembly_machine->chip_count == assembly_machine->item_capacity)
    {
        // Check if motors queue is not full
        if (!queue_is_full(&queue_motors))
        {
            // Check for collision only with the last motor in the queue
            uint8_t would_collide = 0;
            if (queue_motors.size > 0)
            {
                Item last_motor;
                // Get the last item in the queue (at back-1)
                uint8_t last_index = (queue_motors.back - 1 + MAX_ITEMS) % MAX_ITEMS;
                last_motor = queue_motors.buffer[last_index];

                // Convert to pixel coordinates for collision check
                uint8_t spawn_pixel_x = assembly_machine->x;
                uint8_t spawn_pixel_y = assembly_machine->y;
                uint8_t motor_pixel_x = last_motor.x;
                uint8_t motor_pixel_y = last_motor.y;

                would_collide = boxes_overap_8x8(spawn_pixel_x, spawn_pixel_y, motor_pixel_x, motor_pixel_y);
            }

            if (!would_collide)
            {
                create_item(assembly_machine->x + 8, assembly_machine->y + 16, &queue_motors);
                assembly_machine->cog_count = 0;
                assembly_machine->chip_count = 0;
            }
        }
    }
}

void init_factory(void)
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

    place_belt(1, 10, BELT_UP);
    place_belt(1, 9, BELT_UP);
    place_belt(1, 8, BELT_UP);
    place_belt(1, 7, BELT_UP);

    place_belt(2, 12, BELT_DOWN);
    place_belt(2, 11, BELT_DOWN);
    place_belt(2, 10, BELT_DOWN);
    place_belt(2, 9, BELT_DOWN);
    place_belt(2, 8, BELT_DOWN);
    place_belt(2, 7, BELT_DOWN);

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
    init_factory();

    uint8_t frame_counter = 0; // Count frames for item spawning
    uint8_t prev_buttons = 0;  // Track previous button state

    // Loop forever
    while (1)
    {
        // TODO: Game loop
        // 1. Move/Consume. Move items, they can be moved into an assembler if there's room
        // 2. Spawn. Spawners and assemblers spawn if there's room

        // Handle input
        uint8_t buttons = joypad();
        uint8_t a_pressed = (buttons & J_A) && !(prev_buttons & J_A);
        if (a_pressed)
        {
            delete_oldest_item(&queue_chips);
        }
        prev_buttons = buttons;

        // Move existing items
        update_items(&queue_chips);
        update_items(&queue_cogs);
        update_items(&queue_motors);

        // Spawn new items
        update_spawner(&spawner_chip);
        update_spawner(&spawner_cog);
        update_assembly_machine(&assembly_machine);

        // Done processing, yield CPU and wait for start of next frame
        vsync();
    }
}