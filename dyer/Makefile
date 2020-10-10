APP = dyer

include $(SDK_DIR)/Makefile.defs

OBJS = $(ASSETS).gen.o main.o
ASSETDEPS += *.png $(ASSETS).lua

# build assets.html to proof stir-processed assets.
# comment out to disable.
ASSETS_BUILD_PROOF := yes

include $(SDK_DIR)/Makefile.rules

stir:
	stir -o assets.gen.h assets.lua

sim:
	siftulator dyer.elf

ins:
	swiss install dyer.elf