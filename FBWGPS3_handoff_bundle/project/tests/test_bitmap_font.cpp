#include "bitmap_font.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>

int main() {
	const char* path = "test_bitmap_font_fixture.fnt";
	{
		std::ofstream out(path);
		out << "info face=\"Fixture Font\" size=16\n";
		out << "common lineHeight=20 base=15 scaleW=64 scaleH=64 pages=1 packed=0\n";
		out << "page id=0 file=\"font.png\"\n";
		out << "chars count=2\n";
		out << "char id=65 x=1 y=2 width=8 height=12 xoffset=0 yoffset=2 xadvance=9 page=0 chnl=0\n";
		out << "char id=66 x=10 y=2 width=7 height=12 xoffset=0 yoffset=2 xadvance=8 page=0 chnl=0\n";
		out << "kernings count=1\n";
		out << "kerning first=65 second=66 amount=-1\n";
	}
	fbw::BitmapFont font;
	std::string error;
	const bool ok = font.load(path, &error);
	std::remove(path);
	if (!ok) { std::cerr << "FAIL: " << error << "\n"; return 1; }
	const auto* a = font.glyph(65);
	const auto* b = font.glyph(66);
	if (font.line_height != 20 || font.base != 15 || font.pages.size() != 1 ||
		font.glyphs.size() != 2 || font.kernings.size() != 1 || !a || !b || a->xadvance != 9 || b->width != 7) {
		std::cerr << "FAIL: parsed BMFont metrics mismatch\n";
		return 1;
	}
	if (font.glyph(999) != nullptr) { std::cerr << "FAIL: unknown glyph should be absent\n"; return 1; }
	std::cout << "PASS: BMFont descriptor parser fixture (2 glyphs, 1 kerning pair)\n";
	return 0;
}
