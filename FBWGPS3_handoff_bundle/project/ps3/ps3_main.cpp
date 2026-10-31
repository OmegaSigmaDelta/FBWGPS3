// PS3 platform layer (PSL1GHT): RSX framebuffer + ioPad + sysutil exit handling around fbw::App.
//
// !!! NOT COMPILED, NOT RUN. !!!  Written without a PSL1GHT SDK or a console available. Every PSL1GHT call below is from
// memory of the SDK samples (rsxtest / pad sample) and MUST be checked against the installed headers on the first real build.
// Places most likely to need adjustment are tagged  VERIFY.  The platform-neutral game (fbw::App) is tested on Linux;
// this file is only glue: pads -> fw::Input, App::render() frame -> framebuffer, exit callback.
//
// Design choice: no RSX 3D pipeline. fbw::App renders each frame on the CPU (RGBA); this file copies it into a
// 1280x720 XRGB display buffer and flips. Simple and robust; speed on the Cell PPU is UNMEASURED (the Linux x86
// frame composer needs ~35 ms/level of compose time in the 32-level test, a PPU may be several times slower).
// If it is too slow: lower AppConfig width/height (e.g. 960x540 and scale on copy) or optimise software_renderer.cpp.

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <ppu-lv2.h>          // VERIFY
#include <io/pad.h>           // ioPadInit / ioPadGetInfo / ioPadGetData / padInfo / padData
#include <rsx/rsx.h>          // rsxInit / rsxMemalign / gcmSetFlip ...   VERIFY
#include <sysutil/sysutil.h>  // sysUtilRegisterCallback / sysUtilCheckCallback / SYSUTIL_EXIT_GAME
#include <sysutil/video.h>    // videoGetState / videoGetResolution / videoConfigure

#include "app.hpp"
#include "input.hpp"
#include "ps3_pad.hpp"

