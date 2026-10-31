#include "gameplay_runtime.hpp"
#include "contact_router.hpp"

#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
	if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

fw::MapObject makeObject(int id, const std::string& type, const std::string& character,
	double x = 32.0, double y = 64.0) {
	fw::MapObject object;
	object.id = id;
	object.type = type;
	object.x = x;
	object.y = y;
	object.w = 64.0;
	object.h = 64.0;
	object.props = fw::Json::parse("{\"char\":\"" + character + "\"}");
	return object;
}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: test_gameplay_runtime <game-dir>\n";
		return 2;
	}
	std::string error;
	fw::Level level;
	try {
		level = fw::loadLevel(argv[1], "tutorials/levels/forest_01.json");
	} catch (const std::exception& exception) {
		std::cerr << "could not load tutorial map: " << exception.what() << '\n';
		return 2;
	}
	fw::GroundResult ground = fw::buildGround(level);

	// Synthetic sensor objects are appended to the authentic tutorial map so
	// tests exercise the parser-derived spawn/door catalogue and source rules.
	level.chars.push_back(makeObject(9001, "diamond", "fb"));
	level.chars.push_back(makeObject(9002, "diamond", "wg"));
	level.chars.push_back(makeObject(9003, "diamond", "fbwg"));
	level.chars.push_back(makeObject(9004, "diamond", "silver"));
	ground.liquids.push_back({5, 1, 2, 3});
	ground.liquids.push_back({6, 4, 2, 3});
	ground.liquids.push_back({7, 7, 2, 3});
	ground.liquids.push_back({55, 10, 2, 3});

	fbw::GameplayRuntime game;
	check(game.initialize(level, ground, &error), "initialize gameplay runtime: " + error);
	check(game.characters().size() == 2, "two player spawns are required");
	check(game.character("fb") != nullptr && game.character("wg") != nullptr, "Fireboy and Watergirl states exist");
	check(game.doors().size() == 2, "two personal doors loaded");
	check(game.totalDiamonds() >= 3, "normal diamond total excludes silver diamond");
	check(game.liquids().size() == ground.liquids.size(), "all liquid fixtures converted to runtime state");
	check(game.liquids().back().type == 5 && game.liquids().back().frozen,
		"tile type 55 becomes initially frozen water");

	// Ownership and one-time collection rules.
	bool collected = false;
	check(game.contactDiamond(9001, "wg", true, &collected, &error), "mismatched diamond contact is valid");
	check(!collected && game.character("wg")->diamonds == 0, "Watergirl cannot collect Fireboy-only diamond");
	check(game.contactDiamond(9001, "fb", true, &collected, &error) && collected,
		"Fireboy collects Fireboy diamond");
	check(game.contactDiamond(9001, "fb", true, &collected, &error) && !collected,
		"diamond cannot be collected twice");
	check(game.contactDiamond(9002, "wg", true, &collected, &error) && collected,
		"Watergirl collects Watergirl diamond");
	check(game.contactDiamond(9003, "fb", true, &collected, &error) && collected,
		"fbwg diamond accepts Fireboy");
	check(game.contactDiamond(9004, "wg", true, &collected, &error) && collected,
		"silver diamond accepts Watergirl");
	check(game.character("fb")->diamonds == 2, "Fireboy normal diamond counter increments");
	check(game.character("wg")->diamonds == 1 && game.character("wg")->silver_diamonds == 1,
		"Watergirl normal and silver counters increment independently");

	// Liquid semantics from Character.liquidTouched in init.js.
	const int water = static_cast<int>(game.liquids().size()) - 4;
	const int fire = water + 1;
	const int green = water + 2;
	const int frozen_water = water + 3;
	check(game.contactLiquidDeath("wg", water, true, &error), "Watergirl enters water sensor");
	check(!game.character("wg")->dying && game.character("wg")->on_liquid,
		"water is safe for Watergirl and marks liquid contact");
	check(game.contactLiquidDeath("wg", water, false, &error), "Watergirl exits water sensor");
	check(!game.character("wg")->on_liquid, "liquid contact ends cleanly");
	check(game.contactLiquidDeath("fb", water, true, &error), "Fireboy enters water sensor");
	check(game.character("fb")->dying, "water kills Fireboy");

	// Use an isolated runtime for each hazard class: once a character is dying,
	// later safe contacts must not be interpreted as a fresh survival test.
	fbw::GameplayRuntime fire_game;
	check(fire_game.initialize(level, ground, &error), "create isolated fire hazard runtime");
	check(fire_game.contactLiquidDeath("fb", fire, true, &error), "Fireboy enters fire sensor");
	check(!fire_game.character("fb")->dying, "fire is safe for Fireboy");
	check(fire_game.contactLiquidDeath("wg", fire, true, &error), "Watergirl enters fire sensor");
	check(fire_game.character("wg")->dying, "fire kills Watergirl");

	fbw::GameplayRuntime green_game;
	check(green_game.initialize(level, ground, &error), "create isolated green hazard runtime");
	check(green_game.contactLiquidDeath("fb", green, true, &error), "Fireboy enters green sensor");
	check(green_game.contactLiquidDeath("wg", green, true, &error), "Watergirl enters green sensor");
	check(green_game.character("fb")->dying && green_game.character("wg")->dying,
		"green liquid kills both characters");

	check(game.contactIceSurface("fb", frozen_water, true, &error), "frozen water creates ice contact");
	check(game.character("fb")->on_ice, "ice contact enables character ice state");
	check(game.contactIceSurface("fb", frozen_water, false, &error), "ice contact can end");
	check(!game.character("fb")->on_ice, "ice state clears after final ice contact");
	check(game.setLiquidFrozen(frozen_water, false, &error), "initially frozen water can melt");
	check(!game.liquids()[static_cast<size_t>(frozen_water)].frozen, "melt state stored");
	check(!game.setLiquidFrozen(fire, true, &error), "fire cannot be frozen by water-only mechanism");

	// Door sensor ownership, source animation cadence, and level completion.
	fbw::GameplayRuntime doors_game;
	fw::GroundResult no_liquids;
	check(doors_game.initialize(level, no_liquids, &error), "create clean session for door flow");
	int fb_door = -1, wg_door = -1;
	for (const auto& door : doors_game.doors()) {
		if (door.required_character == "fb") fb_door = door.map_object_id;
		if (door.required_character == "wg") wg_door = door.map_object_id;
	}
	check(fb_door >= 0 && wg_door >= 0, "both door IDs found");
	check(doors_game.contactDoor(fb_door, "wg", true, &error), "other player can contact sensor without opening it");
	check(!doors_game.doors()[fb_door == doors_game.doors()[0].map_object_id ? 0 : 1].occupied,
		"wrong character does not open a door");
	check(doors_game.contactDoor(fb_door, "fb", true, &error), "matching Fireboy door contact");
	for (int i = 0; i < 21; ++i) doors_game.update(0.04);
	bool fb_up = false;
	for (const auto& door : doors_game.doors()) if (door.required_character == "fb") fb_up = door.is_up;
	check(fb_up, "door opens after 21 frames at 25 fps");
	check(doors_game.contactDoor(wg_door, "wg", true, &error), "matching Watergirl door contact");
	for (int i = 0; i < 21; ++i) doors_game.update(0.04);
	check(doors_game.outcome() == fbw::LevelOutcome::Won, "both doors fully open completes the level");

	fbw::GameplayRuntime lost_game;
	check(lost_game.initialize(level, ground, &error), "create clean session for death flow");
	check(lost_game.contactLiquidDeath("fb", water, true, &error), "Fireboy death trigger in isolated session");
	check(lost_game.outcome() == fbw::LevelOutcome::Running, "dying animation does not immediately end level");
	lost_game.finishDeathAnimation("fb");
	check(lost_game.outcome() == fbw::LevelOutcome::Lost, "level fails when death animation completes");

	// Backend callback adapter: route typed fixture events through one stable API.
	fbw::ContactRouter router(game);
	auto routed_begin = router.dispatch({fbw::ContactKind::LiquidDeath, "wg", water, true});
	check(routed_begin.handled && routed_begin.error.empty(), "contact router dispatches liquid begin");
	auto routed_end = router.dispatch({fbw::ContactKind::LiquidDeath, "wg", water, false});
	check(routed_end.handled && !game.character("wg")->on_liquid,
		"contact router dispatches liquid end and clears aggregate contact state");
	auto invalid_event = router.dispatch({fbw::ContactKind::Door, "fb", 999999, true});
	check(!invalid_event.handled && !invalid_event.error.empty(),
		"contact router returns backend-safe errors for unknown targets");

	if (failures) {
		std::cerr << failures << " gameplay runtime checks failed\n";
		return 1;
	}
	std::cout << "Gameplay runtime PASS: players=2 doors=2 diamonds=" << game.diamonds().size()
		<< " liquids=" << game.liquids().size() << " hazards=water/fire/green ice=on/off outcome=win/loss\n";
	return 0;
}
