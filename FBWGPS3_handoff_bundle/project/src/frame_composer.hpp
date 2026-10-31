// CPU frame composer: turns the live GameWorld into one RGBA frame (platform independent).
//
// Why this exists: the SDL2 runtime in game_main.cpp draws through SDL textures, which a PS3 build does not have.
// This header produces the same picture purely in software using fbw::SoftwareRenderer, so a platform layer only has to
// copy the finished RGBA frame to its display (PS3: RSX framebuffer; Linux test: PNG file).
//
// STATUS: verified on Linux by tests/test_frame_composer.cpp (frame is written out and inspected). NOT yet run on a PS3.
// Performance on the Cell PPU is unmeasured; the per-pixel rotated blit in software_renderer.cpp is not optimised.
#pragma once

#include "atlas.hpp"
#include "character_animation.hpp"
#include "game_world.hpp"
#include "progression_catalog.hpp"
#include "software_renderer.hpp"
#include "world_render.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

namespace fbw {

struct FrameAssets {
	Atlas ground, chars, mech, temple;
	std::map<std::string, ImageRGBA> images;   // decoded atlas PNGs by atlas key
	ImageRGBA level_image;                     // baked background + ground for the current level
};

struct FrameView { int x = 0, y = 0; };       // top-left of the camera in level pixels

inline bool loadFrameAssets(const std::string& game_dir, FrameAssets& a, std::string& error) {
	const std::string dir = game_dir + "/assets/atlasses/";
	if (!a.ground.load(dir + "GroundAssets.json", &error) || !a.chars.load(dir + "CharAssets.json", &error) ||
		!a.mech.load(dir + "MechAssets.json", &error) || !a.temple.load(dir + "Temples/forest/TempleAssets.json", &error)) return false;
	for (const auto& p : std::vector<std::pair<std::string, std::string>>{
		{"GroundAssets", dir + "GroundAssets.png"}, {"CharAssets", dir + "CharAssets.png"},
		{"MechAssets", dir + "MechAssets.png"}, {"TempleAssets", dir + "Temples/forest/TempleAssets.png"}}) {
		ImageRGBA image;
		if (!image.loadPng(p.second, &error)) return false;
		a.images.emplace(p.first, std::move(image));
	}
	return true;
}

// Bakes background + ground mask for a level (same recipe as makeLevelTexture() in game_main.cpp).
inline bool buildLevelImage(const std::string& game_dir, const fw::progression::CatalogLevel& catalog_level,
	FrameAssets& assets, std::string& error) {
	const fw::Level level = fw::loadLevel(game_dir, catalog_level.filename);
	const fw::GroundResult ground = fw::buildGround(level);
	RenderQueue queue;
	if (!queue.appendGround(ground, assets.ground, "GroundAssets", level.tileW, level.tileH, 1234, &error)) return false;
	auto region = [&](const Atlas& atlas, const std::string& key, const std::string& name, TextureRegion& out) {
		const AtlasFrame* f = atlas.find(name);
		if (!f) { error = "missing frame: " + name; return false; }
		out = TextureRegion{key, {f->x, f->y, f->w, f->h}, f->source_w, f->source_h, f->original_w, f->original_h};
		return true;
	};
	TextureRegion bg, fg;
	if (!region(assets.temple, "TempleAssets", "BackGround0000", bg) || !region(assets.temple, "TempleAssets", "ForeGround0000", fg)) return false;
	SoftwareRenderer software;
	return software.renderTexturedLevel(queue, assets.images, bg, fg, level.w * level.tileW, level.h * level.tileH, assets.level_image, &error);
}

// ---- tiny pixel helpers -------------------------------------------------------------------------------------------

inline void fillRect(ImageRGBA& img, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
	const int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(img.width, x + w), y1 = std::min(img.height, y + h);
	for (int py = y0; py < y1; ++py) for (int px = x0; px < x1; ++px) {
		uint8_t* d = &img.pixels[(static_cast<size_t>(py) * img.width + px) * 4];
		const int inv = 255 - a;
		d[0] = static_cast<uint8_t>((r * a + d[0] * inv) / 255); d[1] = static_cast<uint8_t>((g * a + d[1] * inv) / 255);
		d[2] = static_cast<uint8_t>((b * a + d[2] * inv) / 255); d[3] = 255;
	}
}

inline void drawRectOutline(ImageRGBA& img, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
	fillRect(img, x, y, w, 1, r, g, b); fillRect(img, x, y + h - 1, w, 1, r, g, b);
	fillRect(img, x, y, 1, h, r, g, b); fillRect(img, x + w - 1, y, 1, h, r, g, b);
}

// 5x7 bitmap font (the same glyphs the SDL runtime uses until the real BMFont is available).
inline void drawText(ImageRGBA& img, int x, int y, const std::string& text, int scale = 2, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) {
	static const std::map<char, std::array<uint8_t, 7>> glyphs = {
		{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},
		{'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
		{'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},{'I',{31,4,4,4,4,4,31}},
		{'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
		{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},
		{'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
		{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},
		{'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
		{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
		{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},
		{'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
		{'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},
		{'9',{14,17,17,15,1,1,14}},{':',{0,4,4,0,4,4,0}},{'/',{1,2,2,4,8,8,16}},
		{'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,12,12}},{' ',{0,0,0,0,0,0,0}}};
	for (char c : text) {
		const auto it = glyphs.find(c);
		if (it != glyphs.end())
			for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
				if (it->second[row] & (1u << (4 - col))) fillRect(img, x + col * scale, y + row * scale, scale, scale, r, g, b);
		x += 6 * scale;
	}
}

inline std::string frameSuffix(int n) { std::string s = std::to_string(n); while (s.size() < 4) s.insert(s.begin(), '0'); return s; }

// ---- gameplay frame -------------------------------------------------------------------------------------------------

struct GameplayFrameParams {
	FrameView view;
	bool paused = false;
	bool solo_mode = false;
	GameWorldInput last_input;     // used only to pick running/idle character animation
	int level_id = 0;
	std::string level_type;        // "dark" enables the darkness mask
};

// Camera target the same way game_main.cpp does it: midpoint of both heroes, or the active hero in solo mode.
inline FrameView cameraFor(const GameWorld& world, bool solo_mode, const std::string& solo_active,
	int level_w, int level_h, int view_w, int view_h) {
	const PhysicsBody* fb = world.body("fb"); const PhysicsBody* wg = world.body("wg");
	const PhysicsBody* a = solo_mode ? world.body(solo_active) : fb;
	const PhysicsBody* b = solo_mode ? nullptr : wg;
	const double fx = (a && b) ? (a->x + b->x) * 0.5 : (a ? a->x : (fb ? fb->x : (wg ? wg->x : view_w / 2)));
	const double fy = (a && b) ? (a->y + b->y) * 0.5 : (a ? a->y : (fb ? fb->y : (wg ? wg->y : view_h / 2)));
	FrameView v;
	v.x = std::clamp(static_cast<int>(fx - view_w / 2), 0, std::max(0, level_w - view_w));
	v.y = std::clamp(static_cast<int>(fy - view_h / 2), 0, std::max(0, level_h - view_h));
	return v;
}

inline bool composeGameplayFrame(const GameWorld& world, FrameAssets& assets, const GameplayFrameParams& p,
	ImageRGBA& frame, std::string* error = nullptr) {
	if (frame.width <= 0 || frame.height <= 0) { if (error) *error = "frame has no size"; return false; }
	frame.pixels.assign(static_cast<size_t>(frame.width) * frame.height * 4, 0);
	fillRect(frame, 0, 0, frame.width, frame.height, 15, 22, 29);

	// 1. background + ground: copy the camera window out of the baked level image.
	const ImageRGBA& level = assets.level_image;
	const int dst_x0 = std::max(0, (frame.width - level.width) / 2), dst_y0 = std::max(0, (frame.height - level.height) / 2);
	for (int y = 0; y < frame.height; ++y) {
		const int ly = p.view.y + y - dst_y0;
		if (ly < 0 || ly >= level.height) continue;
		for (int x = 0; x < frame.width; ++x) {
			const int lx = p.view.x + x - dst_x0;
			if (lx < 0 || lx >= level.width) continue;
			const uint8_t* s = &level.pixels[(static_cast<size_t>(ly) * level.width + lx) * 4];
			uint8_t* d = &frame.pixels[(static_cast<size_t>(y) * frame.width + x) * 4];
			d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
		}
	}
	const double ox = p.view.x - dst_x0, oy = p.view.y - dst_y0;   // world -> frame translation (subtracted)
	SoftwareRenderer software;
	std::string local_error;
	std::string* err = error ? error : &local_error;

	// 2. devices/objects at their live positions.
	fw::Level visual = world.session().level();
	for (const auto& platform : world.platforms()) for (auto& o : visual.objects) if (o.id == platform.id && o.type == "platform") { o.x = platform.x; o.y = platform.y; }
	for (const auto& box : world.movableObjects()) for (auto& o : visual.objects) if (o.id == box.id && o.type == box.type) {
		if (box.type == "ball") { o.x = box.body.x - kBallSpawnDx; o.y = box.body.y - kBallSpawnDy; o.rotation = box.angle_deg; }
		else { o.x = box.body.x - 32.0; o.y = box.body.y + 32.0; }
	}
	visual.chars.erase(std::remove_if(visual.chars.begin(), visual.chars.end(), [&](const fw::MapObject& o) {
		if (o.type != "diamond") return false;
		for (const auto& d : world.session().gameplay().diamonds()) if (d.map_object_id == o.id && d.collected) return true;
		return false; }), visual.chars.end());
	RenderQueue all_objects, objects;
	WorldRenderStats stats;
	if (!appendWorldObjects(visual, assets.chars, assets.mech, assets.temple, all_objects, &stats, err)) return false;
	for (const RenderCommand& c : all_objects.commands()) {
		if (c.layer == RenderLayer::Characters || c.frame_name.rfind("FinishBoy", 0) == 0 || c.frame_name.rfind("FinishGirl", 0) == 0) continue;
		objects.push(c);
	}
	if (!software.drawQueueOnto(objects, assets.images, frame, ox, oy, err)) return false;

	// 3. doors, wind, liquids (animated, drawn from live state), then heroes.
	RenderQueue dynamic;
	for (const auto& door : world.session().gameplay().doors()) {
		const fw::MapObject* obj = nullptr;
		for (const auto& c : world.session().level().chars) if (c.id == door.map_object_id) { obj = &c; break; }
		if (!obj) continue;
		const std::string name = std::string(door.required_character == "wg" ? "FinishGirl" : "FinishBoy") + frameSuffix(door.displayed_frame);
		const AtlasFrame* f = assets.temple.find(name); if (!f) continue;
		const double scale = 0.8, left = obj->x + 32.0, top = obj->y + 4.0 - f->original_h * 0.99 * scale;
		dynamic.appendFrameRect(assets.temple, "TempleAssets", name, left, top, f->original_w * scale, f->original_h * scale, RenderLayer::Devices);
	}
	for (const fw::MapObject& wind : world.session().level().objects) if (wind.type == "wind") {
		const int group = wind.props["group"].i32(); const bool invert = wind.props["invertInput"].boolean(false);
		if (!(invert ? !world.groupActive(group) : world.groupActive(group))) continue;
		const int frame = static_cast<int>(std::floor(world.session().elapsedSeconds() * 30.0)) % 31;
		const std::string name = "wind_effect" + frameSuffix(frame);
		if (!assets.mech.find(name)) continue;
		const double length = std::max(1.0, wind.props["length"].num(10.0)) * 32.0, width = std::max(1.0, wind.props["windWidth"].num(2.6)) * 32.0;
		const double cx = wind.x + 32.0, cy = wind.y - 32.0 - length * 0.5;
		dynamic.appendFrameRect(assets.mech, "MechAssets", name, cx - width * 0.5, cy - length * 0.5, width, length, RenderLayer::Effects, nullptr, wind.rotation);
	}
	const auto& liquid_defs = world.session().ground().liquids; const auto& liquid_states = world.session().gameplay().liquids();
	const int anim = static_cast<int>(std::floor(world.session().elapsedSeconds() * 30.0)) % 15;
	for (size_t li = 0; li < liquid_defs.size(); ++li) {
		const auto& liquid = liquid_defs[li]; const bool frozen = li < liquid_states.size() && liquid_states[li].frozen;
		const std::string prefix = liquid.type == 5 || liquid.type == 55 ? "WaterBox" : (liquid.type == 6 ? "FireBox" : "GreenBox");
		const std::string base = frozen ? "IceBox" : prefix, suffix = frozen ? "0000" : frameSuffix(anim);
		for (int part = 0; part < liquid.width; ++part) {
			const std::string name = base + (part == 0 ? "Left" : (part == liquid.width - 1 ? "Right" : "")) + suffix;
			dynamic.appendFrame(assets.ground, "GroundAssets", name, (liquid.tx + part + 0.5) * 32.0, (liquid.ty + 0.5) * 32.0 + (frozen ? 4.0 : 2.0), 1.0, RenderLayer::Liquids);
		}
	}
	auto addHero = [&](const char* id, bool left, bool right) {
		const PhysicsBody* b = world.body(id); if (!b) return;
		CharacterMovementSample sample; sample.grounded = b->grounded; sample.left_down = left; sample.right_down = right;
		sample.velocity_x = b->vx; sample.velocity_y = -b->vy;
		const auto* runtime = world.session().gameplay().character(id); sample.on_ice = runtime && runtime->on_ice;
		const auto pose = resolveCharacterPose(sample); CharacterAnimationFrames frames;
		if (!selectCharacterAnimationFrames(assets.chars, id, pose, world.session().elapsedSeconds(), frames, nullptr)) return;
		dynamic.appendFrame(assets.chars, "CharAssets", frames.legs, b->x, b->y + 13, 1.0, RenderLayer::Characters, nullptr, 0.0, pose.legs_flip_x);
		dynamic.appendFrame(assets.chars, "CharAssets", frames.head, b->x, b->y + 5, 1.0, RenderLayer::Characters, nullptr, pose.head_rotation_degrees, pose.head_flip_x);
	};
	addHero("fb", p.last_input.fireboy.left, p.last_input.fireboy.right);
	addHero("wg", p.last_input.watergirl.left, p.last_input.watergirl.right);
	if (!software.drawQueueOnto(dynamic, assets.images, frame, ox, oy, err)) return false;

	// 4. darkness mask for "dark" levels.
	if (p.level_type == "dark") {
		const PhysicsBody* players[2] = {world.body("fb"), world.body("wg")};
		for (int py = 0; py < frame.height; ++py) for (int px = 0; px < frame.width; ++px) {
			double nearest = 10000.0;
			for (const PhysicsBody* b : players) if (b) nearest = std::min(nearest, std::hypot(px - (b->x - ox), py - (b->y - oy)));
			const double t = std::clamp(nearest / 145.0, 0.0, 1.0);
			fillRect(frame, px, py, 1, 1, 0, 0, 0, static_cast<uint8_t>(30.0 + 190.0 * t * t));
		}
	}

	// 5. HUD + pause / result banner.
	fillRect(frame, 0, 0, frame.width, 38, 10, 15, 20, 210);
	int diamonds = 0; for (const auto& d : world.session().gameplay().diamonds()) if (d.collected) ++diamonds;
	std::ostringstream hud;
	hud << "FBWGPS3  LEVEL " << p.level_id << "  TIME " << std::fixed << std::setprecision(1) << world.session().elapsedSeconds()
		<< "  DIAMONDS " << diamonds << "/" << world.session().gameplay().totalDiamonds();
	drawText(frame, 12, 10, hud.str(), 2);
	const auto outcome = world.session().gameplay().outcome();
	if (p.paused || outcome != LevelOutcome::Running) {
		const int bx = frame.width / 2 - 250, by = frame.height / 2 - 70;
		fillRect(frame, bx, by, 500, 140, 0, 0, 0, 220); drawRectOutline(frame, bx, by, 500, 140, 230, 230, 230);
		const int cx = frame.width / 2, cy = frame.height / 2;
		if (p.paused) { drawText(frame, cx - 54, cy - 35, "PAUSED", 3); drawText(frame, cx - 222, cy + 15, "CROSS RESUME  DOWN RETRY  CIRCLE MENU", 2); }
		else if (outcome == LevelOutcome::Won) {
			drawText(frame, cx - 90, cy - 35, "LEVEL CLEAR", 3); drawText(frame, cx - 150, cy + 8, "CROSS NEXT  CIRCLE MENU", 2);
			drawText(frame, cx - 105, cy + 34, "STARS", 2); drawText(frame, cx + 10, cy + 34, std::to_string(world.session().result().stars) + "/3", 2);
		} else { drawText(frame, cx - 45, cy - 35, "GAME OVER", 3); drawText(frame, cx - 150, cy + 15, "CROSS RETRY  CIRCLE MENU", 2); }
	}
	return true;
}

} // namespace fbw
