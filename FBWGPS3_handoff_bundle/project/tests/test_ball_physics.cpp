// Rolling-ball tests: circle collider in the fallback physics, then balls inside the integrated game world.
// The original ball (init.js, Devices/Ball) is a Box2D circle: radius 0.4 m (12.8 px), density 2.3, friction 0.2,
// restitution 0.1, no damping unless a map property asks for it (none of the 32 maps does).
// Known approximations kept on purpose: no restitution, no friction-limited slipping (the ball always rolls without
// slipping), pushing by a character uses a fixed acceleration rather than a contact solver.
#include "game_world.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " << msg << '\n'; ++failures; } } while (0)

constexpr double kRadius = 12.8;

fbw::PhysicsBody makeBall(double x, double y) {
	fbw::PhysicsBody b;
	b.id = "ball";
	b.x = x; b.y = y;
	b.radius = kRadius;
	b.half_width = b.half_height = kRadius;
	return b;
}

fbw::PhysicsTuning rollingTuning() {
	fbw::PhysicsTuning t;
	t.run_acceleration = 0.0; t.air_acceleration = 0.0; t.horizontal_drag = 0.0; t.jump_speed = 0.0; t.max_speed = 1000.0;
	return t;
}

void step(fbw::PhysicsWorld& w, fbw::PhysicsBody& b, int frames) {
	const fbw::PhysicsTuning t = rollingTuning();
	for (int i = 0; i < frames; ++i) w.step(b, {}, 1.0 / 60.0, t);
}

void testFloor() {
	std::vector<fw::GroundFixture> floor(1);
	floor[0].v = {{0, 100}, {600, 100}, {600, 140}, {0, 140}};
	fbw::PhysicsWorld w(floor);
	fbw::PhysicsBody ball = makeBall(80, 40);
	step(w, ball, 180);
	CHECK(ball.grounded, "ball did not land");
	CHECK(std::abs(ball.y + kRadius - 100.0) < 0.05, "ball not resting on the floor, bottom=" << ball.y + kRadius);
	CHECK(std::abs(ball.vy) < 1.0, "ball still moving vertically: " << ball.vy);

	// No damping: a rolling ball keeps its speed on flat ground.
	ball.vx = 100.0;
	const double x0 = ball.x;
	step(w, ball, 60);
	CHECK(std::abs(ball.vx - 100.0) < 1.0, "rolling ball lost speed on flat ground: " << ball.vx);
	CHECK(std::abs((ball.x - x0) - 100.0) < 3.0, "ball travelled " << ball.x - x0 << " px in one second");
	CHECK(std::abs(ball.y + kRadius - 100.0) < 0.1, "ball left the floor while rolling");
}

void testWall() {
	std::vector<fw::GroundFixture> t(2);
	t[0].v = {{0, 100}, {600, 100}, {600, 140}, {0, 140}};
	t[1].v = {{200, 0}, {220, 0}, {220, 100}, {200, 100}};
	fbw::PhysicsWorld w(t);
	fbw::PhysicsBody ball = makeBall(120, 100 - kRadius);
	ball.vx = 150.0;
	step(w, ball, 120);
	CHECK(ball.x + kRadius <= 200.05, "ball penetrated the wall: right edge=" << ball.x + kRadius);
	CHECK(std::abs(ball.vx) < 1.0, "ball kept horizontal speed against a wall: " << ball.vx);
}

void testSlope() {
	// Surface y = 100 + 0.5 x, descending to the right.
	std::vector<fw::GroundFixture> t(1);
	t[0].v = {{0, 100}, {400, 300}, {0, 300}};
	fbw::PhysicsWorld w(t);
	fbw::PhysicsBody ball = makeBall(60, 80);
	step(w, ball, 30);                 // land and settle onto the slope
	const double v_mid = ball.vx;
	const double x_mid = ball.x;
	step(w, ball, 30);
	const double accel = (ball.vx - v_mid) / 0.5;
	CHECK(ball.vx > 20.0, "ball did not roll downhill, vx=" << ball.vx);
	CHECK(ball.x > x_mid + 5.0, "ball did not travel downhill");
	// Solid disc rolling without slipping: acceleration along the slope is (2/3) g sin(theta), theta = atan(0.5),
	// g = 1000 px/s^2. vx is the horizontal projection of that, so the expected value carries one more cos(theta).
	const double theta = std::atan(0.5);
	const double expected = (2.0 / 3.0) * 1000.0 * std::sin(theta) * std::cos(theta);
	CHECK(std::abs(accel - expected) < expected * 0.03, "slope vx acceleration " << accel << " vs expected " << expected);
	// And it is clearly slower than frictionless sliding (g sin(theta) cos(theta)).
	CHECK(accel < 1000.0 * std::sin(theta) * std::cos(theta) * 0.8, "ball accelerates like a sliding block: " << accel);
	// The centre must stay one radius from the surface line.
	const double dist = (100.0 + 0.5 * ball.x - ball.y) / std::sqrt(1.25);
	CHECK(std::abs(dist - kRadius) < 0.3, "ball sank into / floated off the slope, distance=" << dist);
}

