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
[gsKit + dmaKit](https://github.com/ps2dev/gsKit) e toolchain EE.

```sh
make            # produce ps2pad-diag.elf
./build.sh      # ELF + (legacy) ISO EUR/PAL e USA/NTSC
```

### Avvio (uLaunchELF) — metodo supportato

Serve **un solo file**: `PS2PAD-DIAG.ELF`. I moduli `sio2man`, `padman`,
`libsd` e `audsrv` sono **incorporati nell'ELF**, quindi non servono i `.IRX`
ne' altri file.

1. copia `PS2PAD-DIAG.ELF` in `mass:/APPS/` (o dove preferisci);
2. avvia **uLaunchELF** (FMCB);
3. `mass:/APPS/PS2PAD-DIAG.ELF` -> X.

Il programma **segue la regione della console**: PS2 PAL -> 576i/50 Hz,
NTSC -> 480i/60 Hz; la UI 640x448 e' centrata nella cornice PAL. Si puo'
forzare la modalita' con gli argomenti `-ntsc` / `-pal` (solo con un loader che
li passa all'ELF).

All'avvio, se il pad non risponde entro circa un secondo (perche' il launcher
ha spento padman con `padEnd()`), il programma **resetta l'IOP**, riapplica la
patch `sbv_patch_enable_lmb` e ricarica i moduli dalla RAM: libpad si riaggancia
e il pad torna a funzionare. In quel caso il click audio resta disattivato,
perche' l'IOP perde `libsd`/`audsrv`.

> **Non supportati in questa versione:**
> - **OPL -> Apps**: si blocca nella fase 26 (`padInit`), perche' OPL spegne
>   padman prima di cedere il controllo;
> - **ISO**: su alcune console il boot "gioco" di OPL mostra schermo bianco
>   anche con un ELF minimo di test, quindi il problema non e' il programma.

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
- Con una console PAL e una TV che accetta i 60 Hz puoi forzare NTSC lanciando
  l'ELF con l'argomento `-ntsc`.

**Il pad non risponde**

- Non servono file aggiuntivi: `sio2man`/`padman` sono dentro l'ELF. Se il pad
  non parte entro un secondo, il programma fa da solo il reset dell'IOP
  (guardando lo schermo si vedono lampeggiare i numeri 31…35).
- Se si ferma su un numero 26 o resta `STARTING...`, e' stato avviato da un
  launcher che spegne padman prima del passaggio (tipicamente **OPL → Apps**):
  usa **uLaunchELF**.

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
[gsKit + dmaKit](https://github.com/ps2dev/gsKit) and the EE toolchain.

```sh
make            # builds ps2pad-diag.elf
./build.sh      # ELF + (legacy) EUR/PAL and USA/NTSC ISOs
```

### Launching (uLaunchELF) — supported method

Only **one file** is needed: `PS2PAD-DIAG.ELF`. The IOP modules `sio2man`,
`padman`, `libsd` and `audsrv` are **embedded in the ELF**, so the `.IRX` files
are not required.

1. copy `PS2PAD-DIAG.ELF` to `mass:/APPS/` (or anywhere you like);
2. start **uLaunchELF** (FMCB);
3. `mass:/APPS/PS2PAD-DIAG.ELF` -> X.

The program **follows the console region**: PAL PS2 -> 576i/50 Hz, NTSC ->
480i/60 Hz; the 640x448 UI is centred inside the PAL frame. The mode can be
forced with `-ntsc` / `-pal` (only with a loader that forwards arguments).

If no pad answers within about a second (because the launcher shut padman down
with `padEnd()`), the tool **resets the IOP**, re-applies
`sbv_patch_enable_lmb` and reloads the modules from RAM: libpad rebinds and the
pad works again. In that path the audio click stays off, because the IOP loses
`libsd`/`audsrv`.

> **Not supported in this version:**
> - **OPL -> Apps**: it stops at stage 26 (`padInit`), because OPL shuts padman
>   down before handing over;
> - **ISO**: on some consoles OPL's "game" boot shows a white screen even with
>   a minimal test ELF, so the problem is not this program.

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
  HDMI/scaler accepts 576i.
- On a PAL console with a 60 Hz capable TV, launch the ELF with `-ntsc` to force
  480i.
- No extra files are needed: `sio2man`/`padman` are inside the ELF. If no pad
  answers within a second the tool resets the IOP by itself (stages 31…35 flash
  on screen).
- If it stops at stage 26 or stays on `STARTING...`, it was launched by
  something that shuts padman down before handing over (typically **OPL →
  Apps**): use **uLaunchELF**.

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
| `sbv/` | patch `sbv_patch_enable_lmb` (ps2sdk, AFL-2.0), usata dal reset IOP |
| `splash.jpg` | splash artwork |
| `assets/splash.raw` | 640×448 CT32 splash background |
| `sio2man.irx`, `padman.irx` | pad IOP modules (in the ISO) |
| `libsd.irx`, `audsrv.irx` | audio IOP modules (in the ISO) |
| `tools/genfont.py` | font generator |
| `tools/mkbg.py` | artwork → CT32 raw converter |
| `mkps2iso.xml`, `mkps2iso-usa.xml` | disc layouts (ISO legacy, non supportate) |
| `SYSTEM.CNF`, `SYSTEM-USA.CNF` | boot configs |

## Credits / Licenses

- Project code: MIT, see `LICENSE`.
- Embedded bitmap font derived from **Terminus** by Dimitar Zhekov, SIL Open
  Font License 1.1, see `FONT-LICENSE.txt`.
- PS2SDK, gsKit, dmaKit, libpad, audsrv: their respective licenses.
- `sbv/` = patch `sbv_patch_enable_lmb`/`sbv_patch_disable_prefix_check` from
  ps2sdk (Academic Free License 2.0), necessaria per ricaricare i moduli IOP
  dalla RAM dopo il reset.
