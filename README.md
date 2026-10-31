# PS2 Pad Diag

Tester diagnostico per controller PlayStation 2 (DualShock / DualShock 2),
porting PS2 del progetto PS1
[ControlDiagPSX](https://github.com/FSMaker77/ControlDiagPSX).

Diagnostic tool for PlayStation 2 controllers (DualShock / DualShock 2),
PS2 port of the PS1 project
[ControlDiagPSX](https://github.com/FSMaker77/ControlDiagPSX).

---

## Italiano

### Cosa fa

Mostra a schermo un render del DualShock 2 e verifica in tempo reale:

- tutti i pulsanti (croce direzionale, △ ○ ✕ □, L1/L2/R1/R2,
  SELECT/START, L3/R3);
- la **pressione analogica** dei tasti sensibili (la trasparenza del verde
  segue la pressione 0–255);
- i due **stick analogici**, con un puntino che si muove dentro il pozzetto
  e le coordinate X/Y numeriche;
- i **motori di vibrazione**: tieni premuto **L2** per il motore grande
  (barra BIG), **R2** per il piccolo (barra SML);
- il **click audio** su ogni pressione di pulsante.

La schermata usa il controller reale `assets/controller_ps2.png` e la splash
`assets/splash.jpg` con la barra di caricamento nel riquadro vuoto. La UI è
pensata su una griglia 320×224 scalata 2× (640×448 NTSC) con margini per
l'overscan. Il testo usa un font bitmap 5×10 incorporato (Terminus, SIL OFL
1.1, vedi `FONT-LICENSE.txt`).

Viene usato un solo pad di controllo: la porta 1 se collegata, altrimenti la
porta 2. La porta attiva è evidenziata nell'header.

### Controlli

| Input | Cosa fa |
|---|---|
| Croce direzionale | evidenzia il braccio premuto |
| △ ○ ✕ □ | evidenzia il tasto e la pressione |
| L1 / L2 / R1 / R2 | evidenzia il tasto; L2 = motore grande, R2 = piccolo |
| SELECT / START | evidenzia il tasto |
| L3 / R3 | anello verde sullo stick, coordinate X/Y |
| Stick analogici | puntino mobile nel pozzetto |

### Build

Requisiti: [ps2sdk](https://github.com/ps2dev/ps2sdk) in `$PS2SDK`,
[gsKit + dmaKit](https://github.com/ps2dev/gsKit), toolchain EE e
`mkps2iso`.

```sh
./build.sh      # ELF + ISO EUR/PAL + ISO USA/NTSC
# oppure
make
```

### ISO

Le ISO sono **autosufficienti**: contengono `SYSTEM.CNF`, l'ELF,
`SIO2MAN.IRX`, `PADMAN.IRX`, `LIBSD.IRX` e `AUDSRV.IRX`. Non serve nessun
altro file.

- `ps2pad-diag.iso` — regione **EUR/PAL**, per la console PAL originale
- `ps2pad-diag-usa.iso` — regione **USA/NTSC**

Il programma imposta l'uscita NTSC: la TV deve accettare i 60 Hz.

### Release

I file `.elf` e le due ISO sono allegati alla release **v1.0** nella pagina
[Releases](../../releases).

### Note PCSX2



---

## English

### What it does

Shows a DualShock 2 render and live-tests:

- every button (d-pad, △ ○ ✕ □, L1/L2/R1/R2, SELECT/START, L3/R3);
- **analog pressure** on pressure-sensitive buttons (green opacity follows
  the 0–255 value);
- both **analog sticks**, with a dot moving inside the well and numeric X/Y
  readouts;
- **vibration motors**: hold **L2** for the big motor (BIG bar), **R2** for
  the small one (SML bar);
- a short **click sound** on every button press.

The screen uses the real controller artwork `assets/controller_ps2.png` and
the splash `assets/splash.jpg` with the loading bar inside the empty frame.
The UI is designed on a 320×224 grid scaled 2× (640×448 NTSC) with CRT
overscan margins. Text uses an embedded 5×10 bitmap font (Terminus, SIL OFL
1.1, see `FONT-LICENSE.txt`).

A single control pad drives the UI: port 1 if connected, otherwise port 2.
The active port is highlighted in the header.

### Controls

| Input | Action |
|---|---|
| D-pad | highlights the pressed arm |
| △ ○ ✕ □ | highlights the button and its pressure |
| L1 / L2 / R1 / R2 | highlights the button; L2 = big motor, R2 = small |
| SELECT / START | highlights the button |
| L3 / R3 | green ring on the stick, X/Y readout |
| Analog sticks | moving cursor inside the well |

### Build

Requirements: [ps2sdk](https://github.com/ps2dev/ps2sdk) in `$PS2SDK`,
[gsKit + dmaKit](https://github.com/ps2dev/gsKit), EE toolchain and
`mkps2iso`.

```sh
./build.sh      # ELF + EUR/PAL ISO + USA/NTSC ISO
# or
make
```

### ISO

Both ISOs are **self-contained**: they include `SYSTEM.CNF`, the ELF,
`SIO2MAN.IRX`, `PADMAN.IRX`, `LIBSD.IRX` and `AUDSRV.IRX`. No other files are
required.

- `ps2pad-diag.iso` — **EUR/PAL** region, for an original PAL console
- `ps2pad-diag-usa.iso` — **USA/NTSC** region

The program outputs NTSC: the TV must accept 60 Hz.

### Releases

The `.elf` and both ISOs are attached to release **v1.0** on the
[Releases](../../releases) page.


---

## Project layout

| File | Purpose |
|---|---|
| `main.c` | UI, splash, button overlays, click edge detection |
| `gfx_ps2.c/h` | gsKit 2D layer (quads, text, shapes, backgrounds) |
| `pad_ps2.c/h` | libpad polling, pressure, actuators |
| `audio.c/h` | audsrv ADPCM click |
| `font6x8.h` | generated 5×10 bitmap font |
| `click.sad` | generated one-shot ADPCM click |
| `assets/controller_ps2.png` | controller artwork |
| `assets/controller.raw` | 640×448 CT32 controller background |
| `assets/splash.jpg` | splash artwork |
| `assets/splash.raw` | 640×448 CT32 splash background |
| `sio2man.irx`, `padman.irx` | pad IOP modules (in the ISO) |
| `libsd.irx`, `audsrv.irx` | audio IOP modules (in the ISO) |
| `tools/genfont.py` | font generator |
| `tools/mkbg.py` | artwork → CT32 raw converter |
| `mkps2iso.xml`, `mkps2iso-usa.xml` | disc layouts |
| `SYSTEM.CNF`, `SYSTEM-USA.CNF` | boot configs |

## Credits / Licenses

- Project code: MIT, see `LICENSE`.
- Embedded bitmap font derived from **Terminus** by Dimitar Zhekov, SIL Open
  Font License 1.1, see `FONT-LICENSE.txt`.
- PS2SDK, gsKit, dmaKit, libpad, audsrv: their respective licenses.