void testWorldBalls(const std::string& game_dir) {
	std::string error;
	fw::progression::ForestCatalog catalog;
	if (!fw::progression::loadForestCatalog(game_dir, catalog, &error)) { std::cerr << "catalog: " << error << '\n'; ++failures; return; }
	int total_expected = 0, total_found = 0;
	int first_ball_level = -1;
	for (size_t i = 0; i < catalog.levels.size(); ++i) {
		fw::progression::Progress progress;
		fbw::GameWorld world;
		if (!world.start(game_dir, catalog.levels[i], false, progress, &error)) { std::cerr << "start: " << error << '\n'; ++failures; continue; }
		int expected = 0;
		for (const fw::MapObject& o : world.session().level().objects) if (o.type == "ball") ++expected;
		int found = 0;
		for (const auto& b : world.boxes()) if (b.type == "ball") ++found;
		total_expected += expected; total_found += found;
		CHECK(found == expected, "level " << catalog.levels[i].filename << ": " << found << " ball bodies for " << expected << " ball objects");
		if (expected > 0 && first_ball_level < 0) first_ball_level = static_cast<int>(i);
		for (int f = 0; f < 180; ++f) world.update({}, 1.0 / 60.0);
		const double w_px = world.session().level().w * 32.0, h_px = world.session().level().h * 32.0;
		for (const auto& b : world.boxes()) {
			if (b.type != "ball") continue;
			const bool finite = std::isfinite(b.body.x) && std::isfinite(b.body.y);
			CHECK(finite, "non-finite ball position in " << catalog.levels[i].filename);
			CHECK(b.body.x > 0.0 && b.body.x < w_px && b.body.y > 0.0 && b.body.y < h_px,
				"ball left the map in " << catalog.levels[i].filename << ": " << b.body.x << "," << b.body.y);
		}
	}
	CHECK(total_expected == 12, "expected 12 ball objects across the catalogue, found " << total_expected);
	CHECK(total_found == total_expected, "ball bodies " << total_found << " vs objects " << total_expected);

	// A character running into a ball must set it rolling.
	if (first_ball_level < 0) { std::cerr << "no level with a ball\n"; ++failures; return; }
	fw::progression::Progress progress;
	fbw::GameWorld world;
	if (!world.start(game_dir, catalog.levels[first_ball_level], false, progress, &error)) { std::cerr << error << '\n'; ++failures; return; }
	for (int f = 0; f < 120; ++f) world.update({}, 1.0 / 60.0);       // let everything settle
	const fbw::BoxRuntime* ball = nullptr;
	for (const auto& b : world.boxes()) if (b.type == "ball") { ball = &b; break; }
	if (!ball) { std::cerr << "no ball in the push-test level\n"; ++failures; return; }
	const double bx = ball->body.x, by = ball->body.y, bvx = ball->body.vx;
	const fbw::PhysicsBody* fb = world.body("fb");
	CHECK(world.teleportBody("fb", bx - (fb->half_width + kRadius + 1.0), by + kRadius - fb->half_height),
		"teleportBody failed");
	// Control: standing next to the ball without pressing anything must not move it.
	for (int f = 0; f < 20; ++f) world.update({}, 1.0 / 60.0);
	CHECK(std::abs(ball->body.x - bx) < 1.0, "ball moved without any input: " << ball->body.x - bx << " px");
	// Pressing away from the ball must not drag it either.
	fbw::GameWorldInput away; away.fireboy.left = true;
	for (int f = 0; f < 10; ++f) world.update(away, 1.0 / 60.0);
	CHECK(std::abs(ball->body.x - bx) < 1.0, "ball was dragged by a character walking away: " << ball->body.x - bx << " px");
	CHECK(world.teleportBody("fb", bx - (fb->half_width + kRadius + 1.0), by + kRadius - fb->half_height),
		"second teleportBody failed");
	fbw::GameWorldInput push; push.fireboy.right = true;
	for (int f = 0; f < 30; ++f) world.update(push, 1.0 / 60.0);
	CHECK(ball->body.x > bx + 5.0 || ball->body.vx > bvx + 20.0,
		"ball was not pushed: x " << bx << " -> " << ball->body.x << ", vx " << bvx << " -> " << ball->body.vx);
}
} // namespace

int main(int argc, char** argv) {
	if (argc != 2) return 2;
	testFloor();
	testWall();
	testSlope();
	testWorldBalls(argv[1]);
	if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
	std::cout << "Ball physics PASS: floor, rolling, wall, slope acceleration, 12 world balls, push\n";
	return 0;
}
