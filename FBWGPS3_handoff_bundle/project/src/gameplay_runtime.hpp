#pragma once

// Engine-neutral gameplay event/state layer.
// Physics backends (Box2D desktop / PS3 adapter) report sensor contacts here;
// this file owns the original game's character/liquid/collectible/door rules.

#include "ground.hpp"
#include "level.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fbw {

enum class LevelOutcome {
	Running,
	Won,
	Lost
};

struct WorldPoint {
	double x = 0.0;
	double y = 0.0;
};

struct CharacterRuntime {
	std::string id; // "fb" or "wg"
	int map_object_id = 0;
	WorldPoint spawn_px;
	int diamonds = 0;
	int silver_diamonds = 0;
	bool dying = false;
	bool dead = false;
	bool on_ice = false;
	bool on_liquid = false;
	std::set<int> liquid_contacts;
	std::set<int> ice_contacts;
};

struct DiamondRuntime {
	int map_object_id = 0;
	std::string required_character; // fb, wg, fbwg, or silver
	WorldPoint sensor_center_px;
	bool collected = false;
};

struct DoorRuntime {
	int map_object_id = 0;
	std::string required_character;
	WorldPoint sensor_center_px;
	bool occupied = false;
	double animation_frame = 0.0;
	int displayed_frame = 0;
	bool is_up = false;
};

struct LiquidRuntime {
	int source_type = 0; // 55 is the Tiled "initially frozen water" value
	int type = 0;        // effective device type; 55 is converted to 5 in the source
	int tile_x = 0;
	int tile_y = 0;
	int width_tiles = 0;
	bool frozen = false;
};

class GameplayRuntime {
public:
	bool initialize(const fw::Level& level, const fw::GroundResult& ground,
		std::string* error = nullptr) {
		characters_.clear();
		diamonds_.clear();
		doors_.clear();
		liquids_.clear();
		total_diamonds_ = 0;
		outcome_ = LevelOutcome::Running;

		for (const fw::MapObject& object : level.chars) {
			if (!object.visible) continue;
			if (object.type == "char") {
				const std::string id = object.props["char"].str("fb");
				if (id != "fb" && id != "wg")
					return fail(error, "character object " + std::to_string(object.id) + " has unsupported char property");
				if (characters_.count(id) != 0)
					return fail(error, "duplicate character spawn for " + id);
				CharacterRuntime character;
				character.id = id;
				character.map_object_id = object.id;
				// Tiled tile objects are bottom-left anchored; the original character
				// constructor moves the body origin by (+1m, -1m) in screen pixels.
				character.spawn_px = {object.x + 32.0, object.y - 32.0};
				characters_.emplace(id, std::move(character));
			} else if (object.type == "door") {
				const std::string id = object.props["char"].str("fb");
				if (id != "fb" && id != "wg")
					return fail(error, "door object " + std::to_string(object.id) + " has unsupported char property");
				if (doorForCharacter(id) != nullptr)
					return fail(error, "duplicate door for " + id);
				DoorRuntime door;
				door.map_object_id = object.id;
				door.required_character = id;
				// Door sensor is a 64 x 8 px box at the source sprite position.
				door.sensor_center_px = {object.x + 32.0, object.y + 4.0};
				doors_.push_back(std::move(door));
			} else if (object.type == "diamond") {
				const std::string required = object.props["char"].str("fb");
				if (required != "fb" && required != "wg" && required != "fbwg" && required != "silver")
					return fail(error, "diamond object " + std::to_string(object.id) + " has unsupported char property");
				DiamondRuntime diamond;
				diamond.map_object_id = object.id;
				diamond.required_character = required;
				diamond.sensor_center_px = {object.x + 32.0, object.y - 32.0};
				diamonds_.push_back(std::move(diamond));
				if (required != "silver") ++total_diamonds_;
			}
		}

		if (characters_.count("fb") == 0 || characters_.count("wg") == 0)
			return fail(error, "level must contain one Fireboy and one Watergirl spawn");
		if (doorForCharacter("fb") == nullptr || doorForCharacter("wg") == nullptr)
			return fail(error, "level must contain one Fireboy door and one Watergirl door");

		for (const fw::LiquidDef& liquid : ground.liquids) {
			LiquidRuntime runtime;
			runtime.source_type = liquid.type;
			runtime.type = liquid.type == 55 ? 5 : liquid.type;
			runtime.tile_x = liquid.tx;
			runtime.tile_y = liquid.ty;
			runtime.width_tiles = liquid.width;
			runtime.frozen = liquid.type == 55;
			if (runtime.type != 5 && runtime.type != 6 && runtime.type != 7)
				return fail(error, "unsupported liquid type " + std::to_string(liquid.type));
			liquids_.push_back(runtime);
		}

		if (error) error->clear();
		return true;
	}

