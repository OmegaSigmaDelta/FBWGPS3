#include "level.hpp"
#include <algorithm>

namespace fw {

namespace {
struct Tileset {
    int firstgid = 0;
    std::string name;        // "Ground", "Chars", "Objects", "LargeObjects"
    Json def;                // external tileset json (tiles + tileproperties)
};

std::string baseName(const std::string& p) {
    size_t s = p.find_last_of("/\\"); std::string f = (s == std::string::npos) ? p : p.substr(s + 1);
    size_t d = f.rfind(".json"); return d == std::string::npos ? f : f.substr(0, d);
}

// Fill in tileset tile properties as DEFAULTS: keys already set on the Tiled object win.
// (Verified against the running original on all 32 levels: e.g. wind `length: 15` on the object beats the tile's 10,
//  lever `group: 4` beats the tile's 0.)
void fillDefaults(Json& dst, const Json& defaults) {
    if (!defaults.isObj()) return;
    if (!dst.isObj()) { dst = Json(); dst.type = Json::Obj; }
    for (auto& kv : defaults.o) {
        bool present = false;
        for (auto& d : dst.o) if (d.first == kv.first) { present = true; break; }
        if (!present) dst.o.push_back(kv);
    }
}

const Tileset* findTileset(const std::vector<Tileset>& ts, int gid, int& localIdx) {
    const Tileset* best = nullptr;
    for (auto& t : ts) if (t.firstgid <= gid && (!best || t.firstgid > best->firstgid)) best = &t;
    if (best) localIdx = gid - best->firstgid;
    return best;
}

void readObjects(const Json& layer, const std::vector<Tileset>& ts, std::vector<MapObject>& out) {
    for (auto& jo : layer["objects"].a) {
        MapObject o;
        o.layer = layer["name"].str();
        o.id = jo["id"].i32(); o.gid = jo["gid"].i32(0);
        o.type = jo["type"].str(); o.name = jo["name"].str();
        o.x = jo["x"].num(); o.y = jo["y"].num(); o.w = jo["width"].num(); o.h = jo["height"].num();
        o.rotation = jo["rotation"].num(); o.visible = jo["visible"].boolean(true);
        o.props = jo["properties"];
        for (const Json& point : jo["polyline"].a)
            o.polyline.push_back({point["x"].num(), point["y"].num()});
        for (const Json& point : jo["polygon"].a)
            o.polygon.push_back({point["x"].num(), point["y"].num()});
        if (o.gid > 0) {
            int idx = 0; const Tileset* t = findTileset(ts, o.gid, idx);
            if (t && t->name != "Ground") {
                std::string key = std::to_string(idx);
                const Json& tiles = t->def["tiles"];
                if (tiles.has(key) && tiles[key].has("type")) o.type = tiles[key]["type"].str();   // tile type overrides ""
                fillDefaults(o.props, t->def["tileproperties"][key]);
            }
        }
        // Constructor defaults of gid-less devices (MovingPlatform: properties = {group:0, dx:4, dy:0}).
        // Other device classes read their properties with local fallbacks, see PORTING.md "device defaults".
        if (o.type == "platform") fillDefaults(o.props, Json::parse("{\"group\":0,\"dx\":4,\"dy\":0}"));
        out.push_back(std::move(o));
    }
}
} // namespace

Level loadLevel(const std::string& gameDir, const std::string& rel) {
    Level L; L.file = rel;
    Json m = loadJson(gameDir + "/data/" + rel);
    L.w = m["width"].i32(); L.h = m["height"].i32();
    L.tileW = m["tilewidth"].i32(32); L.tileH = m["tileheight"].i32(32);

    std::vector<Tileset> ts;
    for (auto& jt : m["tilesets"].a) {
        Tileset t; t.firstgid = jt["firstgid"].i32();
        t.name = baseName(jt["source"].str());
        t.def = loadJson(gameDir + "/assets/tilemaps/tilesets/" + t.name + ".json");
        ts.push_back(std::move(t));
    }

    L.gid.assign((size_t)L.w * L.h, 0);
    L.type.assign((size_t)L.w * L.h, Level::NONE);
    for (auto& layer : m["layers"].a) {
        const std::string kind = layer["type"].str(), name = layer["name"].str();
        if (kind == "tilelayer" && name == "Ground") {
            for (int i = 0; i < L.w * L.h && (size_t)i < layer["data"].size(); ++i) {
                int g = layer["data"][i].i32(); L.gid[i] = g;
                if (g > 0) {
                    int idx = 0; const Tileset* t = findTileset(ts, g, idx);
                    L.type[i] = (t ? t->def["tileproperties"][std::to_string(idx)]["type"].i32(0) : 0);
                }
            }
        } else if (kind == "objectgroup") {
            if (name == "Objects") readObjects(layer, ts, L.objects);
            else if (name == "Chars") readObjects(layer, ts, L.chars);
            else readObjects(layer, ts, L.tutorial);
        }
    }
    return L;
}

} // namespace fw
