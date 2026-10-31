#pragma once

#include "render_queue.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace fbw {

enum class CharacterMotion {
	Idle,
	Jumping,
	Rising,
	Falling
};

// Input snapshot required by the original character animation decision tree.
// The physics layer supplies a normalized ground normal from the best feet
// contact and current Box2D linear velocity. The horizontal/vertical thresholds
// below assume the original Box2D world units per second (metres per second).
struct CharacterMovementSample {
	bool grounded = false;
	bool left_down = false;
	bool right_down = false;
	bool on_ice = false;
	double velocity_x = 0.0;
	double velocity_y = 0.0;
	double ground_normal_x = 0.0;
	double ground_normal_y = 1.0;
	double previous_head_rotation_degrees = 0.0;
	bool previous_legs_flip_x = false;
};

struct CharacterPose {
	CharacterMotion motion = CharacterMotion::Idle;
	bool head_flip_x = false;
	bool legs_flip_x = false;
	bool show_ice_overlay = false;
	double head_rotation_degrees = 0.0;
};

inline double clampAngle(double value, double low, double high) {
	return std::max(low, std::min(high, value));
}

// Mirrors the animation-state selection near the end of Character.update() in
// init.js. Positive Y is upward in Box2D's world coordinate system.
inline CharacterPose resolveCharacterPose(const CharacterMovementSample& sample) {
	constexpr double pi = 3.14159265358979323846;
	CharacterPose pose;
	pose.legs_flip_x = sample.previous_legs_flip_x;
	pose.head_rotation_degrees = sample.previous_head_rotation_degrees;

	if (sample.grounded) {
		if (sample.left_down) {
			pose.motion = CharacterMotion::Jumping;
			pose.head_flip_x = true;
			pose.legs_flip_x = true;
			const double angle = std::atan2(sample.ground_normal_y, sample.ground_normal_x) - pi * 0.5;
			pose.head_rotation_degrees = clampAngle(angle, -0.1 * pi, 0.25 * pi) * 180.0 / pi;
		} else if (sample.right_down) {
			pose.motion = CharacterMotion::Jumping;
			pose.head_flip_x = false;
			pose.legs_flip_x = false;
			const double angle = std::atan2(sample.ground_normal_y, sample.ground_normal_x) - pi * 0.5;
			pose.head_rotation_degrees = clampAngle(angle, -0.25 * pi, 0.1 * pi) * 180.0 / pi;
		} else {
			pose.motion = CharacterMotion::Idle;
			pose.head_flip_x = false;
			pose.head_rotation_degrees = 0.0;
		}
	} else {
		const double speed = std::hypot(sample.velocity_x, sample.velocity_y);
		const double nx = speed > 1e-9 ? sample.velocity_x / speed : 0.0;
		const double ny = speed > 1e-9 ? sample.velocity_y / speed : 0.0;
		const double direction_angle = std::atan2(std::max(-0.333, ny), nx);
		if (sample.velocity_x > 4.0) {
			pose.motion = CharacterMotion::Jumping;
			pose.head_flip_x = true;
			pose.legs_flip_x = true;
			pose.head_rotation_degrees = direction_angle * 180.0 / pi;
		} else if (sample.velocity_x < -4.0) {
			pose.motion = CharacterMotion::Jumping;
			pose.head_flip_x = false;
			pose.legs_flip_x = false;
			pose.head_rotation_degrees = (direction_angle + pi) * 180.0 / pi;
		} else if (sample.velocity_y > 3.0) {
			pose.motion = CharacterMotion::Rising;
			pose.head_flip_x = false;
			pose.head_rotation_degrees = 0.0;
		} else if (sample.velocity_y < -3.0) {
			pose.motion = CharacterMotion::Falling;
			pose.head_flip_x = false;
			pose.head_rotation_degrees = 0.0;
		} else if (sample.left_down) {
			pose.motion = CharacterMotion::Jumping;
			pose.head_flip_x = true;
			pose.legs_flip_x = true;
		} else if (sample.right_down) {
			pose.motion = CharacterMotion::Jumping;
			pose.head_flip_x = false;
			pose.legs_flip_x = false;
		} else {
			pose.motion = CharacterMotion::Idle;
			pose.head_flip_x = false;
			pose.head_rotation_degrees = 0.0;
		}
	}
	pose.show_ice_overlay = pose.motion == CharacterMotion::Jumping && sample.on_ice;
	return pose;
}

inline std::string paddedFrameName(const std::string& prefix, int frame) {
	std::string number = std::to_string(frame);
	while (number.size() < 4) number.insert(number.begin(), '0');
	return prefix + number;
}

