#pragma once

// Single dispatch point for contacts reported by a future Box2D/PS3 physics
// backend. This deliberately does not guess collision geometry: the backend
// reports fixture contacts, while GameplayRuntime owns game rules.

#include "gameplay_runtime.hpp"

#include <string>

namespace fbw {

enum class ContactKind {
	LiquidDeath,
	IceSurface,
	Diamond,
	Door
};

struct ContactEvent {
	ContactKind kind = ContactKind::LiquidDeath;
	std::string character_id;
	int target_id = -1; // liquid vector index, or Tiled object ID for diamond/door
	bool began = true;
};

struct ContactDispatchResult {
	bool handled = false;
	bool collected_diamond = false;
	std::string error;
};

class ContactRouter {
public:
	explicit ContactRouter(GameplayRuntime& runtime) : runtime_(runtime) {}

	ContactDispatchResult dispatch(const ContactEvent& event) {
		ContactDispatchResult result;
		switch (event.kind) {
		case ContactKind::LiquidDeath:
			result.handled = runtime_.contactLiquidDeath(
				event.character_id, event.target_id, event.began, &result.error);
			break;
		case ContactKind::IceSurface:
			result.handled = runtime_.contactIceSurface(
				event.character_id, event.target_id, event.began, &result.error);
			break;
		case ContactKind::Diamond:
			result.handled = runtime_.contactDiamond(
				event.target_id, event.character_id, event.began,
				&result.collected_diamond, &result.error);
			break;
		case ContactKind::Door:
			result.handled = runtime_.contactDoor(
				event.target_id, event.character_id, event.began, &result.error);
			break;
		}
		return result;
	}

private:
	GameplayRuntime& runtime_;
};

} // namespace fbw
