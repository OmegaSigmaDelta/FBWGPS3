#pragma once
#include <cstdint>
extern "C" {
struct SDL_Window; struct SDL_Renderer; struct SDL_Texture; struct SDL_GameController;
struct SDL_Rect { int x,y,w,h; }; struct SDL_Point { int x,y; };
struct SDL_Keysym { int scancode; int sym; uint16_t mod; uint32_t unused; };
struct SDL_KeyboardEvent { uint32_t type,timestamp,windowID; uint8_t state,repeat,padding2,padding3; SDL_Keysym keysym; };
union SDL_Event { uint32_t type; SDL_KeyboardEvent key; uint8_t padding[56]; };
int SDL_Init(uint32_t flags);
int SDL_NumJoysticks(void);
int SDL_IsGameController(int joystick_index);
SDL_GameController* SDL_GameControllerOpen(int joystick_index);
void SDL_GameControllerClose(SDL_GameController*);
int16_t SDL_GameControllerGetAxis(SDL_GameController*,int axis);
uint8_t SDL_GameControllerGetButton(SDL_GameController*,int button);
void SDL_Quit(void);
const char* SDL_GetError(void);
SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,uint32_t);
void SDL_DestroyWindow(SDL_Window*);
SDL_Renderer* SDL_CreateRenderer(SDL_Window*,int,uint32_t);
void SDL_DestroyRenderer(SDL_Renderer*);
SDL_Texture* SDL_CreateTexture(SDL_Renderer*,uint32_t,int,int,int);
void SDL_DestroyTexture(SDL_Texture*);
int SDL_UpdateTexture(SDL_Texture*,const SDL_Rect*,const void*,int);
int SDL_SetTextureBlendMode(SDL_Texture*,int);
int SDL_PollEvent(SDL_Event*);
const uint8_t* SDL_GetKeyboardState(int*);
int SDL_SetRenderDrawColor(SDL_Renderer*,uint8_t,uint8_t,uint8_t,uint8_t);
int SDL_RenderClear(SDL_Renderer*);
int SDL_RenderCopy(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*);
int SDL_RenderCopyEx(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*,double,const SDL_Point*,int);
int SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect*);
int SDL_RenderDrawRect(SDL_Renderer*,const SDL_Rect*);
void SDL_RenderPresent(SDL_Renderer*);
void SDL_Delay(uint32_t);
}
constexpr uint32_t SDL_INIT_VIDEO = 0x00000020u; constexpr uint32_t SDL_INIT_JOYSTICK=0x00000200u; constexpr uint32_t SDL_INIT_GAMECONTROLLER=0x00002000u;
constexpr uint32_t SDL_WINDOW_SHOWN = 0x00000004u;
constexpr uint32_t SDL_WINDOW_RESIZABLE = 0x00000020u;
constexpr uint32_t SDL_RENDERER_ACCELERATED = 0x00000002u;
constexpr uint32_t SDL_RENDERER_PRESENTVSYNC = 0x00000004u;
constexpr uint32_t SDL_PIXELFORMAT_ABGR8888 = 0x16762004u;
constexpr int SDL_BLENDMODE_BLEND = 1;
constexpr int SDL_FLIP_NONE = 0; constexpr int SDL_FLIP_HORIZONTAL = 1; constexpr int SDL_FLIP_VERTICAL = 2;
constexpr uint32_t SDL_QUIT = 0x100u;
constexpr uint32_t SDL_KEYDOWN = 0x300u;
constexpr int SDL_WINDOWPOS_CENTERED = 0x2FFF0000;
