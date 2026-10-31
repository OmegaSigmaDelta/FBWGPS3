#pragma once

#include "character_spec.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fw { namespace character {

struct Vector2 {
	double x = 0.0;
	double y = 0.0;
	double length() const { return std::hypot(x, y); }
};

struct ControllerInput {
	std::string character = "fb"; // "fb" = Fireboy, "wg" = Watergirl
	double delta_seconds = 1.0 / 60.0;
	Vector2 velocity;
	std::vector<Vector2> feet_normals;
	bool left = false;
	bool right = false;
	bool jump = false;
	bool on_ice = false;
};

struct ControllerState {
	// The source resets currentRunForce to 0.475 * runForce whenever no
	// horizontal input is held. This is the stable value after the initial
	// no-input update, before ordinary gameplay input begins.
	double current_run_force = 0.475 * runForce;
	bool prohibit_jump = false;
};

// Runtime adapter order: apply jump_impulse when requested, then velocity when
// override_velocity is true, then apply force when apply_force is true. This
// preserves the original Character.update() operation order.
struct StepCommand {
	bool apply_jump_impulse = false;
	Vector2 jump_impulse;
	bool override_velocity = false;
	Vector2 velocity;
	bool apply_force = false;
	Vector2 force;
	double max_speed = defaultMaxSpeed;
	double feet_friction = feetFriction;
	bool jump_triggered = false;
};

class CharacterController {
public:
	const ControllerState& state() const { return state_; }
	void reset() { state_ = {}; }

