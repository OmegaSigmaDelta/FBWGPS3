#include "sdl_abi.hpp"
#include "atlas.hpp"
#include "character_animation.hpp"
#include "game_world.hpp"
#include "progress_save.hpp"
#include "progression_catalog.hpp"
#include "software_renderer.hpp"
#include "world_render.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <array>
#include <iomanip>
#include <sstream>

namespace {
constexpr int SCREEN_W = 960, SCREEN_H = 540;
constexpr int SCANCODE_A=4, SCANCODE_D=7, SCANCODE_W=26;
constexpr int SCANCODE_SPACE=44, SCANCODE_LEFT=80, SCANCODE_RIGHT=79, SCANCODE_UP=82, SCANCODE_DOWN=81;
constexpr int SCANCODE_ESCAPE=41, SCANCODE_ENTER=40, SCANCODE_R=21, SCANCODE_M=16, SCANCODE_TAB=43;
constexpr int PAD_AXIS_LEFT_X=0, PAD_BUTTON_A=0, PAD_BUTTON_B=1, PAD_BUTTON_X=2, PAD_BUTTON_Y=3, PAD_BUTTON_START=6, PAD_BUTTON_DPAD_UP=11, PAD_BUTTON_DPAD_DOWN=12, PAD_BUTTON_DPAD_LEFT=13, PAD_BUTTON_DPAD_RIGHT=14;

struct AppAssets {
	fbw::Atlas ground, chars, mech, temple;
	std::map<std::string, fbw::ImageRGBA> images;
	SDL_Texture* level_texture = nullptr;
	SDL_Texture* character_texture = nullptr;
	SDL_Texture* ground_texture = nullptr;
	SDL_Texture* mech_texture = nullptr;
	SDL_Texture* temple_texture = nullptr;
	SDL_Texture* dark_texture = nullptr;
	int level_w = 0, level_h = 0;
};

bool makeLevelTexture(SDL_Renderer* renderer, const std::string& game_dir,
	const fw::progression::CatalogLevel& catalog_level, AppAssets& assets, std::string& error) {
	const fw::Level level = fw::loadLevel(game_dir, catalog_level.filename);
	const fw::GroundResult ground = fw::buildGround(level);
	fbw::RenderQueue queue;
	if (!queue.appendGround(ground, assets.ground, "GroundAssets", level.tileW, level.tileH, 1234, &error)) return false;
	fbw::WorldRenderStats stats;
	auto region = [](const fbw::Atlas& atlas, const std::string& key, const std::string& name) {
		const fbw::AtlasFrame* f = atlas.find(name);
		if (!f) throw std::runtime_error("missing frame: " + name);
		return fbw::TextureRegion{key, {f->x,f->y,f->w,f->h}, f->source_w,f->source_h,f->original_w,f->original_h};
	};
	const auto bg = region(assets.temple,"TempleAssets","BackGround0000");
	const auto fg = region(assets.temple,"TempleAssets","ForeGround0000");
	fbw::ImageRGBA pixels;
	fbw::SoftwareRenderer software;
	if (!software.renderTexturedLevel(queue, assets.images, bg, fg, level.w*level.tileW, level.h*level.tileH, pixels, &error)) return false;
	if (assets.level_texture) SDL_DestroyTexture(assets.level_texture);
	assets.level_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, 0, pixels.width, pixels.height);
	if (!assets.level_texture) { error = SDL_GetError(); return false; }
	if (SDL_UpdateTexture(assets.level_texture, nullptr, pixels.pixels.data(), pixels.width*4) != 0) { error = SDL_GetError(); return false; }
	assets.level_w = pixels.width; assets.level_h = pixels.height;
	return true;
}

void drawText(SDL_Renderer* renderer, int x, int y, const std::string& text, int scale=2) {
	static const std::map<char,std::array<uint8_t,7>> glyphs = {
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
		{'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,12,12}},{' ',{0,0,0,0,0,0,0}}
	};
	for (char c : text) {
		auto it=glyphs.find(c); if(it!=glyphs.end()) for(int row=0;row<7;++row) for(int col=0;col<5;++col) if(it->second[row]&(1u<<(4-col))) { SDL_Rect px{x+col*scale,y+row*scale,scale,scale}; SDL_RenderFillRect(renderer,&px); }
		x += 6*scale;
	}
}

bool loadAssets(const std::string& game_dir, AppAssets& a, std::string& error) {
	const std::string dir = game_dir + "/assets/atlasses/";
	if (!a.ground.load(dir+"GroundAssets.json",&error) || !a.chars.load(dir+"CharAssets.json",&error) ||
		!a.mech.load(dir+"MechAssets.json",&error) || !a.temple.load(dir+"Temples/forest/TempleAssets.json",&error)) return false;
	for (const auto& p : std::vector<std::pair<std::string,std::string>>{
		{"GroundAssets",dir+"GroundAssets.png"},{"CharAssets",dir+"CharAssets.png"},
		{"MechAssets",dir+"MechAssets.png"},{"TempleAssets",dir+"Temples/forest/TempleAssets.png"}}) {
		fbw::ImageRGBA image;
		if (!image.loadPng(p.second,&error)) return false;
		a.images.emplace(p.first,std::move(image));
	}
	return true;
}
}

