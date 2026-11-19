// [Minty1804 fork] The detailed city further (willzmkv's HD world, their fork; docs/game-internals.md 6t): the detailed
// city drawn further than the game's ~260 m - the cells around the camera read into the port's memory and drawn
// through the game's own world draw.
#pragma once

class PS2Runtime;

namespace vcs::world_hd
{
    // Enhancements > World > Detailed city distance (world.hd_radius, VCS_HD_RADIUS; live).
    void registerSettings();
    // The hooks on the area chooser / swap / flush (the game's list back before its streaming) and the world draw (the
    // list with the extra cells); the `hdworld` command (cells, memory, records added).
    void install(PS2Runtime &runtime);
    // The setting's value now (0 Original .. 3 ~720 m); game/vcs_memory.cpp moves the sector packet heap when on at boot.
    int radius();
}