	const std::map<std::string, CharacterRuntime>& characters() const { return characters_; }
	const std::vector<DiamondRuntime>& diamonds() const { return diamonds_; }
	const std::vector<DoorRuntime>& doors() const { return doors_; }
	const std::vector<LiquidRuntime>& liquids() const { return liquids_; }
	int totalDiamonds() const { return total_diamonds_; }
	LevelOutcome outcome() const { return outcome_; }

	const CharacterRuntime* character(const std::string& id) const {
		auto it = characters_.find(id);
		return it == characters_.end() ? nullptr : &it->second;
	}
	CharacterRuntime* character(const std::string& id) {
		auto it = characters_.find(id);
		return it == characters_.end() ? nullptr : &it->second;
	}

	// Called by the physics adapter for the character feet-sensor against a
	// Liquid.killFixture. The original game kills FB in water, WG in fire, and
	// either character in green liquid (source type 7).
	bool contactLiquidDeath(const std::string& character_id, int liquid_index,
		bool began, std::string* error = nullptr) {
		CharacterRuntime* player = character(character_id);
		if (!player) return fail(error, "unknown character: " + character_id);
		if (liquid_index < 0 || static_cast<size_t>(liquid_index) >= liquids_.size())
			return fail(error, "liquid index is out of range");
		if (began) {
			const bool inserted = player->liquid_contacts.insert(liquid_index).second;
			if (inserted && !player->dying && !player->dead) {
				const int type = liquids_[static_cast<size_t>(liquid_index)].type;
				if (type == 7 || (type == 6 && character_id == "wg") ||
					(type == 5 && character_id == "fb")) {
					player->dying = true;
				}
			}
		} else {
			player->liquid_contacts.erase(liquid_index);
		}
		player->on_liquid = !player->liquid_contacts.empty();
		if (error) error->clear();
		return true;
	}

	// Ice contact is separate from the kill sensor. A real backend reports it
	// for the frozen surface fixture; duplicate begin/end notifications are safe.
	bool contactIceSurface(const std::string& character_id, int liquid_index,
		bool began, std::string* error = nullptr) {
		CharacterRuntime* player = character(character_id);
		if (!player) return fail(error, "unknown character: " + character_id);
		if (liquid_index < 0 || static_cast<size_t>(liquid_index) >= liquids_.size())
			return fail(error, "liquid index is out of range");
		if (began && !liquids_[static_cast<size_t>(liquid_index)].frozen)
			return fail(error, "ice contact reported for a liquid that is not frozen");
		if (began) player->ice_contacts.insert(liquid_index);
		else player->ice_contacts.erase(liquid_index);
		player->on_ice = !player->ice_contacts.empty();
		if (error) error->clear();
		return true;
	}

	bool setLiquidFrozen(int liquid_index, bool frozen, std::string* error = nullptr) {
		if (liquid_index < 0 || static_cast<size_t>(liquid_index) >= liquids_.size())
			return fail(error, "liquid index is out of range");
		LiquidRuntime& liquid = liquids_[static_cast<size_t>(liquid_index)];
		if (liquid.type != 5) return fail(error, "only water can be frozen or melted");
		liquid.frozen = frozen;
		if (error) error->clear();
		return true;
	}

