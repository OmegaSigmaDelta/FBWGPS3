#pragma once

#include "character_animation.hpp"
#include "level.hpp"
#include "render_queue.hpp"

#include <cmath>
#include <string>

namespace fbw {

struct WorldRenderStats {
	size_t characters = 0;
	size_t doors = 0;
	size_t diamonds = 0;
	size_t devices = 0;
	size_t unsupported = 0;
};

// Creates a static, engine-neutral visual pass for Tiled Chars and Objects.
// Animated state, Box2D transforms and light-beam compositing are separate tasks.
inline bool appendWorldObjects(const fw::Level& level,
	const Atlas& char_atlas, const Atlas& mech_atlas, const Atlas& temple_atlas,
	RenderQueue& queue, WorldRenderStats* stats = nullptr,
	std::string* error = nullptr) {
	WorldRenderStats local;
	auto add = [&](const Atlas& atlas, const std::string& key, const std::string& frame,
		double x, double y, double scale, RenderLayer layer,
		double rotation = 0.0, bool flip_x = false) {
		return queue.appendFrame(atlas, key, frame, x, y, scale, layer, error,
			rotation, flip_x, false);
	};
	auto add_rect = [&](const Atlas& atlas, const std::string& key, const std::string& frame,
		double x, double y, double width, double height, RenderLayer layer,
		double rotation = 0.0) {
		return queue.appendFrameRect(atlas, key, frame, x, y, width, height,
			layer, error, rotation);
	};
	auto point = [](const fw::MapObject& object, size_t index) {
		return fw::MapPoint{object.x + object.polyline[index].x,
			object.y + object.polyline[index].y};
	};
	auto add_rope = [&](const fw::MapPoint& a, const fw::MapPoint& b) {
		const double dx = b.x - a.x;
		const double dy = b.y - a.y;
		const double length = std::hypot(dx, dy);
		if (length < 1.0) return true;
		const double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
		return add_rect(mech_atlas, "MechAssets", "pulley_rope0000",
			(a.x + b.x - length) * 0.5, (a.y + b.y - 20.0) * 0.5,
			length, 20.0, RenderLayer::Devices, angle);
	};

	// Characters, doors and collectible gems are defined in Tiled's Chars layer.
	for (const fw::MapObject& object : level.chars) {
		if (!object.visible) continue;
		const std::string character = object.props["char"].str("fb");
		const double center_x = object.x + 32.0;
		const double center_y = object.y - 32.0;
		if (object.type == "char") {
			CharacterMovementSample initial_pose;
			initial_pose.grounded = true;
			const CharacterPose pose = resolveCharacterPose(initial_pose);
			if (!appendCharacterAnimation(queue, char_atlas, "CharAssets", character,
				center_x, center_y, 0.95, pose, 0.0, error)) return false;
			++local.characters;
		} else if (object.type == "door") {
			const std::string frame = character == "wg" ? "FinishGirl0000" : "FinishBoy0000";
			const AtlasFrame* door = temple_atlas.find(frame);
			if (!door) {
				if (error) *error = "missing temple door frame: " + frame;
				return false;
			}
			// Phaser's door sprite uses anchor (0, 0.99), scale 0.8.
			const double scale = 0.8;
			const double left = object.x + 32.0;
			const double top = object.y + 4.0 - door->original_h * 0.99 * scale;
			if (!add_rect(temple_atlas, "TempleAssets", frame, left, top,
				door->original_w * scale, door->original_h * scale,
				RenderLayer::Background)) return false;
			++local.doors;
		} else if (object.type == "diamond") {
			const std::string frame = "diamond_" + character + "0000";
			if (!add(char_atlas, "CharAssets", frame, center_x, center_y - 5.0,
				1.0, RenderLayer::Devices)) return false;
			++local.diamonds;
		}
	}

	// Tile-based mechanical devices. The positions below reproduce the Tiled
	// 64x64 tile-object anchor and BoxedDevice's standard 2m x 2m centre offset.
	for (const fw::MapObject& object : level.objects) {
		if (!object.visible) continue;
		const double cx = object.x + 32.0;
		const double cy = object.y - 32.0;
		const double rotation = object.rotation;
		bool handled = true;
		if (object.type == "pusher") {
			if (!add(mech_atlas, "MechAssets", "pusher_block0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "pusher_block_light0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "lever") {
			if (!add(mech_atlas, "MechAssets", "lever_base0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "lever_base_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "lever_stick0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "lever_stick_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "box") {
			if (!add(mech_atlas, "MechAssets", "movingbox0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "boxheavy") {
			if (!add(mech_atlas, "MechAssets", "movingbox20000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "boxmirror") {
			if (!add(mech_atlas, "MechAssets", "BoxMirror0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "ball") {
			if (!add(mech_atlas, "MechAssets", "ball0000", object.x + 16.0, object.y - 11.2,
				1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "beamer") {
			if (!add(mech_atlas, "MechAssets", "beamer0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "beamer_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "infinitemirror") {
			if (!add(mech_atlas, "MechAssets", "rotmirror_circle0000", cx, cy, 1.0, RenderLayer::Devices)) return false;
			if (!add(mech_atlas, "MechAssets", "infinitemirror0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "infinitemirror_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "rotmirror") {
			if (!add(mech_atlas, "MechAssets", "rotmirror_circle0000", cx, cy, 1.0, RenderLayer::Devices)) return false;
			if (!add(mech_atlas, "MechAssets", "rotmirror0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "rotmirror_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "lightpusher") {
			if (!add(mech_atlas, "MechAssets", "lightpusher_base0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "lightpusher_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "timedpusher") {
			if (!add(mech_atlas, "MechAssets", "timed_pusher_base0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "timed_pusher_light_off0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
		} else if (object.type == "wind") {
			const double wx = object.x - 48.0;
			const double wy = object.y - 32.0;
			if (!add(mech_atlas, "MechAssets", "wind_base0000", wx, wy, 1.0,
				RenderLayer::Liquids, rotation + 90.0)) return false;
			if (!add(mech_atlas, "MechAssets", "wind_base_light_off0000", wx, wy, 1.0,
				RenderLayer::Liquids, rotation + 90.0)) return false;
		} else if (object.type == "portal") {
			if (!add(mech_atlas, "MechAssets", "portal0000", cx, cy, 1.0, RenderLayer::Devices, rotation)) return false;
			if (!add(mech_atlas, "MechAssets", "portal_center0000", cx, cy, 1.0, RenderLayer::Devices)) return false;
			if (!add(mech_atlas, "MechAssets", "portal_light_off0000", cx, cy, 1.0, RenderLayer::Devices)) return false;
		} else if (object.type == "platform") {
			double width = object.w;
			double height = object.h;
			double px = object.x;
			double py = object.y;
			double angle = rotation;
			if (width < height) {
				std::swap(width, height);
				px = object.x + object.h * 0.5;
				py = object.y + object.w * 0.5;
				angle -= 90.0;
			} else {
				px += width * 0.5;
				py += height * 0.5;
			}
			const double bar_y = py - 16.0;
			if (width > 32.0 && !add_rect(mech_atlas, "MechAssets", "BarCenter0000",
				px - width * 0.5 + 16.0, bar_y - 16.0, width - 32.0, 32.0,
				RenderLayer::Devices, angle)) return false;
			if (!add(mech_atlas, "MechAssets", "BarCapLeft0000", px + width * 0.5 - 16.0,
				bar_y, 1.0, RenderLayer::Devices, angle)) return false;
			if (!add(mech_atlas, "MechAssets", "BarCapRight0000", px - width * 0.5 + 16.0,
				bar_y, 1.0, RenderLayer::Devices, angle)) return false;
			if (!add_rect(mech_atlas, "MechAssets", "moving_platform_light_off0000",
				px - width * 0.5, py - 32.0, width, 32.0, RenderLayer::Devices, angle)) return false;
		} else if (object.type == "hanging") {
			if (object.polyline.size() < 2) {
				if (error) *error = "hanging device has no usable polyline";
				return false;
			}
			const fw::MapPoint anchor = point(object, 0);
			const fw::MapPoint platform = point(object, 1);
			if (!add_rope(anchor, platform)) return false;
			if (!add(mech_atlas, "MechAssets", "hanging_anchor0000", anchor.x, anchor.y,
				1.0, RenderLayer::Devices)) return false;
			if (!add(mech_atlas, "MechAssets", "hanging_base0000", anchor.x, anchor.y,
				1.0, RenderLayer::Devices)) return false;
			if (!add(mech_atlas, "MechAssets", "hanging_platform0000", platform.x, platform.y,
				1.0, RenderLayer::Devices)) return false;
		} else if (object.type == "pulley") {
			if (object.polyline.size() < 4) {
				if (error) *error = "pulley device needs four polyline points";
				return false;
			}
			const fw::MapPoint p0 = point(object, 0);
			const fw::MapPoint p1 = point(object, 1);
			const fw::MapPoint p2 = point(object, 2);
			const fw::MapPoint p3 = point(object, 3);
			if (!add_rope(p0, p1) || !add_rope(p1, p2) || !add_rope(p2, p3)) return false;
			for (const fw::MapPoint& anchor : {p1, p2}) {
				if (!add(mech_atlas, "MechAssets", "pulley_anchor0000", anchor.x, anchor.y,
					1.0, RenderLayer::Devices)) return false;
			}
			for (const fw::MapPoint& platform : {p0, p3}) {
				if (!add(mech_atlas, "MechAssets", "pulley_base0000", platform.x, platform.y,
					1.0, RenderLayer::Devices)) return false;
				if (!add(mech_atlas, "MechAssets", "pulley_platform0000", platform.x, platform.y,
					1.0, RenderLayer::Devices)) return false;
			}
		} else if (object.type == "roman") {
			if (object.polygon.size() < 3) {
				if (error) *error = "roman device has no usable polygon";
				return false;
			}
			const fw::MapPoint a{object.x + object.polygon[0].x, object.y + object.polygon[0].y};
			const fw::MapPoint b{object.x + object.polygon[1].x, object.y + object.polygon[1].y};
			const fw::MapPoint c{object.x + object.polygon[2].x, object.y + object.polygon[2].y};
			const double dx = c.x - b.x;
			const double dy = c.y - b.y;
			const double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
			const double length = std::hypot(dx, dy);
			if (!add(mech_atlas, "MechAssets", "roman_base0000", a.x, a.y,
				1.0, RenderLayer::Devices, rotation)) return false;
			if (!add_rect(mech_atlas, "MechAssets", "roman_bar0000", b.x, b.y - 15.0,
				length, 30.0, RenderLayer::Devices, angle)) return false;
			if (!add(mech_atlas, "MechAssets", "roman_end0000", b.x, b.y,
				1.0, RenderLayer::Devices, angle, true)) return false;
			if (!add(mech_atlas, "MechAssets", "roman_end0000", c.x, c.y,
				1.0, RenderLayer::Devices, angle)) return false;
			if (!add(mech_atlas, "MechAssets", "roman_center0000", (b.x + c.x) * 0.5,
				(b.y + c.y) * 0.5, 1.0, RenderLayer::Devices)) return false;
		} else {
			handled = false;
		}
		if (handled) ++local.devices;
		else ++local.unsupported;
	}

	if (stats) *stats = local;
	if (error) error->clear();
	return true;
}

} // namespace fbw
