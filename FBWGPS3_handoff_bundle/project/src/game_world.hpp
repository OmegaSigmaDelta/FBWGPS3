#pragma once

// First integrated, deterministic playable simulation layer. This deliberately
// keeps rendering/platform input separate so SDL2 and PSL1GHT can drive the
// same world. Coordinates are pixels, +Y downward.
#include "level_session.hpp"
#include "physics_world.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>

namespace fbw {

struct GameWorldInput {
	PhysicsInput fireboy;
	PhysicsInput watergirl;
};

struct GameWorldStep {
	PhysicsStepResult fireboy_physics;
	PhysicsStepResult watergirl_physics;
	LevelOutcome outcome = LevelOutcome::Running;
	int newly_collected = 0;
};

struct PlatformRuntime { int id=0,group=0; double x=0,y=0,w=0,h=0,start_x=0,start_y=0,end_x=0,end_y=0; };
// A pushable dynamic object: a box (AABB) or a ball (circle, `type == "ball"`). `angle_deg` is the visual roll angle.
struct BoxRuntime { int id=0; std::string type="box"; PhysicsBody body; double angle_deg=0.0; };

// Ball constants from the original Ball device (init.js): circle radius 0.4 m at 32 px/m, and the spawn offset
// (x + 16, y - 11.2) from the Tiled tile object that the render plan already uses for the ball sprite.
constexpr double kBallRadiusPx = 12.8;
constexpr double kBallSpawnDx = 16.0, kBallSpawnDy = -11.2;

class GameWorld {
public:
	bool start(const std::string& game_dir, const fw::progression::CatalogLevel& level,
		bool single_player, fw::progression::Progress& progress, std::string* error = nullptr) {
		if (!session_.start(game_dir, level, single_player, progress, error)) return false;
		base_terrain_ = session_.ground().fixtures;
		// Add solid surfaces for the static approximation of hanging/pulley
		// platforms and the Roman balance beam.
		for(const fw::MapObject& object:session_.level().objects) {
			auto add_rect=[&](double cx,double cy,double half_w,double half_h) { auto f=makeFixture({{cx-half_w,cy-half_h},{cx+half_w,cy-half_h},{cx+half_w,cy+half_h},{cx-half_w,cy+half_h}}); base_terrain_.push_back(std::move(f)); };
			if(object.type=="hanging" && object.polyline.size()>=2) add_rect(object.x+object.polyline[1].x,object.y+object.polyline[1].y,32.0,8.0);
			if(object.type=="pulley" && object.polyline.size()>=4) { for(size_t i:{size_t(0),size_t(3)}) add_rect(object.x+object.polyline[i].x,object.y+object.polyline[i].y,32.0,8.0); }
			if(object.type=="roman" && object.polygon.size()>=3) {
				const auto& a=object.polygon[1]; const auto& b=object.polygon[2]; const double dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
				if(len>1e-6) { const double nx=-dy/len*15.0,ny=dx/len*15.0; auto f=makeFixture({{object.x+a.x+nx,object.y+a.y+ny},{object.x+b.x+nx,object.y+b.y+ny},{object.x+b.x-nx,object.y+b.y-ny},{object.x+a.x-nx,object.y+a.y-ny}}); base_terrain_.push_back(std::move(f)); }
			}
		}
		platforms_.clear(); boxes_.clear(); groups_.clear(); trigger_contacts_.clear(); lever_states_.clear(); pusher_contacts_.clear(); pusher_initial_states_.clear();
		for (const fw::MapObject& object : session_.level().objects) {
			if (object.type == "platform" && object.w > 0.0 && object.h > 0.0) {
				PlatformRuntime platform; platform.id=object.id; platform.group=object.props["group"].i32();
				platform.x=platform.start_x=object.x; platform.y=platform.start_y=object.y;
				platform.w=object.w; platform.h=object.h;
				platform.end_x=object.x+object.props["dx"].num()*32.0; platform.end_y=object.y+object.props["dy"].num()*32.0;
				platforms_.push_back(platform); groups_.emplace(platform.group,false);
			} else if (object.type == "lever") { groups_.emplace(object.props["group"].i32(),false); lever_states_[object.id]=object.props["initialState"].i32()!=0; }
			else if (object.type == "pusher") {
				groups_.emplace(object.props["group"].i32(),false); pusher_initial_states_[object.id]=object.props["initialState"].i32()!=0;
				const double cx=object.x+32.0,cy=object.y-32.0; auto fixture=makeFixture({{cx-24.0,cy},{cx+24.0,cy},{cx+24.0,cy+19.2},{cx-24.0,cy+19.2}}); base_terrain_.push_back(std::move(fixture));
			} else if (object.type == "box") {
				BoxRuntime box; box.id=object.id; box.body.id="box_"+std::to_string(object.id);
				box.body.x=object.x+32.0; box.body.y=object.y-32.0; box.body.half_width=24.0; box.body.half_height=24.0;
				boxes_.push_back(box);
			} else if (object.type == "ball") {
				BoxRuntime ball; ball.id=object.id; ball.type="ball"; ball.body.id="ball_"+std::to_string(object.id);
				ball.body.x=object.x+kBallSpawnDx; ball.body.y=object.y+kBallSpawnDy;
				ball.body.radius=ball.body.half_width=ball.body.half_height=kBallRadiusPx;
				boxes_.push_back(ball);
			}
		}
		updatePlatformPositions(0.0);
		bodies_.clear();
		liquid_contacts_.clear();
		ice_contacts_.clear();
		jump_was_down_.clear();
		death_timers_.clear();
		for (const auto& item : session_.gameplay().characters()) {
			PhysicsBody body;
			body.id = item.first;
			body.x = item.second.spawn_px.x;
			body.y = item.second.spawn_px.y;
			body.half_width = 12.0;
			body.half_height = 25.0;
			bodies_.emplace(item.first, body);
		}
		if (error) error->clear();
		return true;
	}

