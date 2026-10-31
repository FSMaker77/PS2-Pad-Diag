#!/bin/sh
# Build script for the DualShock 2 tester (ps2sdk + gsKit)
set -e

# Shared PS2 environment (compilers + ps2sdk + gsKit).
if [ -f "$HOME/ps2env.sh" ]; then
	. "$HOME/ps2env.sh"
else
	export PS2DEV=/usr/local/ps2dev
	export PS2SDK="$PS2DEV/ps2sdk"
	export PS2SDKSRC=/home/fsmaker/ps2sdk
	export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin:$PATH"
fi

echo "PS2DEV=$PS2DEV"
echo "PS2SDK=$PS2SDK"
echo

make clean
make

echo
echo "Output files:"
echo "  ps2pad-diag.elf  - EE executable (run via ps2client / OPL / host)"
echo
if command -v mkps2iso >/dev/null 2>&1; then
	mkps2iso -y mkps2iso.xml
	mkps2iso -y mkps2iso-usa.xml
	echo "  ps2pad-diag.iso      - EU/PAL disc (original console)"
	echo "  ps2pad-diag-usa.iso  - US/NTSC disc (NTSC console/emulator)"
fi
