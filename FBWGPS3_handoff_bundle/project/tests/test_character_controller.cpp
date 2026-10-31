#include "character_controller.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {
bool near(double a, double b, double epsilon = 1e-6) {
	return std::abs(a - b) <= epsilon;
}
}

int main() {	
	using namespace fw::character;
	std::string error;
	StepCommand command;
	CharacterController controller;

	ControllerInput input;
	input.character = "fb";
	input.velocity = {3.0, 0.0};
	if (!controller.step(input, command, &error) || !command.apply_force ||
		!near(command.force.x, -21.0) || !near(command.force.y, 0.0) ||
		!near(controller.state().current_run_force, 0.475 * runForce) ||
		!near(command.feet_friction, feetFriction)) {
		std::cerr << "FAIL: idle braking command: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "fb";
	input.right = true;
	input.feet_normals = {{0.0, 1.0}};
	if (!controller.step(input, command, &error) || !command.apply_force ||
		!near(controller.state().current_run_force, 0.475 * runForce + 130.0 * input.delta_seconds / (0.475 * runForce)) ||
		command.force.x >= 0.0 || !near(command.force.y, 20.0)) {
		std::cerr << "FAIL: grounded run-force ramp: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "fb";
	input.left = true;
	if (!controller.step(input, command, &error) || !command.apply_force ||
		!near(command.force.x, floatForce) || !near(command.force.y, 0.0)) {
		std::cerr << "FAIL: airborne horizontal float force: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "fb";
	input.right = true;
	input.on_ice = true;
	input.feet_normals = {{0.0, 1.0}};
	if (!controller.step(input, command, &error) || !command.apply_force ||
		!near(command.force.x, -controller.state().current_run_force * 0.2) ||
		!near(command.force.y, 4.0) || !near(command.feet_friction, 0.0) ||
		!near(command.max_speed, defaultMaxSpeed)) {
		std::cerr << "FAIL: Fireboy ice force/friction: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "wg";
	input.on_ice = true;
	input.velocity = {4.0, 0.0};
	input.right = true;
	input.feet_normals = {{0.0, 1.0}};
	if (!controller.step(input, command, &error) || !command.override_velocity ||
		!near(command.max_speed, waterIceMaxSpeed) || !near(command.velocity.length(), waterIceMaxSpeed) ||
		!near(command.feet_friction, 0.7)) {
		std::cerr << "FAIL: Watergirl ice speed/friction clamp: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "fb";
	input.jump = true;
	input.feet_normals = {{0.0, 1.0}};
	if (!controller.step(input, command, &error) || !command.jump_triggered ||
		!command.apply_jump_impulse || !command.override_velocity ||
		!near(command.jump_impulse.y, jumpImpulse) ||
		!near(command.velocity.y, jumpVelocity) || !controller.state().prohibit_jump) {
		std::cerr << "FAIL: normal-ground jump: " << error << "\n";
		return 1;
	}
	if (!controller.step(input, command, &error) || command.jump_triggered) {
		std::cerr << "FAIL: jump must not retrigger while held\n";
		return 1;
	}
	input.jump = false;
	if (!controller.step(input, command, &error) || controller.state().prohibit_jump) {
		std::cerr << "FAIL: releasing jump should re-arm jump input\n";
		return 1;
	}
	input.jump = true;
	if (!controller.step(input, command, &error) || !command.jump_triggered) {
		std::cerr << "FAIL: jump should trigger again after release\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "fb";
	input.jump = true;
	input.feet_normals = {{0.8, 0.6}};
	if (!controller.step(input, command, &error) || !command.jump_triggered ||
		command.velocity.y <= 0.0 || command.velocity.y >= jumpVelocity ||
		!near(command.force.y, 0.0)) {
		std::cerr << "FAIL: slope jump velocity/vertical-force suppression: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "wg";
	input.jump = true;
	input.on_ice = true;
	input.feet_normals = {{0.0, 1.0}};
	if (!controller.step(input, command, &error) || !command.jump_triggered ||
		!near(command.velocity.length(), waterIceMaxSpeed)) {
		std::cerr << "FAIL: Watergirl ice jump speed cap: " << error << "\n";
		return 1;
	}

	controller.reset();
	input.velocity = {4.0, 0.0};
	if (!controller.step(input, command, &error) || !command.jump_triggered ||
		!near(command.velocity.x, 0.0) || !near(command.velocity.y, waterIceMaxSpeed)) {
		std::cerr << "FAIL: Watergirl no-input ice braking must precede jump velocity: " << error << "\n";
		return 1;
	}

	controller.reset();
	input = {};
	input.character = "fb";
	input.velocity = {100.0, 0.0};
	if (!controller.step(input, command, &error) || !command.override_velocity ||
		!near(command.velocity.length(), absoluteMaxSpeed)) {
		std::cerr << "FAIL: absolute character speed clamp: " << error << "\n";
		return 1;
	}

	input.character = "unknown";
	if (controller.step(input, command, nullptr)) {
		std::cerr << "FAIL: unsupported character id should be rejected\n";
		return 1;
	}
	input.character = "fb";
	input.delta_seconds = 0.0;
	if (controller.step(input, command, nullptr)) {
		std::cerr << "FAIL: zero timestep should be rejected\n";
		return 1;
	}

	std::cout << "PASS: character run-force ramp, air control, ice friction/speed, jump impulse/velocity, hold lock, slope jump and validation\n";
	return 0;
}
