# PSL1GHT build for FBWGPS3.  NOT TESTED: no PS3 toolchain was available when this was written.
# Needs: ps3toolchain + PSL1GHT (PS3DEV, PSL1GHT env vars) and ps3libraries' libpng + zlib (portlibs).
# Build:   make -C ps3         -> fbwgps3.elf / fbwgps3.self        make -C ps3 pkg -> fbwgps3.pkg  (needs sfo/pkg tools)
# Run the game from RPCS3 first (emulator) before trying hardware.
ifeq ($(strip $(PSL1GHT)),)
$(error "PSL1GHT is not set. Install ps3toolchain + PSL1GHT and export PS3DEV and PSL1GHT.")
endif
include $(PSL1GHT)/ppu_rules

TARGET   := fbwgps3
TITLE    := Fireboy and Watergirl - Forest Temple (port)
APPID    := FBWG00001
CONTENTID:= UP0001-$(APPID)_00-0000000000000000

CXXFILES := ps3_main.cpp ../src/level.cpp ../src/ground.cpp ../src/software_renderer.cpp
INCLUDES := -I../src
# The codebase needs C++17 (std::clamp etc.); the toolchain's GCC must support -std=gnu++17 (VERIFY: old ps3toolchain GCC versions may not).
CXXFLAGS := -std=gnu++17 -O2 -Wall -mcpu=cell $(INCLUDES) -DFBW_TITLE_ID=\"$(APPID)\" -I$(PORTLIBS)/include
LDFLAGS  := -L$(PORTLIBS)/lib -lpng -lz -lrsx -lgcm_sys -lio -lsysutil -lsysmodule -lrt -llv2 -lm

OBJS := $(CXXFILES:.cpp=.o)
.PHONY: all clean pkg
all: $(TARGET).self
$(TARGET).elf: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
%.self: %.elf
	$(SELF) $< $@
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
pkg: $(TARGET).self
	$(VERBOSE)mkdir -p pkg/USRDIR && cp $(TARGET).self pkg/USRDIR/EBOOT.BIN
	$(VERBOSE)$(SFO) --title "$(TITLE)" --appid "$(APPID)" -f $(PSL1GHT)/host/bin/../sfo.xml pkg/PARAM.SFO   # VERIFY
	$(VERBOSE)$(PKG) --contentid $(CONTENTID) pkg/ $(TARGET).pkg                                           # VERIFY
	@echo "Copy the game/ asset folder next to EBOOT.BIN in pkg/USRDIR/ BEFORE running 'make pkg' (not done automatically)."
clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).self $(TARGET).pkg
