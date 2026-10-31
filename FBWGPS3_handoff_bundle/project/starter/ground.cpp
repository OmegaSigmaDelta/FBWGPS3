#include "ground.hpp"
#include <initializer_list>
#include <utility>

namespace fw {
namespace {

constexpr double PTM = 32.0;   // pixels per metre (Phaser Box2D setPTMRatio(32))

// The original builds each polygon in metres relative to a base point b (metres) as  X = pt - b  and the plugin stores
// coordinates mirrored, so the screen pixel position is  px = (b - pt) * 32.  `bx,by` are b already in pixels.
std::vector<Pt> poly(double bx, double by, std::initializer_list<std::pair<double, double>> pts) {
    std::vector<Pt> out;
    for (auto& p : pts) out.push_back({bx - p.first * PTM, by - p.second * PTM});
    return out;
}

// States/Level/Ground.createGroundShape(type, base)
std::vector<Pt> groundShape(int t, double bx, double by) {
    if (t == 0 || t == 10)                    return poly(bx, by, {{0, 0}, {-1, 0}, {-1, -1}, {0, -1}});       // full square
    if (t == 1 || t == -2 || t == 11)         return poly(bx, by, {{-1, 0}, {-1, -1}, {0, -1}});               // triangle: lower-right
    if (t == 2 || t == 12)                    return poly(bx, by, {{0, 0}, {-1, -1}, {0, -1}});                // triangle: lower-left
    if (t == 3)                               return poly(bx, by, {{0, 0}, {-1, 0}, {-1, -1}});                // triangle: upper-right
    if (t == 4)                               return poly(bx, by, {{0, 0}, {-1, 0}, {0, -1}});                 // triangle: upper-left
    if (t == 5 || t == 6 || t == 7)           return poly(bx, by, {{0, -0.5}, {-1, -0.5}, {-1, -1}, {0, -1}}); // lower half slab
    if (t == 8)                               return poly(bx, by, {{0, 0}, {-1.3, -0.5}, {-1.3, -1}, {0, -1}});// pool left edge wedge
    if (t == 9)                               return poly(bx, by, {{0.3, -0.5}, {-1, 0}, {-1, -1}, {0.3, -1}});// pool right edge wedge
    return {};
}

// Ground.drawTile type normalisation
int drawType(int t) { return t == 10 ? 0 : t == 11 ? 1 : t == 12 ? 2 : t == 55 ? 5 : t; }

// Box2D quirk reproduced on purpose: b2PolygonShape::Set() with fewer than 3 distinct vertices (zero-area input)
// falls back to a 2m x 2m box centred on the body origin (here: px -32..32). The original game contains these stray
// boxes for 2-tile-wide liquid pools (the "slab" between the edge wedges has zero width). Keep for exact parity.
std::vector<Pt> finalizePoly(std::vector<Pt> v) {
    std::vector<Pt> u;
    for (auto& p : v) { bool dup = false; for (auto& q : u) if (q.x == p.x && q.y == p.y) { dup = true; break; } if (!dup) u.push_back(p); }
    if (u.size() < 3) return {{-PTM, -PTM}, {PTM, -PTM}, {PTM, PTM}, {-PTM, PTM}};
    return v;
}

bool isLiquid(int t) { return t == 5 || t == 6 || t == 7 || t == 55; }

} // namespace

GroundResult buildGround(const Level& L) {
    GroundResult R;
    const double TW = L.tileW, TH = L.tileH;

    for (int C = 0; C < L.h; ++C) {
        int A = 0;                       // run length counter (original keeps it per row)
        double v = 0, w = 0;             // run start in pixels
        for (int B = 0; B < L.w; ++B) {
            if (!L.hasTile(B, C)) continue;
            const int t = L.typeAt(B, C);
            const double d = B * TW, e = C * TH;
            const int tr = L.typeAt(B + 1, C);                 // right neighbour (NONE if absent)
            const bool rightExists = L.hasTile(B + 1, C);

            if (t == 0) {
                // ---- solid ground: merge horizontal runs into one box ----
                if (A == 0) { v = B * TW; w = C * TH; A = 1; }
                if (rightExists && tr == 0) {
                    A += 1; R.draws.push_back({(int)d, (int)e, drawType(t)});
                } else {
                    // polygon (0,0),(-u,0),(-u,-s),(0,-s) with u = A tiles, s = 1 tile, base = run start (v,w)
                    R.fixtures.push_back({poly(v, w, {{0, 0}, {-(double)A, 0}, {-(double)A, -1}, {0, -1}})});
                    R.draws.push_back({(int)d, (int)e, drawType(t)});
                    A = 0;
                }
            } else if (isLiquid(t)) {
                // ---- liquid pool (types 5/6/7/55): merge run, add two edge wedges + a slab ----
                if (A == 0) { v = B * TW; w = C * TH; A = 1; }
                if (rightExists && tr == t) {
                    if (A > 1) R.draws.push_back({(int)d, (int)e, drawType(t)});
                    A += 1;
                } else {
                    // left edge wedge (shape 8) anchored on the first tile of the run
                    R.fixtures.push_back({groundShape(8, d - PTM * A + PTM, e)});
                    R.draws.push_back({(int)(d - TW * (A - 1)), (int)e, 8});
                    // right edge wedge (shape 9) on the last tile
                    R.fixtures.push_back({groundShape(9, d, e)});
                    R.draws.push_back({(int)d, (int)e, 9});
                    // slab between the wedges: x in [v+32, v+32*(A-1)], y in [w+14.4, w+32]
                    const double u = A, s = 1;
                    R.fixtures.push_back({finalizePoly(poly(v, w, {{-1, -s / 2 + 0.05}, {-u + 1, -s / 2 + 0.05}, {-u + 1, -s}, {-1, -s}}))});
                    R.liquids.push_back({t, B + 1 - A, C, A});
                    A = 0;
                }
            } else if (t == -1) {
                // decoration-only cell (no collision)
                R.draws.push_back({(int)d, (int)e, 0});
            } else {
                // ---- slopes / other: one polygon per tile ----
                auto s = groundShape(t, d, e);
                if (!s.empty()) R.fixtures.push_back({std::move(s)});
                R.draws.push_back({(int)d, (int)e, drawType(t)});
            }
        }
    }
    return R;
}

} // namespace fw
