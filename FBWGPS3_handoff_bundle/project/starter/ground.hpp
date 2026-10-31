// Terrain builder. Direct port of States/Level/Ground.convertTilemap / createGroundShape (init.js).
//
// Output is engine-neutral: collision polygons in SCREEN PIXELS (y down), exactly what the original
// feeds to Box2D. Feed them to b2PolygonShape after dividing by PTM (32 px = 1 m) and applying your own
// world orientation (the Phaser-Box2D plugin stored coordinates mirrored: box2d = -px/32; a native port can
// simply use +px/32 with +y down and gravity (0, +31.25) m/s^2 = 1000 px/s^2).
//
// Fixture params used by the original: friction 0.8, categoryBits 1, maskBits 0x1111 (4369).
#pragma once
#include "level.hpp"

namespace fw {

struct Pt { double x, y; };

struct GroundFixture {
    std::vector<Pt> v;      // convex polygon, <= 8 vertices (pixels)
};

struct LiquidDef {          // Ground.CreateLiquid(type, B+1, C, A): a pool of `width` tiles starting at tile (tx, ty)
    int type;               // tile type 5, 6, 7 or 55 (see Liquid device for what each type means)
    int tx, ty, width;
};

struct DrawOp {             // Ground.drawTile(ctx, px, py, type): draw atlas frame "MaskBox<type>0000" (GroundAssets)
    int px, py;             // top-left of the 32x32 cell in pixels (the frame is centred on it, see drawTile)
    int frameType;          // already normalised: 0..9 (0 = pick one of MaskBox00000..3)
};

struct GroundResult {
    std::vector<GroundFixture> fixtures;
    std::vector<LiquidDef> liquids;
    std::vector<DrawOp> draws;
    static constexpr double friction = 0.8;
    static constexpr int categoryBits = 1, maskBits = 4369;
};

GroundResult buildGround(const Level& L);

} // namespace fw
