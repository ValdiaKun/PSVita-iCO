TITLEID     := ICOV00001
TARGET      := ico_vita_gl_probe
SOURCES     := .
INCLUDES    :=

LIBS = -lvitaGL -lstdc++ -lSceLibKernel_stub -lSceAppMgr_stub -lSceAppUtil_stub -lmathneon \
       -lc -lSceCommonDialog_stub -lm -lSceGxm_stub -lSceDisplay_stub -lSceSysmodule_stub \
       -lzip -lz -lvitashark -lSceShaccCgExt -ltaihen_stub -lSceShaccCg_stub \
       -lSceKernelDmacMgr_stub -lSceCtrl_stub

CFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
OBJS     := $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
PREFIX   := arm-vita-eabi
CC       := $(PREFIX)-gcc
CXX      := $(PREFIX)-g++
CFLAGS   := -g -Wl,-q -O2 -Wall -Wextra -Werror -ftree-vectorize
CXXFLAGS := $(CFLAGS) -fno-exceptions -std=gnu++11 -fpermissive

.PHONY: all clean
all: $(TARGET).vpk

$(TARGET).vpk: eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLEID) "$(TARGET)" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin $@

eboot.bin: $(TARGET).velf
	vita-make-fself -s $< eboot.bin

$(TARGET).velf: $(TARGET).elf
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f *.o *.velf *.elf *.vpk param.sfo eboot.bin