	GameWorldStep update(const GameWorldInput& input, double dt) {
		GameWorldStep result;
		if (!std::isfinite(dt) || dt <= 0.0 || session_.finished()) {
			result.outcome = session_.gameplay().outcome();
			return result;
		}
		const double safe_dt = std::min(dt, 0.05);
		updatePlatformPositions(safe_dt);
		rebuildCollision(false);
		updateBoxes(input,safe_dt);
		rebuildCollision(true);
		stepCharacter("fb", input.fireboy, safe_dt, result.fireboy_physics, result.newly_collected);
		stepCharacter("wg", input.watergirl, safe_dt, result.watergirl_physics, result.newly_collected);
		updateLeverContacts();
		for (const auto& item : session_.gameplay().characters()) {
			if (item.second.dying && !item.second.dead) {
				death_timers_[item.first] += safe_dt;
				if (death_timers_[item.first] >= death_animation_seconds_)
					session_.gameplay().finishDeathAnimation(item.first);
			}
		}
		session_.update(dt);
		result.outcome = session_.gameplay().outcome();
		return result;
	}

	const PhysicsBody* body(const std::string& id) const {
		auto it = bodies_.find(id);
		return it == bodies_.end() ? nullptr : &it->second;
	}
	// Debug/test helper: place a character body at a position with zero velocity. Returns false for an unknown id.
	bool teleportBody(const std::string& id,double x,double y) {
		PhysicsBody* body=findBody(id); if(!body) return false;
		body->x=x; body->y=y; body->vx=body->vy=0.0; body->grounded=false; return true;
	}
	const LevelSession& session() const { return session_; }
	LevelSession& session() { return session_; }
	const std::vector<PlatformRuntime>& platforms() const { return platforms_; }
	const std::vector<BoxRuntime>& boxes() const { return boxes_; }
	const std::vector<BoxRuntime>& movableObjects() const { return boxes_; }
	bool groupActive(int group) const { auto it=groups_.find(group); return it!=groups_.end() && it->second; }
	static constexpr double death_animation_seconds_ = 0.45;

private:
	LevelSession session_;
	PhysicsWorld physics_;
	std::vector<fw::GroundFixture> base_terrain_;
	std::vector<PlatformRuntime> platforms_;
	std::vector<BoxRuntime> boxes_;
	std::map<int,bool> groups_;
	std::set<int> trigger_contacts_;
	std::map<int,bool> lever_states_;
	std::map<int,bool> pusher_initial_states_;
	std::set<int> pusher_contacts_;
	std::map<std::string, PhysicsBody> bodies_;
	std::map<std::string, std::set<int>> liquid_contacts_;
	std::map<std::string, std::set<int>> ice_contacts_;
	std::map<std::string, bool> jump_was_down_;
	std::map<std::string, double> death_timers_;