inline int contiguousFrameCount(const Atlas& atlas, const std::string& prefix, int maximum = 64) {
	int count = 0;
	for (; count < maximum; ++count) {
		if (!atlas.find(paddedFrameName(prefix, count))) break;
	}
	return count;
}

struct CharacterAnimationFrames {
	std::string head;
	std::string legs;
	std::string ice;
};

inline bool selectCharacterAnimationFrames(const Atlas& atlas, const std::string& character,
	const CharacterPose& pose, double elapsed_seconds, CharacterAnimationFrames& output,
	std::string* error = nullptr) {
	if ((character != "fb" && character != "wg") || !std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
		if (error) *error = "invalid character id or animation time";
		return false;
	}
	const std::string prefix = character == "wg" ? "water" : "fire";
	std::string head_animation;
	int head_fps = 30;
	switch (pose.motion) {
	case CharacterMotion::Idle: head_animation = "_head_idle"; break;
	case CharacterMotion::Jumping: head_animation = "_head_jumping"; break;
	case CharacterMotion::Rising: head_animation = "_head_rising"; break;
	case CharacterMotion::Falling: head_animation = "_head_falling"; break;
	}
	const std::string head_prefix = prefix + head_animation;
	const int head_count = contiguousFrameCount(atlas, head_prefix);
	if (head_count <= 0) {
		if (error) *error = "missing character head animation: " + head_prefix;
		return false;
	}
	const int head_index = static_cast<int>(std::floor(elapsed_seconds * head_fps)) % head_count;
	CharacterAnimationFrames selected;
	selected.head = paddedFrameName(head_prefix, head_index);

	if (pose.motion == CharacterMotion::Jumping) {
		const int leg_fps = character == "wg" && pose.show_ice_overlay ? 10 : 30;
		const std::string legs_prefix = prefix + "_legs_running";
		const int leg_count = contiguousFrameCount(atlas, legs_prefix);
		if (leg_count <= 0) {
			if (error) *error = "missing character running animation: " + legs_prefix;
			return false;
		}
		selected.legs = paddedFrameName(legs_prefix,
			static_cast<int>(std::floor(elapsed_seconds * leg_fps)) % leg_count);
	} else {
		selected.legs = prefix + "_legs_idle0000";
		if (!atlas.find(selected.legs)) {
			if (error) *error = "missing idle legs frame: " + selected.legs;
			return false;
		}
	}

	if (pose.show_ice_overlay) {
		const std::string ice_prefix = prefix + "_ice_legs";
		const int ice_count = contiguousFrameCount(atlas, ice_prefix);
		if (ice_count <= 0) {
			if (error) *error = "missing ice overlay animation: " + ice_prefix;
			return false;
		}
		selected.ice = paddedFrameName(ice_prefix,
			static_cast<int>(std::floor(elapsed_seconds * 10.0)) % ice_count);
	}

	output = std::move(selected);
	if (error) error->clear();
	return true;
}

inline bool appendCharacterAnimation(RenderQueue& queue, const Atlas& atlas,
	const std::string& atlas_key, const std::string& character,
	double center_x, double center_y, double scale, const CharacterPose& pose,
	double elapsed_seconds, std::string* error = nullptr) {
	if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
		!std::isfinite(scale) || scale <= 0.0) {
		if (error) *error = "invalid character sprite transform";
		return false;
	}
	CharacterAnimationFrames frames;
	if (!selectCharacterAnimationFrames(atlas, character, pose, elapsed_seconds, frames, error)) return false;
	auto append_legs = [&]() {
		return queue.appendFrame(atlas, atlas_key, frames.legs, center_x, center_y, scale,
			RenderLayer::Characters, error, 0.0, pose.legs_flip_x, false);
	};
	auto append_head = [&]() {
		return queue.appendFrame(atlas, atlas_key, frames.head, center_x, center_y, scale,
			RenderLayer::Characters, error, pose.head_rotation_degrees, pose.head_flip_x, false);
	};
	// Phaser moves the legs above the head while rising; all other states retain
	// constructor order (legs first, then head).
	if (pose.motion == CharacterMotion::Rising) {
		if (!append_head() || !append_legs()) return false;
	} else {
		if (!append_legs() || !append_head()) return false;
	}
	if (!frames.ice.empty() && !queue.appendFrame(atlas, atlas_key, frames.ice,
		center_x, center_y, scale, RenderLayer::Characters, error,
		0.0, pose.legs_flip_x, false)) return false;
	if (error) error->clear();
	return true;
}

} // namespace fbw
