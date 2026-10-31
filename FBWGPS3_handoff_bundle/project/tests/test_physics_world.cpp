#include "physics_world.hpp"
#include <cmath>
#include <iostream>
#include <vector>

int main() {
	std::vector<fw::GroundFixture> terrain(1);
	terrain[0].v = {{0, 100}, {320, 100}, {320, 140}, {0, 140}};
	fbw::PhysicsWorld world(terrain);
	fbw::PhysicsBody body;
	body.id = "fb";
	body.x = 80;
	body.y = 50;
	for (int i = 0; i < 120; ++i) world.step(body, {}, 1.0 / 60.0);
	if (!body.grounded || std::abs((body.y + body.half_height) - 100.0) > 0.01 || std::abs(body.vy) > 0.01) {
		std::cerr << "falling body did not settle on platform: y=" << body.y << " vy=" << body.vy << " grounded=" << body.grounded << '\n';
		return 1;
	}
	fbw::PhysicsInput jump; jump.jump = true;
	world.step(body, jump, 1.0 / 60.0);
	if (body.grounded || body.vy >= 0.0) { std::cerr << "jump did not launch upward\n"; return 1; }
	for (int i = 0; i < 120; ++i) world.step(body, {}, 1.0 / 60.0);
	if (!body.grounded) { std::cerr << "body did not land after jump\n"; return 1; }
	fbw::PhysicsBody wall_body; wall_body.x = -10; wall_body.y = 70;
	world.step(wall_body, {}, 1.0 / 60.0);
	if (world.overlapsTerrain(wall_body)) { std::cerr << "test body unexpectedly overlaps platform\n"; return 1; }
	// A sloped convex fixture should resolve along its surface normal instead of
	// acting like an axis-aligned wall.
	std::vector<fw::GroundFixture> slope(1);
	slope[0].v = {{0,100},{100,50},{100,130},{0,130}};
	fbw::PhysicsWorld slope_world(slope);
	fbw::PhysicsBody slope_body; slope_body.x=75; slope_body.y=25;
	for(int i=0;i<120;++i) slope_world.step(slope_body,{},1.0/60.0);
	if(!slope_body.grounded || slope_world.overlapsTerrain(slope_body)) {
		std::cerr << "body did not resolve on sloped terrain\n"; return 1;
	}
	// A fast horizontal approach must stop at a vertical wall without tunnelling.
	std::vector<fw::GroundFixture> wall(1);
	wall[0].v = {{100, 0}, {120, 0}, {120, 220}, {100, 220}};
	fbw::PhysicsWorld wall_world(wall);
	fbw::PhysicsBody runner; runner.x = 55; runner.y = 100;
	fbw::PhysicsInput run_right; run_right.right = true;
	fbw::PhysicsTuning no_gravity; no_gravity.gravity = 0.0;
	for (int i = 0; i < 90; ++i) wall_world.step(runner, run_right, 1.0 / 60.0, no_gravity);
	if (runner.x + runner.half_width > 100.01 || wall_world.overlapsTerrain(runner) || std::abs(runner.vx) > 0.01) {
		std::cerr << "body penetrated vertical wall: x=" << runner.x << " vx=" << runner.vx << '\n';
		return 1;
	}
	// A ceiling must cancel upward velocity and leave the body below it.
	std::vector<fw::GroundFixture> ceiling(1);
	ceiling[0].v = {{0, 0}, {320, 0}, {320, 12}, {0, 12}};
	fbw::PhysicsWorld ceiling_world(ceiling);
	fbw::PhysicsBody jumper; jumper.x = 80; jumper.y = 45; jumper.vy = -400;
	for (int i = 0; i < 4; ++i) ceiling_world.step(jumper, {}, 1.0 / 60.0, no_gravity);
	if (jumper.y - jumper.half_height < 11.99 || std::abs(jumper.vy) > 0.01 || ceiling_world.overlapsTerrain(jumper)) {
		std::cerr << "body penetrated ceiling: y=" << jumper.y << " vy=" << jumper.vy << '\n';
		return 1;
	}
	// Two adjacent floor fixtures must not create a seam that catches a body.
	std::vector<fw::GroundFixture> seam(2);
	seam[0].v = {{0,100},{100,100},{100,140},{0,140}};
	seam[1].v = {{100,100},{200,100},{200,140},{100,140}};
	fbw::PhysicsWorld seam_world(seam);
	fbw::PhysicsBody seam_runner; seam_runner.x=55; seam_runner.y=75;
	for(int i=0;i<20;++i) { fbw::PhysicsInput move; move.right=true; seam_world.step(seam_runner,move,1.0/60.0); }
	if(!seam_runner.grounded || seam_runner.x <= 100.0 || seam_world.overlapsTerrain(seam_runner)) {
		std::cerr << "body snagged on seam between adjacent floor fixtures: x=" << seam_runner.x << " y=" << seam_runner.y << '\n';
		return 1;
	}
	// A wall collision should not pin vertical movement: gravity can make the
	// character slide down the wall while horizontal penetration stays blocked.
	fbw::PhysicsBody slider; slider.x=75; slider.y=70;
	fbw::PhysicsTuning gravity; gravity.run_acceleration=0.0; gravity.air_acceleration=0.0; gravity.horizontal_drag=0.0; gravity.max_speed=300.0; gravity.gravity=900.0; slider.vx=220.0;
	for(int i=0;i<45;++i) wall_world.step(slider,{},1.0/60.0,gravity);
	if(slider.x+slider.half_width>100.01 || wall_world.overlapsTerrain(slider) || slider.y<=70.0) {
		std::cerr << "body did not slide down while blocked by wall: x=" << slider.x << " y=" << slider.y << '\n';
		return 1;
	}
	// Unfrozen liquid geometry must not behave like a solid platform.
	std::vector<fw::GroundFixture> liquid(1);
	liquid[0].v = {{0, 100}, {320, 100}, {320, 140}, {0, 140}};
	liquid[0].liquid_type = 5;
	fbw::PhysicsWorld liquid_world(liquid);
	fbw::PhysicsBody sinker;
	sinker.x = 80; sinker.y = 50;
	for (int i = 0; i < 30; ++i) liquid_world.step(sinker, {}, 1.0 / 60.0);
	if (sinker.grounded || sinker.y <= 100.0) {
		std::cerr << "unfrozen liquid incorrectly acted as a solid platform\n";
		return 1;
	}
	std::cout << "physics world fall/jump/landing/wall/ceiling/slope/liquid sensor filtering: OK\n";
	return 0;
}
