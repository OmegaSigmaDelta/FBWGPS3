// Level data model + Tiled loader.
// Ported from init.js: States/Level/Level.parseMapPropper (gid -> device type/properties).
//
// IMPORTANT (verified on the 32 shipped levels): tileset order and firstgid differ per level
// (Chars/Objects/LargeObjects appear at different offsets), so gids MUST be resolved through each
// map's own "tilesets" array - never hardcode ids.
#pragma once
#include "json.hpp"
#include <string>
#include <vector>
#include <map>

namespace fw {

struct MapObject {
    std::string layer;      // "Chars", "Objects", "Tutorial_*"
    int    id   = 0;
    int    gid  = 0;        // 0 for gid-less objects (platform / hanging / pulley / roman)
    std::string type;       // resolved device class key: pusher, lever, box, beamer, rotmirror, lightpusher, ball,
                            // infinitemirror, boxmirror, boxheavy, timedpusher, wind, portal (Objects/LargeObjects),
                            // char, door, diamond (Chars), or Tiled's own type (platform, hanging, pulley, roman)
    std::string name;
    double x = 0, y = 0, w = 0, h = 0, rotation = 0;   // raw Tiled values (tile objects: y is the BOTTOM edge)
    bool   visible = true;
    Json   props;           // Tiled object properties; tileset tile properties only fill keys the object does not set
};

struct Level {
    std::string file;
    int w = 0, h = 0, tileW = 32, tileH = 32;
    std::vector<int> gid;       // Ground tile layer, row-major, 0 = empty
    std::vector<int> type;      // Ground tile type per cell (INT_MIN when empty), from Ground.json tileproperties.type
    std::vector<MapObject> objects;   // layer "Objects"
    std::vector<MapObject> chars;     // layer "Chars" (characters, doors, diamonds)
    std::vector<MapObject> tutorial;  // Tutorial_* layers (text hints)

    static constexpr int NONE = -2147483647 - 1;
    bool hasTile(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h && gid[y * w + x] > 0; }
    int  typeAt(int x, int y) const { return hasTile(x, y) ? type[y * w + x] : NONE; }
};

// gameDir = folder containing game.json (the "game/" folder); levelPath relative to gameDir/data/
// e.g. loadLevel("game", "forest/levels/forest_02.json")
Level loadLevel(const std::string& gameDir, const std::string& levelRelPath);

} // namespace fw
