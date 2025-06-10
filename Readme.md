This project was adapted from the DBDK Game Boy [template](https://github.com/gbdk-2020/gbdk-2020/tree/4.4.0/gbdk-lib/examples/gb/template_subfolders)

# Building
- Install make.
- Install GBDK (v4.4.0): https://gbdk.org/docs/api/docs_getting_started.html.
- Make sure you can compile the examples by running `make` from `gbdk/examples`.
- Set `GBDK_HOME` environment variable to GBDK install location (required by makefile). e.g. `export GBDK_HOME=/c/gbdk/`
- Run `make` from root directory of this project.

# Editing Tiles and Tilemaps
- Install GBTD and GBMB
- To use maps and tiles, export as GBDK c ("export to" option)
  - Make sure to set tiles a to b setting in "export to" menu
  - Label in "export to menu" controls the variable names in the generated code
- Things didn't seem to work when using 16x16 tiles in GBTD, but 8x8 tiles did.

# TODO: Misc notes
snippet to run game from CLI: `/c/Program\ Files/bgb/bgb.exe obj/Example.gb &`