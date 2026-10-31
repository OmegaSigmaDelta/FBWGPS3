// Prints the extracted terrain as JSON so it can be diffed against the original game's real Box2D fixtures.
// usage: dump_ground <gameDir> <levelRel>     e.g.  dump_ground ../game forest/levels/forest_02.json
#include "../src/ground.hpp"
#include <cstdio>
using namespace fw;
static void ser(const Json& j) {   // tiny serializer for property dumps
    switch (j.type) {
        case Json::Null: printf("null"); break;
        case Json::Bool: printf(j.b ? "true" : "false"); break;
        case Json::Num:  printf("%g", j.n); break;
        case Json::Str:  printf("\"%s\"", j.s.c_str()); break;
        case Json::Arr:  printf("["); for (size_t i = 0; i < j.a.size(); ++i) { if (i) printf(","); ser(j.a[i]); } printf("]"); break;
        case Json::Obj:  printf("{"); for (size_t i = 0; i < j.o.size(); ++i) { if (i) printf(","); printf("\"%s\":", j.o[i].first.c_str()); ser(j.o[i].second); } printf("}"); break;
    }
}
int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: dump_ground <gameDir> <levelRel>\n"); return 2; }
    try {
        Level L = loadLevel(argv[1], argv[2]);
        GroundResult G = buildGround(L);
        printf("{\"w\":%d,\"h\":%d,\"fixtures\":[", L.w, L.h);
        for (size_t i = 0; i < G.fixtures.size(); ++i) {
            printf("%s[", i ? "," : "");
            for (size_t k = 0; k < G.fixtures[i].v.size(); ++k)
                printf("%s[%.3f,%.3f]", k ? "," : "", G.fixtures[i].v[k].x, G.fixtures[i].v[k].y);
            printf("]");
        }
        printf("],\"liquids\":[");
        for (size_t i = 0; i < G.liquids.size(); ++i)
            printf("%s{\"type\":%d,\"x\":%d,\"y\":%d,\"width\":%d}", i ? "," : "", G.liquids[i].type, G.liquids[i].tx, G.liquids[i].ty, G.liquids[i].width);
        printf("],\"objects\":[");
        auto dump = [&](const std::vector<MapObject>& v, bool& first) {
            for (auto& o : v) {
                printf("%s{\"layer\":\"%s\",\"gid\":%d,\"type\":\"%s\",\"x\":%g,\"y\":%g,\"w\":%g,\"h\":%g,\"props\":", first ? "" : ",", o.layer.c_str(), o.gid, o.type.c_str(), o.x, o.y, o.w, o.h);
                if (o.props.isObj()) ser(o.props); else printf("null");
                printf("}");
                first = false;
            }
        };
        bool first = true; dump(L.objects, first); dump(L.chars, first);
        printf("]}\n");
    } catch (std::exception& e) { fprintf(stderr, "error: %s\n", e.what()); return 1; }
    return 0;
}