	// Generates the exact Box2D operations used by Character.update(). This
	// layer deliberately does not own a Box2D body: a future Box2D/RSX runtime
	// adapter applies these operations in the order returned here.
	bool step(const ControllerInput& input, StepCommand& output, std::string* error = nullptr) {
		if (input.character != "fb" && input.character != "wg")
			return fail(error, "character must be fb or wg");
		if (!std::isfinite(input.delta_seconds) || input.delta_seconds <= 0.0 ||
			!std::isfinite(input.velocity.x) || !std::isfinite(input.velocity.y))
			return fail(error, "invalid controller timestep or velocity");
		for (const Vector2& normal : input.feet_normals) {
			if (!std::isfinite(normal.x) || !std::isfinite(normal.y))
				return fail(error, "invalid feet contact normal");
		}

		const bool watergirl = input.character == "wg";
		const double max_speed = watergirl && input.on_ice ? waterIceMaxSpeed : defaultMaxSpeed;
		const double friction = !input.on_ice ? feetFriction : (watergirl ? 0.7 : 0.0);
		const Vector2* normal = nullptr;
		// The JS code sorts contacts by dot(normal, (0,-1)) ascending, which is
		// equivalent to choosing the contact with the greatest positive Y normal.
		for (const Vector2& candidate : input.feet_normals) {
			if (!normal || candidate.y > normal->y) normal = &candidate;
		}

		StepCommand command;
		command.max_speed = max_speed;
		command.feet_friction = friction;
		command.velocity = input.velocity;
		Vector2 force;
		bool force_defined = false;
		bool suppress_force_y = false;

		if (input.left) {
			if (normal) {
				const double increment = 130.0 * input.delta_seconds / state_.current_run_force;
				const double adjusted_increment = input.velocity.length() < 1.0 &&
					state_.current_run_force > 0.5 * runForce ? runForce : increment;
				if (state_.current_run_force < runForce)
					state_.current_run_force = std::min(state_.current_run_force + adjusted_increment, runForce);
				const double p = normal->y > 0.6 ? normal->y : 0.0;
				const double magnitude = std::max(0.0, std::sqrt(p));
				force.x = state_.current_run_force * normal->y * magnitude;
				force.y = -state_.current_run_force * normal->x * magnitude;
				if (p > 0.6 && force.y >= 0.0 && force.y < 20.0) force.y += 20.0;
			} else {
				force = {floatForce, 0.0};
			}
			force_defined = true;
		} else if (input.right) {
			if (normal) {
				const double increment = 130.0 * input.delta_seconds / state_.current_run_force;
				const double adjusted_increment = input.velocity.length() < 1.0 &&
					state_.current_run_force > 0.5 * runForce ? runForce : increment;
				if (state_.current_run_force < runForce)
					state_.current_run_force = std::min(state_.current_run_force + adjusted_increment, runForce);
				const double p = normal->y > 0.6 ? normal->y : 0.0;
				const double magnitude = std::max(0.0, std::sqrt(p));
				force.x = -state_.current_run_force * normal->y * magnitude;
				force.y = state_.current_run_force * normal->x * magnitude;
				if (p > 0.6 && force.y >= 0.0 && force.y < 20.0) force.y += 20.0;
			} else {
				force = {-floatForce, 0.0};
			}
			force_defined = true;
		} else {
			state_.current_run_force = 0.475 * runForce;
			if (input.on_ice && watergirl) {
				command.override_velocity = true;
				command.velocity = {0.0, 0.0};
				force = {20.0 * -input.velocity.x, 0.0};
			} else {
				force = {7.0 * -input.velocity.x, 0.0};
			}
			force_defined = true;
		}

		if (!input.jump) state_.prohibit_jump = false;
		const bool can_jump = normal && normal->y > 0.2;
		const bool jump_contact = can_jump || input.feet_normals.size() >= 2;
		if (!state_.prohibit_jump && input.jump && jump_contact) {
			command.apply_jump_impulse = true;
			command.jump_impulse = {0.0, jumpImpulse};
			command.jump_triggered = true;
			state_.prohibit_jump = true;
			if (input.on_ice) {
				if (watergirl) {
					command.override_velocity = true;
					command.velocity = {command.velocity.x, jumpIceVelocity};
				} else if (normal->y > 0.9) {
					command.override_velocity = true;
					command.velocity = {input.velocity.x, 3.0 * normal->y};
				} else {
					command.override_velocity = true;
					command.velocity = {3.0 * normal->x, 3.0 * normal->y};
				}
			} else if (normal->y > 0.65) {
				command.override_velocity = true;
				command.velocity = {input.velocity.x, jumpVelocity};
			} else {
				command.override_velocity = true;
				command.velocity = {input.velocity.x,
					std::sqrt(std::sqrt(normal->y)) * jumpVelocity};
				const double speed = command.velocity.length();
				if (speed > jumpVelocity) {
					command.velocity.x *= jumpVelocity / speed;
					command.velocity.y *= jumpVelocity / speed;
				}
				suppress_force_y = true;
			}
		}

		if (force_defined) {
			if (suppress_force_y) force.y = 0.0;
			if (input.on_ice) {
				const double ice_multiplier = watergirl ? 1.0 : 0.2;
				force.x *= ice_multiplier;
				force.y *= ice_multiplier;
				const double speed = command.velocity.length();
				if (speed > max_speed && speed > 0.0) {
					command.override_velocity = true;
					command.velocity.x *= max_speed / speed;
					command.velocity.y *= max_speed / speed;
				}
			}
			if ((force.x > 0.0 && command.velocity.x < max_speed) ||
				(force.x < 0.0 && command.velocity.x > -max_speed)) {
				command.apply_force = true;
				command.force = force;
			}
		}

		const double final_speed = command.velocity.length();
		if (final_speed > absoluteMaxSpeed && final_speed > 0.0) {
			command.override_velocity = true;
			command.velocity.x *= absoluteMaxSpeed / final_speed;
			command.velocity.y *= absoluteMaxSpeed / final_speed;
		}
		output = command;
		if (error) error->clear();
		return true;
	}

private:
	static bool fail(std::string* error, const std::string& message) {
		if (error) *error = message;
		return false;
	}
	ControllerState state_;
};

}} // namespace fw::character
