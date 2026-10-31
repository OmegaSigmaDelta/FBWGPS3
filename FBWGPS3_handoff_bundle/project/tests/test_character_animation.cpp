#include "atlas.hpp"
#include "character_animation.hpp"

#include <cmath>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: test_character_animation <game-root>\n";
		return 2;
	}
	fbw::Atlas atlas;
	std::string error;
	if (!atlas.load(std::string(argv[1]) + "/assets/atlasses/CharAssets.json", &error)) {
		std::cerr << "FAIL: " << error << "\n";
		return 1;
	}

	fbw::CharacterMovementSample movement;
	movement.grounded = true;
	movement.left_down = true;
	const fbw::CharacterPose left_pose = fbw::resolveCharacterPose(movement);
	if (left_pose.motion != fbw::CharacterMotion::Jumping ||
		!left_pose.head_flip_x || !left_pose.legs_flip_x ||
		std::abs(left_pose.head_rotation_degrees) > 0.001) {
		std::cerr << "FAIL: grounded left movement pose mismatch\n";
		return 1;
	}

	movement = {};
	movement.velocity_x = 5.0;
	movement.velocity_y = 2.0;
	const fbw::CharacterPose moving_pose = fbw::resolveCharacterPose(movement);
	if (moving_pose.motion != fbw::CharacterMotion::Jumping || !moving_pose.head_flip_x ||
		!moving_pose.legs_flip_x) {
		std::cerr << "FAIL: airborne horizontal movement pose mismatch\n";
		return 1;
	}

	movement = {};
	movement.velocity_y = 3.1;
	movement.previous_legs_flip_x = true;
	const fbw::CharacterPose rising_pose = fbw::resolveCharacterPose(movement);
	if (rising_pose.motion != fbw::CharacterMotion::Rising || rising_pose.legs_flip_x != true) {
		std::cerr << "FAIL: rising pose or retained leg facing mismatch\n";
		return 1;
	}
	movement.velocity_y = -3.1;
	if (fbw::resolveCharacterPose(movement).motion != fbw::CharacterMotion::Falling) {
		std::cerr << "FAIL: falling pose mismatch\n";
		return 1;
	}

	fbw::CharacterAnimationFrames frames;
	if (!fbw::selectCharacterAnimationFrames(atlas, "fb", {}, 1.0, frames, &error) ||
		frames.head != "fire_head_idle0011" || frames.legs != "fire_legs_idle0000") {
		std::cerr << "FAIL: Fireboy idle loop selection: " << error << "\n";
		return 1;
	}
	if (!fbw::selectCharacterAnimationFrames(atlas, "wg", {}, 1.0, frames, &error) ||
		frames.head != "water_head_idle0000") {
		std::cerr << "FAIL: Watergirl idle loop selection: " << error << "\n";
		return 1;
	}
	fbw::CharacterPose jumping;
	jumping.motion = fbw::CharacterMotion::Jumping;
	if (!fbw::selectCharacterAnimationFrames(atlas, "fb", jumping, 0.5, frames, &error) ||
		frames.head != "fire_head_jumping0004" || frames.legs != "fire_legs_running0007" ||
		!frames.ice.empty()) {
		std::cerr << "FAIL: Fireboy jumping animation: " << error << "\n";
		return 1;
	}
	fbw::CharacterPose icy_jump;
	icy_jump.motion = fbw::CharacterMotion::Jumping;
	icy_jump.show_ice_overlay = true;
	if (!fbw::selectCharacterAnimationFrames(atlas, "wg", icy_jump, 0.35, frames, &error) ||
		frames.legs != "water_legs_running0003" || frames.ice != "water_ice_legs0000") {
		std::cerr << "FAIL: Watergirl ice animation: " << error << "\n";
		return 1;
	}
	fbw::CharacterPose rising;
	rising.motion = fbw::CharacterMotion::Rising;
	if (!fbw::selectCharacterAnimationFrames(atlas, "fb", rising, 0.2, frames, &error) ||
		frames.head != "fire_head_rising0001" || frames.legs != "fire_legs_idle0000") {
		std::cerr << "FAIL: rising animation: " << error << "\n";
		return 1;
	}
	fbw::CharacterPose falling;
	falling.motion = fbw::CharacterMotion::Falling;
	if (!fbw::selectCharacterAnimationFrames(atlas, "fb", falling, 0.2, frames, &error) ||
		frames.head != "fire_head_falling0001") {
		std::cerr << "FAIL: falling animation: " << error << "\n";
		return 1;
	}

	fbw::RenderQueue queue;
	if (!fbw::appendCharacterAnimation(queue, atlas, "CharAssets", "wg",
		100.0, 100.0, 0.95, icy_jump, 0.35, &error) || queue.commands().size() != 3) {
		std::cerr << "FAIL: cannot build Watergirl draw commands: " << error << "\n";
		return 1;
	}
	if (queue.commands()[0].frame_name != "water_legs_running0003" ||
		queue.commands()[1].frame_name != "water_head_jumping0010" ||
		queue.commands()[2].frame_name != "water_ice_legs0000") {
		std::cerr << "FAIL: character render command order/frame selection mismatch\n";
		return 1;
	}
	queue.clear();
	if (!fbw::appendCharacterAnimation(queue, atlas, "CharAssets", "fb",
		100.0, 100.0, 0.95, rising, 0.2, &error) ||
		queue.commands().size() != 2 ||
		queue.commands()[0].frame_name != "fire_head_rising0001" ||
		queue.commands()[1].frame_name != "fire_legs_idle0000") {
		std::cerr << "FAIL: rising render-layer child order mismatch: " << error << "\n";
		return 1;
	}
	if (fbw::selectCharacterAnimationFrames(atlas, "x", {}, 0.0, frames, nullptr)) {
		std::cerr << "FAIL: unknown character id should be rejected\n";
		return 1;
	}

	std::cout << "PASS: Fireboy/Watergirl idle, run/jump, rise/fall, ice overlay, facing, head rotation and sprite draw order\n";
	return 0;
}