	// The source Diamond fixture is a sensor and awards one collectible only
	// when the matching character touches it. "fbwg" accepts either character;
	// "silver" increments the separate puzzle counter.
	bool contactDiamond(int object_id, const std::string& character_id,
		bool began, bool* collected = nullptr, std::string* error = nullptr) {
		if (collected) *collected = false;
		CharacterRuntime* player = character(character_id);
		if (!player) return fail(error, "unknown character: " + character_id);
		if (!began) { if (error) error->clear(); return true; }
		for (DiamondRuntime& diamond : diamonds_) {
			if (diamond.map_object_id != object_id) continue;
			const bool allowed = diamond.required_character == "silver" ||
				diamond.required_character == "fbwg" || diamond.required_character == character_id;
			if (allowed && !diamond.collected) {
				diamond.collected = true;
				if (diamond.required_character == "silver") ++player->silver_diamonds;
				else ++player->diamonds;
				if (collected) *collected = true;
			}
			if (error) error->clear();
			return true;
		}
		return fail(error, "unknown diamond object id: " + std::to_string(object_id));
	}

	// The door sensor opens only for the matching character. Animation uses
	// the original 25 fps rate and clamps elapsed time to 40 ms per update.
	bool contactDoor(int object_id, const std::string& character_id,
		bool began, std::string* error = nullptr) {
		if (!character(character_id)) return fail(error, "unknown character: " + character_id);
		for (DoorRuntime& door : doors_) {
			if (door.map_object_id != object_id) continue;
			if (door.required_character == character_id) door.occupied = began;
			if (error) error->clear();
			return true;
		}
		return fail(error, "unknown door object id: " + std::to_string(object_id));
	}

	void finishDeathAnimation(const std::string& character_id) {
		CharacterRuntime* player = character(character_id);
		if (player && player->dying) player->dead = true;
		refreshOutcome();
	}

	void update(double delta_seconds) {
		if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) return;
		const double step = std::min(delta_seconds, 0.04);
		for (DoorRuntime& door : doors_) {
			if (door.occupied) door.animation_frame = std::min(21.0, door.animation_frame + 25.0 * step);
			else door.animation_frame = std::max(0.0, door.animation_frame - 25.0 * step);
			door.displayed_frame = std::max(0, std::min(21, static_cast<int>(std::floor(door.animation_frame + 0.5))));
			door.is_up = door.animation_frame >= 21.0;
		}
		refreshOutcome();
	}

private:
	std::map<std::string, CharacterRuntime> characters_;
	std::vector<DiamondRuntime> diamonds_;
	std::vector<DoorRuntime> doors_;
	std::vector<LiquidRuntime> liquids_;
	int total_diamonds_ = 0;
	LevelOutcome outcome_ = LevelOutcome::Running;

	static bool fail(std::string* error, const std::string& message) {
		if (error) *error = message;
		return false;
	}
	DoorRuntime* doorForCharacter(const std::string& id) {
		for (DoorRuntime& door : doors_) if (door.required_character == id) return &door;
		return nullptr;
	}
	void refreshOutcome() {
		if (outcome_ != LevelOutcome::Running) return;
		const CharacterRuntime* fb = character("fb");
		const CharacterRuntime* wg = character("wg");
		if ((fb && fb->dead) || (wg && wg->dead)) {
			outcome_ = LevelOutcome::Lost;
			return;
		}
		bool fb_door_up = false, wg_door_up = false;
		for (const DoorRuntime& door : doors_) {
			if (door.required_character == "fb") fb_door_up = door.is_up;
			if (door.required_character == "wg") wg_door_up = door.is_up;
		}
		if (fb_door_up && wg_door_up) outcome_ = LevelOutcome::Won;
	}
};

} // namespace fbw
