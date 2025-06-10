#include <gb/gb.h>
#include <stdint.h>
#include "../res/example_factory_1_small.h"
#include "../res/tiles_small.h"
#include "../res/example_factory_1.h"
#include "../res/tiles.h"
#include "../res/dungeon_map.h"
#include "../res/dungeon_tiles.h"

void init_gfx(void)
{
    // Load Background tiles and then map
    set_bkg_data(0, 48u, tiles_small);
    set_bkg_tiles(0, 0, 32u, 32u, example_factory_1_small);

    // set_bkg_data(0, 46u, tiles);
    // set_bkg_tiles(0, 0, 46u, 18u, example_factory_1);

    // set_bkg_data(0, 79u, dungeon_tiles);
    // set_bkg_tiles(0, 0, 32u, 32u, dungeon_mapPLN0);

    // Turn the background map on to make it visible
    SHOW_BKG;
}

void main(void)
{
    init_gfx();

    // Loop forever
    while (1)
    {

        // Game main loop processing goes here

        // Done processing, yield CPU and wait for start of next frame
        vsync();
    }
}