	static fw::GroundFixture makeFixture(std::initializer_list<fw::Pt> points) {
		fw::GroundFixture fixture; fixture.v.reserve(points.size());
		for(const fw::Pt& point:points) fixture.v.push_back(point);
		return fixture;
	}

	static bool overlap(double ax, double ay, double ahw, double ahh,
		double bx, double by, double bhw, double bhh) {
		return std::abs(ax - bx) <= ahw + bhw && std::abs(ay - by) <= ahh + bhh;
	}

	void updatePlatformPositions(double dt) {
		for (PlatformRuntime& p : platforms_) {
			const double old_x=p.x,old_y=p.y;
			const bool active=groups_[p.group]; const double tx=active?p.end_x:p.start_x, ty=active?p.end_y:p.start_y;
			const double max_step=180.0*dt; const double dx=tx-p.x,dy=ty-p.y,dist=std::hypot(dx,dy);
			if(dist>1e-6) { const double ratio=std::min(1.0,max_step/dist); p.x+=dx*ratio; p.y+=dy*ratio; }
			double ox=old_x,oy=old_y,ow=p.w,oh=p.h;
			if(ow>=oh) { oy+=oh*0.5-32.0; oh=32.0; }
			else { ox+=p.h*0.5-16.0; oy+=p.w*0.5-p.h*0.5; ow=32.0; oh=p.h; }
			for(auto& entry:bodies_) { PhysicsBody& body=entry.second; const bool standing=body.grounded && std::abs(body.y+body.collision_offset_y+body.half_height-oy)<5.0 && body.x+body.half_width>ox && body.x-body.half_width<ox+ow; if(standing) { body.x+=p.x-old_x; body.y+=p.y-old_y; } }
		}
		rebuildCollision(true);
	}

	void rebuildCollision(bool include_boxes) {
		std::vector<fw::GroundFixture> collision=base_terrain_;
		for(const PlatformRuntime& p:platforms_) {
			double x=p.x,y=p.y,w=p.w,h=p.h;
			if(w>=h) { y+=h*0.5-32.0; h=32.0; }
			else { x+=p.h*0.5-16.0; y+=p.w*0.5-p.h*0.5; w=32.0; }
			auto f=makeFixture({{x,y},{x+w,y},{x+w,y+h},{x,y+h}}); collision.push_back(std::move(f));
		}
		if(include_boxes) for(const BoxRuntime& b:boxes_) collision.push_back(fixtureFor(b));
		physics_.setTerrain(collision);
	}

	// Collision shape other bodies see for a pushable object. A ball becomes a regular 12-gon whose edges are tangent
	// to the circle (flats at top, bottom and both sides), so a character can stand on it without sinking in.
	static fw::GroundFixture fixtureFor(const BoxRuntime& b) {
		const PhysicsBody& body=b.body;
		if(body.radius<=0.0) return makeFixture({{body.x-body.half_width,body.y-body.half_height},{body.x+body.half_width,body.y-body.half_height},{body.x+body.half_width,body.y+body.half_height},{body.x-body.half_width,body.y+body.half_height}});
		constexpr int sides=12; const double pi=std::acos(-1.0), circumradius=body.radius/std::cos(pi/sides);
		fw::GroundFixture fixture; fixture.v.reserve(sides);
		for(int i=0;i<sides;++i) { const double a=(i+0.5)*2.0*pi/sides; fixture.v.push_back({body.x+circumradius*std::cos(a),body.y+circumradius*std::sin(a)}); }
		return fixture;
	}

