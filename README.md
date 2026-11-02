# PS2 Pad Diag

Tester diagnostico per controller PlayStation 2 (DualShock / DualShock 2),
porting PS2 del progetto PS1
[ControlDiagPSX](https://github.com/FSMaker77/ControlDiagPSX).

Diagnostic tool for PlayStation 2 controllers (DualShock / DualShock 2),
PS2 port of the PS1 project
[ControlDiagPSX](https://github.com/FSMaker77/ControlDiagPSX).

<p align="center">
  <a href="controller_ps2.png"><img src="preview_controller.png" width="480" alt="PS2 Pad Diag controller"></a>
  <a href="splash.jpg"><img src="preview_splash.jpg" width="480" alt="PS2 Pad Diag splash"></a>
</p>

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

La schermata usa il controller reale `controller_ps2.png` e la splash
`splash.jpg` con la barra di caricamento nel riquadro vuoto. La UI è
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

Il programma **segue la regione della console**: su una PS2 PAL esce a 576i/50 Hz,
su una PS2 NTSC a 480i/60 Hz. La UI (640×448) viene centrata nella cornice PAL
con bande nere sopra e sotto. Per forzare la modalità si possono passare gli
argomenti `-ntsc` o `-pal` all'ELF (es. via ps2link), ma non servono per le ISO.

> **Se sullo schermo vedi solo bianco** leggi la sezione *Risoluzione problemi*
> più sotto: è quasi sempre la TV che non aggancia i 60 Hz, oppure il buffer
> del GS che non è mai stato disegnato (memoria video non inizializzata).

### Release

I file `.elf` e le due ISO sono allegati alla release **v1.0** nella pagina
[Releases](../../releases).

### Note PCSX2

PCSX2 azzera la memoria video all'avvio, la PS2 no (di norma contiene `0xff`,
cioè bianco). Per questo un errore che in emulatore si vede come **schermo
nero** sulla console si vede come **schermo bianco**: il GS sta mostrando un
framebuffer che nessuno ha ancora disegnato. Ora entrambi i framebuffer vengono
riempiti subito dopo l'inizializzazione, e la sequenza di avvio è:

1. schermata `STARTING...` (solo rettangoli, nessuna texture);
2. caricamento moduli IOP e pad;
3. splash con la barra di avanzamento;
4. schermata principale.

Quindi, se sulla console vedi:

- **bianco** → il problema è prima della schermata `STARTING...`: modalità
  video/TV oppure GS che non parte;
- **`STARTING...` che resta fermo** → si blocca nel caricamento dei moduli
  IOP (`SIO2MAN`/`PADMAN`/`LIBSD`/`AUDSRV`);
- **splash e poi nero/bloccato** → problema nel caricamento della texture;
- **la UI** → tutto ok.

### Risoluzione problemi

**Schermo bianco / nessun segnale sulla PS2**

- Verifica che la TV accetti il segnale della console: la UI segue la regione,
  quindi su una PS2 PAL esce a 50 Hz. Se la TV è collegata tramite un adattatore
  HDMI/scaler, controlla che supporti 576i.
- Se hai bruciato la ISO sbagliata per la tua console, usa quella della tua
  regione (`ps2pad-diag.iso` PAL, `ps2pad-diag-usa.iso` NTSC).
- Con una console PAL e una TV che accetta i 60 Hz puoi forzare NTSC lanciando
  l'ELF con l'argomento `-ntsc`.

**Il pad non risponde**

- Con l'ELF avviato da memory card/USB i moduli `cdrom0:` non esistono: il
  programma ripiega su `rom0:XSIO2MAN`/`rom0:XPADMAN`. Se anche questi non
  bastano, copia `SIO2MAN.IRX` e `PADMAN.IRX` accanto all'ELF.

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

The screen uses the real controller artwork `controller_ps2.png` and
the splash `splash.jpg` with the loading bar inside the empty frame.
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

The program **follows the console region**: a PAL PS2 outputs 576i/50 Hz, an
NTSC PS2 outputs 480i/60 Hz. The 640×448 UI is centred inside the PAL frame
with black bars above and below. The mode can be forced by passing `-ntsc` or
`-pal` to the ELF (e.g. through ps2link); the ISOs need no arguments.

> **If you only get a white screen** see *Troubleshooting* below: it is almost
> always a TV that cannot lock to 60 Hz, or a GS framebuffer that was never
> drawn (uninitialised video memory).

### Releases

The `.elf` and both ISOs are attached to release **v1.0** on the
[Releases](../../releases) page.


---

### Troubleshooting

PCSX2 zeroes video memory at boot, the PS2 does not (it normally reads back as
`0xff`, i.e. white). That is why a bug that looks like a **black screen** in the
emulator looks like a **white screen** on the console: the GS is scanning out a
framebuffer nobody has drawn yet. Both framebuffers are now painted right after
initialisation, and the boot sequence is:

1. `STARTING...` screen (rectangles only, no texture);
2. IOP modules and pad loading;
3. splash with the progress bar;
4. main screen.

So, on the console:

- **white** → the failure is before `STARTING...`: video mode/TV or GS init;
- **`STARTING...` stays** → stuck loading the IOP modules
  (`SIO2MAN`/`PADMAN`/`LIBSD`/`AUDSRV`);
- **splash, then black/frozen** → texture upload problem;
- **the UI** → everything works.

Other checks:

- The UI follows the console region (PAL PS2 → 50 Hz). Make sure the TV or the
  HDMI/scaler accepts 576i, and burn the ISO for the console's region
  (`ps2pad-diag.iso` PAL, `ps2pad-diag-usa.iso` NTSC).
- On a PAL console with a 60 Hz capable TV, launch the ELF with `-ntsc` to force
  480i.
- When the ELF is launched from a memory card/USB the `cdrom0:` modules do not
  exist; the tool falls back to `rom0:XSIO2MAN`/`rom0:XPADMAN`. If that is not
  enough, copy `SIO2MAN.IRX` and `PADMAN.IRX` next to the ELF.

## Project layout

| File | Purpose |
|---|---|
| `main.c` | UI, splash, button overlays, click edge detection |
| `gfx_ps2.c/h` | gsKit 2D layer (quads, text, shapes, backgrounds) |
| `pad_ps2.c/h` | libpad polling, pressure, actuators |
| `audio.c/h` | audsrv ADPCM click |
| `font6x8.h` | generated 5×10 bitmap font |
| `click.sad` | generated one-shot ADPCM click |
| `controller_ps2.png` | controller artwork |
| `assets/controller.raw` | 640×448 CT32 controller background |
| `splash.jpg` | splash artwork |
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
