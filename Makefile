# TEST DIAG - DualShock 2 controller tester (ps2sdk + gsKit)
#
# Build with the EE (Emotion Engine) toolchain. Requires:
#   - ps2sdk            https://github.com/ps2dev/ps2sdk   (installed to $PS2SDK)
#   - gsKit + dmaKit    https://github.com/ps2dev/gsKit    (installed to $PS2DEV/gsKit)
#
# Make sure PS2SDK, PS2DEV and the ps2sdk tools are in your environment
# (see ../ps2env.sh), then run:  make

EE_BIN    = ps2pad-diag.elf
EE_OBJS   = main.o gfx_ps2.o pad_ps2.o audio.o modules.o click_adp.o \
            controller_bg.o splash_bg.o sio2man_irx.o padman_irx.o \
            sbv/common.o sbv/patch_enable_lmb.o sbv/patch_disable_prefix_check.o \
            sbv/slib.o sbv/smem.o sbv/smod.o

EE_INCS   = -I$(PS2DEV)/gsKit/include -I.
EE_LDFLAGS = -s
EE_LIBS   = -L$(PS2DEV)/gsKit/lib -lgskit -ldmakit -lgskit_toolkit -lpad -laudsrv -lpatches

all: $(EE_BIN)

gfx_ps2.o: font6x8.h

controller_bg.c: assets/controller.raw
	$(PS2SDK)/bin/bin2c $< $@ controller_bg

splash_bg.c: assets/splash.raw
	$(PS2SDK)/bin/bin2c $< $@ splash_bg

click_adp.c: click.sad
	$(PS2SDK)/bin/bin2c $< $@ click_adp

sio2man_irx.c: sio2man.irx
	$(PS2SDK)/bin/bin2c $< $@ sio2man_irx

padman_irx.c: padman.irx
	$(PS2SDK)/bin/bin2c $< $@ padman_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

clean:
	rm -f $(EE_BIN) $(EE_OBJS)