	void rebuildCollisionExceptBox(int excluded_id) {
		std::vector<fw::GroundFixture> collision=base_terrain_;
		for(const PlatformRuntime& p:platforms_) { double x=p.x,y=p.y,w=p.w,h=p.h; if(w>=h) { y+=h*0.5-32.0; h=32.0; } else { x+=p.h*0.5-16.0; y+=p.w*0.5-p.h*0.5; w=32.0; } auto f=makeFixture({{x,y},{x+w,y},{x+w,y+h},{x,y+h}}); collision.push_back(std::move(f)); }
		for(const BoxRuntime& b:boxes_) if(b.id!=excluded_id) collision.push_back(fixtureFor(b));
		physics_.setTerrain(collision);
	}

	void updateBoxes(const GameWorldInput& input,double dt) {
		for(BoxRuntime& box:boxes_) {
			rebuildCollisionExceptBox(box.id);
			const bool ball=box.type=="ball";
			PhysicsInput push;
			for(const auto& entry:bodies_) {
				const PhysicsBody& player=entry.second;
				const PhysicsInput& keys=entry.first=="fb"?input.fireboy:input.watergirl;
				if(std::abs(player.y+player.collision_offset_y-box.body.y)<player.half_height+box.body.half_height &&
					std::abs(player.x-box.body.x)<player.half_width+box.body.half_width+5.0) {
					if(keys.left!=keys.right) {
						// A ball is only pushed away from the character that is pressing towards it.
						if(ball && ((keys.right && player.x>box.body.x) || (keys.left && player.x<box.body.x))) continue;
						if(keys.left) push.left=true; else push.right=true;
					}
				}
			}
			PhysicsTuning tuning; tuning.gravity=1000.0; tuning.run_acceleration=1100.0; tuning.air_acceleration=200.0; tuning.max_speed=150.0; tuning.jump_speed=0.0; tuning.horizontal_drag=10.0;
			if(ball) {
				// Rolling ball: no drag and no speed cap (Box2D default damping is zero); a character can only push it
				// up to a walking pace, so the push is dropped once the ball is already rolling that fast that way.
				constexpr double max_push_speed=200.0;
				if((push.right && box.body.vx>=max_push_speed) || (push.left && box.body.vx<=-max_push_speed)) push=PhysicsInput{};
				tuning.run_acceleration=900.0; tuning.air_acceleration=0.0; tuning.max_speed=1000.0; tuning.horizontal_drag=0.0;
			}
			physics_.step(box.body,push,dt,tuning);
			if(ball) box.angle_deg+=box.body.vx*std::min(dt,0.05)/box.body.radius*(180.0/std::acos(-1.0));
		}
	}

	void updateLeverContacts() {
		std::set<int> lever_now, pusher_now;
		for(const fw::MapObject& object : session_.level().objects) {
			if(object.type!="lever" && object.type!="pusher") continue;
			const double cx=object.x+32.0,cy=object.y-32.0;
			for(const auto& entry:bodies_) {
				const PhysicsBody& b=entry.second;
				if(overlap(b.x,b.y,b.half_width,b.half_height,cx,cy,24.0,28.0)) { (object.type=="lever"?lever_now:pusher_now).insert(object.id); break; }
			}
		}
		for(int id:lever_now) if(!trigger_contacts_.count(id)) lever_states_[id]=!lever_states_[id];
		trigger_contacts_=std::move(lever_now); pusher_contacts_=std::move(pusher_now);
		for(auto& group:groups_) group.second=false;
		std::set<int> signal_groups;
		for(const fw::MapObject& object:session_.level().objects) {
			const int group=object.props["group"].i32();
			if(object.type=="lever") { signal_groups.insert(group); if(lever_states_[object.id]) groups_[group]=true; }
			if(object.type=="pusher") { signal_groups.insert(group); if(pusher_initial_states_[object.id] || pusher_contacts_.count(object.id)) groups_[group]=true; }
		}
		for(const fw::MapObject& object:session_.level().objects) if(object.type=="wind") {
			const int group=object.props["group"].i32();
			if(!signal_groups.count(group) && object.props["initialState"].i32()!=0) groups_[group]=true;
		}
	}

