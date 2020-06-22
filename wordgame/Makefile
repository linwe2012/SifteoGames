APP = wordgame

include $(SDK_DIR)/Makefile.defs

OBJS = $(ASSETS).gen.o main.o
ASSETDEPS += *.png $(ASSETS).lua

#CFLAGS += -O3
#CFLAGS += -DNO_LOG

# build assets.html to proof stir-processed assets.
# comment out to disable.
ASSETS_BUILD_PROOF := yes



include $(SDK_DIR)/Makefile.rules

stir:
	stir -o assets.gen.h assets.lua