int main(int argc, char** argv) {
	const std::string game_dir = argc > 1 ? argv[1] : "game";
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER) != 0) { std::cerr << "SDL init failed: " << SDL_GetError() << "\n"; return 1; }
	SDL_Window* window = SDL_CreateWindow("Fireboy & Watergirl — native port prototype", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
	if (!window) { std::cerr << SDL_GetError() << "\n"; SDL_Quit(); return 1; }
	SDL_Renderer* renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) renderer = SDL_CreateRenderer(window,-1,0);
	if (!renderer) { std::cerr << SDL_GetError() << "\n"; SDL_DestroyWindow(window); SDL_Quit(); return 1; }
	SDL_GameController* controller=nullptr; SDL_GameController* controller2=nullptr;
	for(int i=0;i<SDL_NumJoysticks();++i) if(SDL_IsGameController(i)) { SDL_GameController* opened=SDL_GameControllerOpen(i); if(opened) { if(!controller) controller=opened; else { controller2=opened; break; } } }
	AppAssets assets; std::string error;
	try {
		if (!loadAssets(game_dir,assets,error)) throw std::runtime_error(error);
		const auto& ground_image = assets.images.at("GroundAssets");
		assets.ground_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, 0, ground_image.width, ground_image.height);
		if (!assets.ground_texture || SDL_UpdateTexture(assets.ground_texture,nullptr,ground_image.pixels.data(),ground_image.width*4)!=0) throw std::runtime_error(SDL_GetError());
		SDL_SetTextureBlendMode(assets.ground_texture,SDL_BLENDMODE_BLEND);
		const auto& mech_image = assets.images.at("MechAssets");
		assets.mech_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, 0, mech_image.width, mech_image.height);
		if (!assets.mech_texture || SDL_UpdateTexture(assets.mech_texture,nullptr,mech_image.pixels.data(),mech_image.width*4)!=0) throw std::runtime_error(SDL_GetError());
		SDL_SetTextureBlendMode(assets.mech_texture,SDL_BLENDMODE_BLEND);
		const auto& temple_image = assets.images.at("TempleAssets");
		assets.temple_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, 0, temple_image.width, temple_image.height);
		if (!assets.temple_texture || SDL_UpdateTexture(assets.temple_texture,nullptr,temple_image.pixels.data(),temple_image.width*4)!=0) throw std::runtime_error(SDL_GetError());
		SDL_SetTextureBlendMode(assets.temple_texture,SDL_BLENDMODE_BLEND);
		const auto& char_image = assets.images.at("CharAssets");
		assets.character_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, 0, char_image.width, char_image.height);
		if (!assets.character_texture || SDL_UpdateTexture(assets.character_texture,nullptr,char_image.pixels.data(),char_image.width*4)!=0) throw std::runtime_error(SDL_GetError());
		SDL_SetTextureBlendMode(assets.character_texture,SDL_BLENDMODE_BLEND);
		assets.dark_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,0,SCREEN_W,SCREEN_H);
		if(!assets.dark_texture) throw std::runtime_error(SDL_GetError());
		SDL_SetTextureBlendMode(assets.dark_texture,SDL_BLENDMODE_BLEND);
		std::vector<uint8_t> dark_pixels(SCREEN_W*SCREEN_H*4,0);
		fw::progression::ForestCatalog catalog;
		if (!fw::progression::loadForestCatalog(game_dir,catalog,&error)) throw std::runtime_error(error);
		auto first = std::find_if(catalog.levels.begin(),catalog.levels.end(),[](const auto& l){return l.initial;});
		if (first == catalog.levels.end()) throw std::runtime_error("no initial level in catalogue");
		size_t level_index=static_cast<size_t>(std::distance(catalog.levels.begin(),first));
		const fw::progression::CatalogLevel* current_level=&catalog.levels[level_index];
		fw::progression::Progress progress;
		const std::string save_path = "fbw_progress.json";
		fw::progression::loadProgressFile(save_path,catalog.id,progress,nullptr);
		fbw::GameWorld world;
		if (!world.start(game_dir,*current_level,false,progress,&error)) throw std::runtime_error(error);
		if (!makeLevelTexture(renderer,game_dir,*current_level,assets,error)) throw std::runtime_error(error);
		if (argc > 2 && (std::string(argv[2]) == "--validate" || std::string(argv[2]) == "--validate-all")) {
			const bool all_levels = std::string(argv[2]) == "--validate-all";
			if (all_levels) {
				for (const auto& candidate : catalog.levels) {
					if (!world.start(game_dir,candidate,false,progress,&error) || !makeLevelTexture(renderer,game_dir,candidate,assets,error))
						throw std::runtime_error("level " + std::to_string(candidate.rules.id) + ": " + error);
					fbw::RenderQueue verify_queue; fbw::WorldRenderStats verify_stats;
					if(!fbw::appendWorldObjects(world.session().level(),assets.chars,assets.mech,assets.temple,verify_queue,&verify_stats,&error)) throw std::runtime_error("level objects " + std::to_string(candidate.rules.id) + ": " + error);
				}
				std::cout << "desktop runtime validation OK; levels=" << catalog.levels.size() << "\n";
			} else std::cout << "desktop runtime startup OK; level=" << current_level->rules.id << ", size=" << assets.level_w << "x" << assets.level_h << ", diamonds=" << world.session().gameplay().totalDiamonds() << "\n";
			if (assets.level_texture) SDL_DestroyTexture(assets.level_texture);
			if (assets.character_texture) SDL_DestroyTexture(assets.character_texture);
			if (assets.ground_texture) SDL_DestroyTexture(assets.ground_texture);
			if (assets.mech_texture) SDL_DestroyTexture(assets.mech_texture);
			if (assets.temple_texture) SDL_DestroyTexture(assets.temple_texture);
			if (assets.dark_texture) SDL_DestroyTexture(assets.dark_texture);
			SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); if(controller) SDL_GameControllerClose(controller); if(controller2) SDL_GameControllerClose(controller2); SDL_Quit(); return 0;
		}
		bool running=true, paused=false, in_menu=true, outcome_saved=false, solo_mode=false; std::string solo_active="fb"; fbw::GameWorldInput last_input; bool pad_up_prev=false,pad_down_prev=false,pad_a_prev=false,pad_b_prev=false,pad_x_prev=false,pad_y_prev=false,pad_start_prev=false; int view_x=0, view_y=0;
		auto previous=std::chrono::steady_clock::now();
		while (running) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type==SDL_QUIT) running=false;
				if (event.type==SDL_KEYDOWN && !event.key.repeat) {
					const int key=event.key.keysym.scancode;
					if (in_menu) {
						if (key==SCANCODE_UP) level_index=(level_index+catalog.levels.size()-1)%catalog.levels.size();
						if (key==SCANCODE_DOWN) level_index=(level_index+1)%catalog.levels.size();
						if (key==SCANCODE_TAB) { solo_mode=!solo_mode; solo_active="fb"; }
						current_level=&catalog.levels[level_index];
						if (key==SCANCODE_ENTER) {
							const auto menu_state=fw::progression::calculateMenuProgress(catalog,progress);
							const bool locked=level_index<menu_state.levels.size() && menu_state.levels[level_index].state==0;
							if (!locked && world.start(game_dir,*current_level,solo_mode,progress,&error) && makeLevelTexture(renderer,game_dir,*current_level,assets,error)) { in_menu=false; paused=false; outcome_saved=false; }
						}
						if (key==SCANCODE_ESCAPE) running=false;
					} else {
						if (key==SCANCODE_ESCAPE) { if (paused) running=false; else paused=true; }
						if (key==SCANCODE_M) { in_menu=true; paused=false; fw::progression::saveProgressFile(save_path,catalog.id,progress,nullptr); }
						if (key==SCANCODE_TAB && solo_mode) solo_active=solo_active=="fb"?"wg":"fb";
						if (key==SCANCODE_ENTER && paused) paused=false;
						if (key==SCANCODE_ENTER && !paused && world.session().gameplay().outcome()!=fbw::LevelOutcome::Running) {
							if (world.session().gameplay().outcome()==fbw::LevelOutcome::Won) {
								const auto menu_state=fw::progression::calculateMenuProgress(catalog,progress); bool found_next=false;
								for(size_t offset=1;offset<catalog.levels.size();++offset) { const size_t candidate=(level_index+offset)%catalog.levels.size(); if(menu_state.levels[candidate].state>0) { level_index=candidate; found_next=true; break; } }
								if(!found_next) in_menu=true;
							} else in_menu=true;
							current_level=&catalog.levels[level_index];
							if (!in_menu && world.start(game_dir,*current_level,solo_mode,progress,&error) && makeLevelTexture(renderer,game_dir,*current_level,assets,error)) outcome_saved=false;
						}
						if (key==SCANCODE_R) { if (world.start(game_dir,*current_level,solo_mode,progress,&error)) { outcome_saved=false; makeLevelTexture(renderer,game_dir,*current_level,assets,error); } }
					}
				}
			}
			const uint8_t* keys=SDL_GetKeyboardState(nullptr);
			if(controller) {
				const bool pad_up=SDL_GameControllerGetButton(controller,PAD_BUTTON_DPAD_UP)!=0;
				const bool pad_down=SDL_GameControllerGetButton(controller,PAD_BUTTON_DPAD_DOWN)!=0;
				const bool pad_a=SDL_GameControllerGetButton(controller,PAD_BUTTON_A)!=0;
				const bool pad_start=SDL_GameControllerGetButton(controller,PAD_BUTTON_START)!=0;
				const bool pad_b=SDL_GameControllerGetButton(controller,PAD_BUTTON_B)!=0;
				const bool pad_x=SDL_GameControllerGetButton(controller,PAD_BUTTON_X)!=0;
				const bool pad_y=SDL_GameControllerGetButton(controller,PAD_BUTTON_Y)!=0;
				if(in_menu && pad_x && !pad_x_prev) { solo_mode=!solo_mode; solo_active="fb"; }
				if(!in_menu && solo_mode && pad_y && !pad_y_prev) solo_active=solo_active=="fb"?"wg":"fb";
				if(in_menu && pad_b && !pad_b_prev) running=false;
				if(!in_menu && pad_b && !pad_b_prev) { in_menu=true; paused=false; fw::progression::saveProgressFile(save_path,catalog.id,progress,nullptr); }
				if(in_menu && pad_up && !pad_up_prev) level_index=(level_index+catalog.levels.size()-1)%catalog.levels.size();
				if(in_menu && pad_down && !pad_down_prev) level_index=(level_index+1)%catalog.levels.size();
				current_level=&catalog.levels[level_index];
				if(in_menu && pad_a && !pad_a_prev) { const auto state=fw::progression::calculateMenuProgress(catalog,progress); if(state.levels[level_index].state>0 && world.start(game_dir,*current_level,solo_mode,progress,&error) && makeLevelTexture(renderer,game_dir,*current_level,assets,error)) { in_menu=false; paused=false; outcome_saved=false; } }
				if(!in_menu && pad_start && !pad_start_prev) paused=!paused;
				pad_up_prev=pad_up; pad_down_prev=pad_down; pad_a_prev=pad_a; pad_b_prev=pad_b; pad_x_prev=pad_x; pad_y_prev=pad_y; pad_start_prev=pad_start;
			}
			auto now=std::chrono::steady_clock::now();
			double dt=std::chrono::duration<double>(now-previous).count(); previous=now;
			if (!in_menu && !paused && world.session().gameplay().outcome()==fbw::LevelOutcome::Running) {
				fbw::GameWorldInput input;
				const bool solo=solo_mode;
				if(!solo || solo_active=="fb") { input.fireboy.left=keys[SCANCODE_LEFT]; input.fireboy.right=keys[SCANCODE_RIGHT]; input.fireboy.jump=keys[SCANCODE_UP]||keys[SCANCODE_SPACE]; }
				if(!solo) { input.watergirl.left=keys[SCANCODE_A]; input.watergirl.right=keys[SCANCODE_D]; input.watergirl.jump=keys[SCANCODE_W]; }
				if(solo && solo_active=="wg") { input.watergirl.left=keys[SCANCODE_LEFT]; input.watergirl.right=keys[SCANCODE_RIGHT]; input.watergirl.jump=keys[SCANCODE_UP]||keys[SCANCODE_SPACE]; }
				if(controller) { const int axis=SDL_GameControllerGetAxis(controller,PAD_AXIS_LEFT_X); const bool l=axis < -9000||SDL_GameControllerGetButton(controller,PAD_BUTTON_DPAD_LEFT), r=axis > 9000||SDL_GameControllerGetButton(controller,PAD_BUTTON_DPAD_RIGHT), j=SDL_GameControllerGetButton(controller,PAD_BUTTON_A)||SDL_GameControllerGetButton(controller,PAD_BUTTON_DPAD_UP); if(solo&&solo_active=="wg") { input.watergirl.left=input.watergirl.left||l; input.watergirl.right=input.watergirl.right||r; input.watergirl.jump=input.watergirl.jump||j; } else { input.fireboy.left=input.fireboy.left||l; input.fireboy.right=input.fireboy.right||r; input.fireboy.jump=input.fireboy.jump||j; } }
				if(controller2 && !solo) { const int axis=SDL_GameControllerGetAxis(controller2,PAD_AXIS_LEFT_X); input.watergirl.left=input.watergirl.left||axis < -9000||SDL_GameControllerGetButton(controller2,PAD_BUTTON_DPAD_LEFT); input.watergirl.right=input.watergirl.right||axis > 9000||SDL_GameControllerGetButton(controller2,PAD_BUTTON_DPAD_RIGHT); input.watergirl.jump=input.watergirl.jump||SDL_GameControllerGetButton(controller2,PAD_BUTTON_A)||SDL_GameControllerGetButton(controller2,PAD_BUTTON_DPAD_UP); }
				last_input=input; world.update(input,dt);
			}
			if (world.session().gameplay().outcome()!=fbw::LevelOutcome::Running && !outcome_saved) { fw::progression::saveProgressFile(save_path,catalog.id,progress,nullptr); outcome_saved=true; }
			const fbw::PhysicsBody* fb=world.body("fb"); const fbw::PhysicsBody* wg=world.body("wg");
			// In solo mode, follow the currently controlled character. Averaging both
			// bodies can leave the active player off-screen when switching heroes.
			const fbw::PhysicsBody* focus_a = solo_mode ? world.body(solo_active) : fb;
			const fbw::PhysicsBody* focus_b = solo_mode ? nullptr : wg;
			const double focus_x=(focus_a&&focus_b)?(focus_a->x+focus_b->x)*0.5:(focus_a?focus_a->x:(fb?fb->x:(wg?wg->x:SCREEN_W/2)));
			const double focus_y=(focus_a&&focus_b)?(focus_a->y+focus_b->y)*0.5:(focus_a?focus_a->y:(fb?fb->y:(wg?wg->y:SCREEN_H/2)));
			view_x=std::clamp(static_cast<int>(focus_x-SCREEN_W/2),0,std::max(0,assets.level_w-SCREEN_W));
			view_y=std::clamp(static_cast<int>(focus_y-SCREEN_H/2),0,std::max(0,assets.level_h-SCREEN_H));
			SDL_SetRenderDrawColor(renderer,15,22,29,255); SDL_RenderClear(renderer);
			if (in_menu) {
				SDL_SetRenderDrawColor(renderer,28,43,54,255); SDL_Rect panel{SCREEN_W/2-260,55,520,430}; SDL_RenderFillRect(renderer,&panel);
				SDL_SetRenderDrawColor(renderer,240,240,240,255); SDL_RenderDrawRect(renderer,&panel);
				drawText(renderer,SCREEN_W/2-210,82,"FIREBOY AND WATERGIRL",3);
				drawText(renderer,SCREEN_W/2-132,130,"FOREST TEMPLE",3);
				drawText(renderer,SCREEN_W/2-150,205,"SELECT LEVEL",2);
				SDL_SetRenderDrawColor(renderer,70,95,110,255); SDL_Rect choice{SCREEN_W/2-100,242,200,46}; SDL_RenderFillRect(renderer,&choice);
				SDL_SetRenderDrawColor(renderer,255,255,255,255); SDL_RenderDrawRect(renderer,&choice);
				drawText(renderer,SCREEN_W/2-42,258,"LEVEL",2); drawText(renderer,SCREEN_W/2+42,258,std::to_string(current_level->rules.id),2);
				const auto menu_state=fw::progression::calculateMenuProgress(catalog,progress);
				if(level_index<menu_state.levels.size() && menu_state.levels[level_index].state==0) drawText(renderer,SCREEN_W/2-42,300,"LOCKED",2);
				else drawText(renderer,SCREEN_W/2-42,300,"UNLOCKED",2);
				drawText(renderer,SCREEN_W/2-205,320,"UP DOWN SELECT  ENTER PLAY",2);
				drawText(renderer,SCREEN_W/2-205,352,"FIREBOY ARROWS SPACE",2);
				drawText(renderer,SCREEN_W/2-205,382,"WATERGIRL A D W",2);
				drawText(renderer,SCREEN_W/2-205,398,solo_mode?"MODE SOLO":"MODE TWO PLAYER",2);
				if(solo_mode) drawText(renderer,SCREEN_W/2-205,410,"PAD Y OR TAB SWITCH HERO",1);
				drawText(renderer,SCREEN_W/2-205,422,"TAB OR PAD X CHANGE MODE",2);
				drawText(renderer,SCREEN_W/2-205,442,"ESC OR PAD B QUIT",2);
				drawText(renderer,SCREEN_W/2-205,462,"TOTAL STARS",2); drawText(renderer,SCREEN_W/2+70,462,std::to_string(fw::progression::calculateMenuProgress(catalog,progress).total_stars),2);
			} else {
			SDL_Rect src{view_x,view_y,std::min(SCREEN_W,assets.level_w),std::min(SCREEN_H,assets.level_h)};
			SDL_Rect dst{0,0,SCREEN_W,SCREEN_H}; SDL_RenderCopy(renderer,assets.level_texture,&src,&dst);
			fw::Level visual_level=world.session().level();
			for(const auto& platform:world.platforms()) for(auto& object:visual_level.objects) if(object.id==platform.id&&object.type=="platform") { object.x=platform.x; object.y=platform.y; }
			for(const auto& box:world.movableObjects()) for(auto& object:visual_level.objects) if(object.id==box.id&&object.type==box.type) { object.x=box.body.x-32.0; object.y=box.body.y+32.0; }
			visual_level.chars.erase(std::remove_if(visual_level.chars.begin(),visual_level.chars.end(),[&](const fw::MapObject& object){ if(object.type!="diamond") return false; for(const auto& diamond:world.session().gameplay().diamonds()) if(diamond.map_object_id==object.id&&diamond.collected) return true; return false; }),visual_level.chars.end());
			fbw::RenderQueue object_queue; fbw::WorldRenderStats object_stats;
			if(fbw::appendWorldObjects(visual_level,assets.chars,assets.mech,assets.temple,object_queue,&object_stats,nullptr)) {
				for(const auto& command:object_queue.ordered()) {
					if(command.layer==fbw::RenderLayer::Characters || command.frame_name.rfind("FinishBoy",0)==0 || command.frame_name.rfind("FinishGirl",0)==0) continue;
					SDL_Texture* texture=command.atlas_key=="CharAssets"?assets.character_texture:(command.atlas_key=="MechAssets"?assets.mech_texture:(command.atlas_key=="TempleAssets"?assets.temple_texture:assets.ground_texture));
					SDL_Rect sr{command.source.x,command.source.y,command.source.w,command.source.h};
					SDL_Rect dr{static_cast<int>(command.x-view_x),static_cast<int>(command.y-view_y),std::max(1,static_cast<int>(command.w)),std::max(1,static_cast<int>(command.h))};
					SDL_Point pivot{static_cast<int>(command.pivot_x-command.x),static_cast<int>(command.pivot_y-command.y)};
					int flip=(command.flip_x?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE)|(command.flip_y?SDL_FLIP_VERTICAL:SDL_FLIP_NONE);
					SDL_RenderCopyEx(renderer,texture,&sr,&dr,command.rotation_degrees+(command.rotated_in_atlas?90.0:0.0),&pivot,flip);
				}
			}
			auto frame_suffix=[](int n){ std::string s=std::to_string(n); while(s.size()<4) s.insert(s.begin(),'0'); return s; };
			for(const auto& door:world.session().gameplay().doors()) {
				const fw::MapObject* object=nullptr; for(const auto& candidate:world.session().level().chars) if(candidate.id==door.map_object_id) { object=&candidate; break; }
				if(!object) continue;
				const std::string prefix=door.required_character=="wg"?"FinishGirl":"FinishBoy";
				const std::string frame_name=prefix+frame_suffix(door.displayed_frame); const auto* f=assets.temple.find(frame_name); if(!f) continue;
				const double scale=0.8,left=object->x+32.0,top=object->y+4.0-f->original_h*0.99*scale;
				SDL_Rect sr{f->x,f->y,f->w,f->h}; SDL_Rect dr{static_cast<int>(left-view_x+f->source_x*scale),static_cast<int>(top-view_y+f->source_y*scale),std::max(1,static_cast<int>(f->source_w*scale)),std::max(1,static_cast<int>(f->source_h*scale))};
				SDL_RenderCopyEx(renderer,assets.temple_texture,&sr,&dr,f->rotated?90.0:0.0,nullptr,SDL_FLIP_NONE);
			}
			for(const fw::MapObject& wind:world.session().level().objects) if(wind.type=="wind") {
				const int group=wind.props["group"].i32(); const bool invert=wind.props["invertInput"].boolean(false); const bool active=invert?!world.groupActive(group):world.groupActive(group); if(!active) continue;
				const int frame=static_cast<int>(std::floor(world.session().elapsedSeconds()*30.0))%31; const std::string name="wind_effect"+frame_suffix(frame); const auto* f=assets.mech.find(name); if(!f) continue;
				const double length=std::max(1.0,wind.props["length"].num(10.0))*32.0,width=std::max(1.0,wind.props["windWidth"].num(2.6))*32.0;
				const double cx=wind.x+32.0,cy=wind.y-32.0-length*0.5; SDL_Rect sr{f->x,f->y,f->w,f->h}; SDL_Rect dr{static_cast<int>(cx-view_x-width*0.5),static_cast<int>(cy-view_y-length*0.5),static_cast<int>(width),static_cast<int>(length)};
				SDL_Point center{dr.w/2,dr.h/2}; SDL_RenderCopyEx(renderer,assets.mech_texture,&sr,&dr,wind.rotation+(f->rotated?90.0:0.0),&center,SDL_FLIP_NONE);
			}
			const auto& liquid_defs=world.session().ground().liquids; const auto& liquid_states=world.session().gameplay().liquids();
			const int anim_frame=static_cast<int>(std::floor(world.session().elapsedSeconds()*30.0))%15;
			for(size_t li=0;li<liquid_defs.size();++li) {
				const auto& liquid=liquid_defs[li]; const bool frozen=li<liquid_states.size()&&liquid_states[li].frozen;
				std::string prefix=liquid.type==5||liquid.type==55?"WaterBox":(liquid.type==6?"FireBox":"GreenBox");
				const std::string base=frozen?"IceBox":prefix; const std::string suffix=frozen?"0000":frame_suffix(anim_frame);
				for(int part=0;part<liquid.width;++part) {
					std::string name=base+(part==0?"Left":(part==liquid.width-1?"Right":""))+suffix;
					const auto* f=assets.ground.find(name); if(!f) continue;
					const double cx=(liquid.tx+part+0.5)*32.0,cy=(liquid.ty+0.5)*32.0+(frozen?4.0:2.0);
					SDL_Rect srect{f->x,f->y,f->w,f->h}; SDL_Rect drect{static_cast<int>(cx-view_x-f->original_w/2+f->source_x),static_cast<int>(cy-view_y-f->original_h/2+f->source_y),f->source_w,f->source_h};
					SDL_RenderCopyEx(renderer,assets.ground_texture,&srect,&drect,f->rotated?90.0:0.0,nullptr,SDL_FLIP_NONE);
				}
			}
			if(current_level->rules.type=="dark") {
				for(int py=0;py<SCREEN_H;++py) for(int px=0;px<SCREEN_W;++px) {
					double nearest=10000.0;
					for(const fbw::PhysicsBody* player:{fb,wg}) if(player) { const double dx=px-(player->x-view_x),dy=py-(player->y-view_y); nearest=std::min(nearest,std::hypot(dx,dy)); }
					const double t=std::clamp(nearest/145.0,0.0,1.0); const uint8_t alpha=static_cast<uint8_t>(30.0+190.0*t*t);
					const size_t i=(static_cast<size_t>(py)*SCREEN_W+px)*4; dark_pixels[i]=0; dark_pixels[i+1]=0; dark_pixels[i+2]=0; dark_pixels[i+3]=alpha;
				}
				SDL_UpdateTexture(assets.dark_texture,nullptr,dark_pixels.data(),SCREEN_W*4); SDL_RenderCopy(renderer,assets.dark_texture,nullptr,nullptr);
			}
			auto drawPlayer=[&](const fbw::PhysicsBody* b,const std::string& id,bool left,bool right) {
				if (!b) return;
				fbw::CharacterMovementSample sample; sample.grounded=b->grounded; sample.left_down=left; sample.right_down=right; sample.velocity_x=b->vx; sample.velocity_y=-b->vy; const auto* runtime=world.session().gameplay().character(id); sample.on_ice=runtime&&runtime->on_ice;
				const auto pose=fbw::resolveCharacterPose(sample); fbw::CharacterAnimationFrames frames;
				if (!fbw::selectCharacterAnimationFrames(assets.chars,id,pose,world.session().elapsedSeconds(),frames,nullptr)) return;
				auto blit=[&](const std::string& name,double cx,double cy,double angle,bool flip) {
					const auto* f=assets.chars.find(name); if (!f) return;
					SDL_Rect src{f->x,f->y,f->w,f->h}; const int dw=std::max(1,f->source_w),dh=std::max(1,f->source_h);
					const int offset_x=flip?f->original_w-f->source_x-f->source_w:f->source_x;
					SDL_Rect dst{static_cast<int>(cx-view_x-f->original_w/2+offset_x),static_cast<int>(cy-view_y-f->original_h/2+f->source_y),dw,dh};
					SDL_Point center{dw/2,dh/2}; SDL_RenderCopyEx(renderer,assets.character_texture,&src,&dst,angle+(f->rotated?90.0:0.0),&center,flip?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
				};
				blit(frames.legs,b->x,b->y+13,0,pose.legs_flip_x); blit(frames.head,b->x,b->y+5,pose.head_rotation_degrees,pose.head_flip_x);
			};
			drawPlayer(fb,"fb",last_input.fireboy.left,last_input.fireboy.right); drawPlayer(wg,"wg",last_input.watergirl.left,last_input.watergirl.right);
			SDL_SetRenderDrawColor(renderer,10,15,20,210); SDL_Rect hud{0,0,SCREEN_W,38}; SDL_RenderFillRect(renderer,&hud);
			SDL_SetRenderDrawColor(renderer,255,255,255,255);
			int diamonds=0; for (const auto& d : world.session().gameplay().diamonds()) if (d.collected) ++diamonds;
			std::ostringstream hud_text; hud_text << "FBWGPS3  LEVEL " << current_level->rules.id << "  TIME " << std::fixed << std::setprecision(1) << world.session().elapsedSeconds() << "  DIAMONDS " << diamonds << "/" << world.session().gameplay().totalDiamonds();
			drawText(renderer,12,10,hud_text.str(),2);
			if (paused || world.session().gameplay().outcome()!=fbw::LevelOutcome::Running) {
				SDL_SetRenderDrawColor(renderer,0,0,0,220); SDL_Rect shade{SCREEN_W/2-250,SCREEN_H/2-70,500,140}; SDL_RenderFillRect(renderer,&shade);
				SDL_SetRenderDrawColor(renderer,230,230,230,255); SDL_Rect border{SCREEN_W/2-250,SCREEN_H/2-70,500,140}; SDL_RenderDrawRect(renderer,&border);
				SDL_SetRenderDrawColor(renderer,255,255,255,255);
				if(paused) drawText(renderer,SCREEN_W/2-54,SCREEN_H/2-35,"PAUSED",3);
				else if(world.session().gameplay().outcome()==fbw::LevelOutcome::Won) drawText(renderer,SCREEN_W/2-90,SCREEN_H/2-35,"LEVEL CLEAR",3);
				else drawText(renderer,SCREEN_W/2-45,SCREEN_H/2-35,"GAME OVER",3);
				if(paused) drawText(renderer,SCREEN_W/2-144,SCREEN_H/2+15,"ENTER RESUME  ESC QUIT",2);
				else if(world.session().gameplay().outcome()==fbw::LevelOutcome::Won) {
					drawText(renderer,SCREEN_W/2-180,SCREEN_H/2+8,"ENTER NEXT LEVEL  M MENU",2);
					drawText(renderer,SCREEN_W/2-105,SCREEN_H/2+34,"STARS",2); drawText(renderer,SCREEN_W/2+10,SCREEN_H/2+34,std::to_string(world.session().result().stars)+"/3",2);
				} else drawText(renderer,SCREEN_W/2-180,SCREEN_H/2+15,"R RETRY  M MENU",2);
			}
			}
			SDL_RenderPresent(renderer);
			if (dt<0.001) SDL_Delay(1);
		}
		fw::progression::saveProgressFile(save_path,catalog.id,progress,nullptr);
	} catch (const std::exception& e) { std::cerr << "Game startup failed: " << e.what() << "\n"; }
	if (assets.level_texture) SDL_DestroyTexture(assets.level_texture);
	if (assets.character_texture) SDL_DestroyTexture(assets.character_texture);
	if (assets.ground_texture) SDL_DestroyTexture(assets.ground_texture);
	if (assets.mech_texture) SDL_DestroyTexture(assets.mech_texture);
	if (assets.temple_texture) SDL_DestroyTexture(assets.temple_texture);
	if (assets.dark_texture) SDL_DestroyTexture(assets.dark_texture);
	SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); if(controller) SDL_GameControllerClose(controller); if(controller2) SDL_GameControllerClose(controller2); SDL_Quit();
	return 0;
}