	void stepCharacter(const std::string& id, const PhysicsInput& input, double dt,
		PhysicsStepResult& physics_result, int& newly_collected) {
		PhysicsBody* body_ptr = findBody(id);
		CharacterRuntime* character = session_.gameplay().character(id);
		if (!body_ptr || !character || character->dying || character->dead) return;
		PhysicsTuning tuning;
		if(character->on_ice) { tuning.horizontal_drag=1.0; if(id=="wg") tuning.max_speed=64.0; }
		PhysicsInput effective_input=input;
		effective_input.jump=input.jump && !jump_was_down_[id];
		jump_was_down_[id]=input.jump;
		physics_result = physics_.step(*body_ptr, effective_input, dt, tuning);
		for (const fw::MapObject& object:session_.level().objects) if(object.type=="wind") {
			const int group=object.props["group"].i32(); const bool invert=object.props["invertInput"].boolean(false);
			const bool active=invert?!groups_[group]:groups_[group];
			const double cx=object.x+32.0, base_y=object.y-32.0;
			const double length=std::max(1.0,object.props["length"].num(10.0))*32.0;
			const double width=std::max(1.0,object.props["windWidth"].num(2.6))*16.0;
			const double cy=base_y-length*0.5;
			if(active && overlap(body_ptr->x,body_ptr->y+body_ptr->collision_offset_y,body_ptr->half_width,body_ptr->half_height,cx,cy,width,length*0.5)) {
				const double strength=std::clamp(1.0-std::abs(body_ptr->y-base_y)/length,0.1,1.0);
				body_ptr->vy=std::max(-700.0,body_ptr->vy-500.0*strength*dt);
			}
		}
		for (const DiamondRuntime& diamond : session_.gameplay().diamonds()) {
			if (diamond.collected) continue;
			if (overlap(body_ptr->x, body_ptr->y, body_ptr->half_width, body_ptr->half_height,
				diamond.sensor_center_px.x, diamond.sensor_center_px.y, 13.0, 13.0)) {
				bool collected = false;
				if (session_.gameplay().contactDiamond(diamond.map_object_id, id, true, &collected))
					newly_collected += collected ? 1 : 0;
			}
		}
		for (const DoorRuntime& door : session_.gameplay().doors()) {
			const bool inside = overlap(body_ptr->x, body_ptr->y, body_ptr->half_width, body_ptr->half_height,
				door.sensor_center_px.x, door.sensor_center_px.y, 32.0, 8.0);
			if (door.required_character == id && inside != door.occupied)
				session_.gameplay().contactDoor(door.map_object_id, id, inside);
		}
		const auto& liquids = session_.gameplay().liquids();
		auto& contacts = liquid_contacts_[id]; std::set<int> now;
		auto& ice_contacts = ice_contacts_[id]; std::set<int> ice_now;
		for (size_t i = 0; i < liquids.size(); ++i) {
			const LiquidRuntime& liquid = liquids[i];
			const double x = (liquid.tile_x + liquid.width_tiles * 0.5) * 32.0;
			const double y = (liquid.tile_y + 0.5) * 32.0;
			const bool touching = overlap(body_ptr->x, body_ptr->y + body_ptr->collision_offset_y + body_ptr->half_height * 0.55,
				body_ptr->half_width * 0.7, body_ptr->half_height * 0.45, x, y, liquid.width_tiles * 16.0, 16.0);
			if(liquid.frozen) { if(touching) { ice_now.insert(static_cast<int>(i)); if(!ice_contacts.count(static_cast<int>(i))) session_.gameplay().contactIceSurface(id,static_cast<int>(i),true); } }
			else if(touching) { now.insert(static_cast<int>(i)); if(!contacts.count(static_cast<int>(i))) session_.gameplay().contactLiquidDeath(id,static_cast<int>(i),true); }
		}
		for (int old : contacts) if (!now.count(old)) session_.gameplay().contactLiquidDeath(id, old, false);
		for (int old : ice_contacts) if (!ice_now.count(old)) session_.gameplay().contactIceSurface(id, old, false);
		contacts = std::move(now); ice_contacts = std::move(ice_now);
	}

	PhysicsBody* findBody(const std::string& id) {
		auto it = bodies_.find(id);
		return it == bodies_.end() ? nullptr : &it->second;
	}
};

} // namespace fbw