namespace {

constexpr int kWidth = 1280, kHeight = 720;
#ifndef FBW_TITLE_ID
#define FBW_TITLE_ID "FBWG00001"
#endif
// VERIFY: USRDIR location when launched as an installed title. For a quick test from an external drive change to the ELF's folder.
const char* kGameDir = "/dev_hdd0/game/" FBW_TITLE_ID "/USRDIR/game";
const char* kSavePath = "/dev_hdd0/game/" FBW_TITLE_ID "/USRDIR/fbw_progress.json";

bool g_exit_requested = false;

void sysutilCallback(u64 status, u64 /*param*/, void* /*usrdata*/) {
	if (status == SYSUTIL_EXIT_GAME) g_exit_requested = true;   // XMB / PS-button "Quit game"
}

// ---- RSX display ------------------------------------------------------------------------------------------------
struct Display {
	gcmContextData* ctx = nullptr;
	u32* buffer[2] = {nullptr, nullptr};
	u32 offset[2] = {0, 0};
	int current = 0;
	u32 pitch = 0;
	bool first_flip = true;
};

bool initDisplay(Display& d) {
	const u32 kHostSize = 1024 * 1024, kCbSize = 0x100000;
	void* host = memalign(1024 * 1024, kHostSize);
	if (!host) return false;
	rsxInit(&d.ctx, kCbSize, kHostSize, host);                       // VERIFY signature
	if (!d.ctx) return false;

	videoState state; videoResolution res;
	if (videoGetState(0, 0, &state) != 0 || state.state != 0) return false;   // VERIFY: need a connected display
	if (videoGetResolution(state.displayMode.resolution, &res) != 0) return false;
	// This layer renders 1280x720. VERIFY: if the console is set to a different output resolution, switch to
	// VIDEO_RESOLUTION_720 here via videoConfigure and require it to succeed.
	videoConfiguration config; memset(&config, 0, sizeof(config));
	config.resolution = VIDEO_RESOLUTION_720;
	config.format = VIDEO_BUFFER_FORMAT_XRGB;
	config.pitch = kWidth * sizeof(u32);
	if (videoConfigure(0, &config, nullptr, 0) != 0) return false;
	if (videoGetState(0, 0, &state) != 0) return false;

	gcmSetFlipMode(GCM_FLIP_VSYNC);
	d.pitch = kWidth * sizeof(u32);
	const u32 size = d.pitch * kHeight;
	for (int i = 0; i < 2; ++i) {
		d.buffer[i] = static_cast<u32*>(rsxMemalign(64, size));
		if (!d.buffer[i]) return false;
		if (rsxAddressToOffset(d.buffer[i], &d.offset[i]) != 0) return false;
		if (gcmSetDisplayBuffer(i, d.offset[i], d.pitch, kWidth, kHeight) != 0) return false;
		memset(d.buffer[i], 0, size);
	}
	gcmResetFlipStatus();
	return true;
}

void waitFlip() {
	while (gcmGetFlipStatus() != 0) usleep(200);
	gcmResetFlipStatus();
}

// RGBA bytes (R,G,B,A in memory order) -> big-endian XRGB word. Cell is big-endian, so a u32 reads 0x00RRGGBB.
void presentFrame(Display& d, const fbw::ImageRGBA& frame) {
	if (!d.first_flip) waitFlip();
	u32* dst = d.buffer[d.current];
	const uint8_t* src = frame.pixels.data();
	const size_t count = static_cast<size_t>(kWidth) * kHeight;
	for (size_t i = 0; i < count; ++i, src += 4)
		dst[i] = (static_cast<u32>(src[0]) << 16) | (static_cast<u32>(src[1]) << 8) | static_cast<u32>(src[2]);
	gcmResetFlipStatus();
	gcmSetFlip(d.ctx, d.current);                                    // VERIFY
	rsxFlushBuffer(d.ctx);
	gcmSetWaitFlip(d.ctx);
	d.current ^= 1; d.first_flip = false;
}

// ---- pads -------------------------------------------------------------------------------------------------------
void readPads(fw::PadState out[2]) {
	padInfo info; ioPadGetInfo(&info);                               // VERIFY: field name status[] / connected pad count
	for (int i = 0; i < 2; ++i) {
		fw::ps3::RawPad raw;
		if (info.status[i]) {
			padData data; ioPadGetData(i, &data);
			if (data.len > 0) {                                      // VERIFY: len == 0 means no new sample
				raw.connected = true;
				// Convert PSL1GHT's per-button fields into the Cell-pad digital1/digital2 byte layout that ps3_pad.hpp consumes.
				// Using the named fields avoids trusting raw bit positions from memory (see ps3_pad.hpp VERIFY note).
				raw.digital1 = (data.BTN_SELECT ? fw::ps3::D1_SELECT : 0) | (data.BTN_START ? fw::ps3::D1_START : 0) |
					(data.BTN_UP ? fw::ps3::D1_UP : 0) | (data.BTN_DOWN ? fw::ps3::D1_DOWN : 0) |
					(data.BTN_LEFT ? fw::ps3::D1_LEFT : 0) | (data.BTN_RIGHT ? fw::ps3::D1_RIGHT : 0);
				raw.digital2 = (data.BTN_CROSS ? fw::ps3::D2_CROSS : 0) | (data.BTN_CIRCLE ? fw::ps3::D2_CIRCLE : 0) |
					(data.BTN_SQUARE ? fw::ps3::D2_SQUARE : 0) | (data.BTN_TRIANGLE ? fw::ps3::D2_TRIANGLE : 0) |
					(data.BTN_L1 ? fw::ps3::D2_L1 : 0) | (data.BTN_R1 ? fw::ps3::D2_R1 : 0) |
					(data.BTN_L2 ? fw::ps3::D2_L2 : 0) | (data.BTN_R2 ? fw::ps3::D2_R2 : 0);
				raw.left_x = static_cast<uint8_t>(data.ANA_L_H);
				raw.left_y = static_cast<uint8_t>(data.ANA_L_V);
			}
		}
		out[i] = fw::ps3::toPadState(raw);
	}
}

double nowSeconds() {
	timeval tv; gettimeofday(&tv, nullptr);
	return static_cast<double>(tv.tv_sec) + static_cast<double>(tv.tv_usec) * 1e-6;
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
	sysUtilRegisterCallback(0, sysutilCallback, nullptr);
	ioPadInit(7);

	Display display;
	if (!initDisplay(display)) { fprintf(stderr, "FBWGPS3: display init failed\n"); return 1; }

	fbw::AppConfig cfg; cfg.game_dir = kGameDir; cfg.save_path = kSavePath; cfg.width = kWidth; cfg.height = kHeight;
	fbw::App app; std::string error;
	if (!app.init(cfg, error)) { fprintf(stderr, "FBWGPS3: init failed: %s\n", error.c_str()); return 1; }

	fw::Input input; fbw::ImageRGBA frame;
	double previous = nowSeconds();
	while (!g_exit_requested) {
		sysUtilCheckCallback();
		const double now = nowSeconds(); const double dt = now - previous; previous = now;
		fw::PadState pads[2]; readPads(pads);
		input.update(pads, static_cast<float>(dt));
		app.update(input, dt);
		if (!app.render(frame, &error)) { fprintf(stderr, "FBWGPS3: render failed: %s\n", error.c_str()); break; }
		presentFrame(display, frame);
	}
	app.shutdown();                                                  // writes progress
	ioPadEnd();
	return 0;
}
