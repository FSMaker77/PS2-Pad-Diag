# Report tecnico — PS2 Pad Diag: controller non rilevato

Documento di handoff. Contiene tutto ciò che è stato verificato, i valori esatti
letti a schermo, i riferimenti al codice (ps2sdk / gsKit / OPL), le ipotesi e gli
esperimenti già fatti. Serve a far proseguire l'analisi senza ripartire da zero.

---

## 1. Obiettivo del programma

`PS2 Pad Diag` è un tester per controller DualShock/DualShock 2 per PlayStation 2
(porting PS1 di ControlDiagPSX). Mostra a schermo il render del controller e in
tempo reale: tasti premuti, pressione analogica, stick, vibrazione, click audio.

Codice: `/home/fsmaker/ps2-pad-diag` (repo git, remote
`https://github.com/FSMaker77/PS2-Pad-Diag.git`, 16 commit locali non pushati).

Stack: ps2sdk + gsKit + dmaKit + libpad + audsrv, toolchain EE, `mkps2iso`.

---

## 2. Ambiente

| Voce | Valore |
|---|---|
| Console | PS2 **PAL**, uscita tramite **adattatore PS2→HDMI** |
| Avvio homebrew | **FMCB** → OPL (ultima versione) oppure uLaunchELF |
| Supporto | chiavetta USB FAT32 (label `MODULES`), ISO in `mass:/DVD/`, ELF in `mass:/APPS/` |
| Rete | PS2 su `192.168.1.10` (DHCP del modem), PC su `192.168.2.209/24` (router). Per farli parlare serve `sudo ip addr add 192.168.1.200/24 dev eth0` sul PC |
| Emulatore di test | PCSX2 2.8.2 (BIOS USA scph39001), usato solo per verifiche |

Il PS2 e il PC sono collegati **allo stesso switch**, ma su due sottoreti diverse
(modem `192.168.1.x`, router `192.168.2.x`).

---

## 3. Stato attuale

> **RISOLTO (3/11, build 12:30).** Il fallback con reset IOP + patch LMB +
> moduli incorporati è stato **verificato su hardware**: con
> `ps2client execee host:ps2pad-diag.elf -hardreset` il programma mostra
> `P1:ANA`. Vedi §9.1 per i dettagli; il report resta come documentazione del
> percorso e delle cause intermedie.

| Aspetto | Stato |
|---|---|
| Video: modalità, niente schermo bianco/nero, UI, splash, artwork | **FUNZIONA** (verificato a video) |
| Pad via **ps2link** (`ps2client execee host:...`) | **FUNZIONA** (`P1:ANA`, verificato) |
| Pad via **OPL → Apps** | **NON funziona** |
| Pad via **uLaunchELF → mass:/APPS/...ELF** | **NON funziona** |
| ISO avviata da OPL (percorso "gioco") | **schermo bianco**, anche con un ELF minimo di test (176 KB) → problema separato, non del nostro programma |

---

## 4. Cronologia dei sintomi (valori esatti letti a schermo)

La UI mostra, quando il pad non è rilevato:

```
NO PAD <flags> S:<x> P:<x> I:<x> O:<x>/<x> <state1>/<state2> <req1>/<req2>
```

e in basso la provenienza dei moduli. Valori osservati:

| Contesto | Riga |
|---|---|
| OPL Apps, build iniziali | `NO PAD S:o P:o I:o O:o/o 7/1` (porta 1 = `PAD_STATE_ERROR`) |
| OPL Apps, dopo alcune correzioni | `NO PAD S:o P:o I:o O:o/o 5/5 b/b` (5 = `PAD_STATE_EXECCMD`, b = `PAD_RSTAT_BUSY`) |
| OPL Apps, dopo reload moduli | `NO PAD S:o P:o I:o O:o/o 5/1 b/0` |
| OPL Apps, build con messaggio | `NO PAD …` + basso: `COPY SIO2MAN.IRX+PADMAN.IRX IN APPS/` → i file **non** venivano visti |
| **uLaunchELF** (foto, build 3/11 11:51) | `NO PAD R S:o P:o I:- O:-/- 5/5 b/b` + basso `SIO mass:/APPS/SIO2MAN.IRX  PAD mass:/APPS/PADMAN.IRX` |
| **uLaunchELF** (build 3/11 12:22, con `padEnd()` prima del reload) | `NO PAD R S:o P:o I:- O:o/o 5/5 b/b` → **ancora `I:-` = padInit 0** |

Significato dei campi:
- `S:` / `P:` = `sio2man` / `padman` caricati (`o`), già residenti (`r`), falliti (`-`)
- `I:` = `padInit()`, `O:` = `padPortOpen()` porta 1 / 2
- ultimi due gruppi = `padGetState()` e `padGetReqState()` per porta
- prefisso `R ` = ha scaricato e ricaricato lo stack; `H ` = manca il servizio
  heap dell'IOP

Stati libpad: `0` DISCONN, `1` FINDPAD, `2` FINDCTP1, `5` EXECCMD, `6` STABLE,
`7` ERROR. Richieste: `0` COMPLETE, `1` FAILED, `2` BUSY.

---

## 5. Architettura del programma

| File | Ruolo |
|---|---|
| `main.c` | UI, splash, overlay tasti, rilevamento edge per il click |
| `gfx_ps2.c/h` | layer 2D gsKit (quad, testo, forme, sfondi), gestione VRAM |
| `pad_ps2.c/h` | polling libpad, pressione, attuatori, diagnostica |
| `modules.c/h` | caricamento moduli IOP da più percorsi + probe RPC |
| `audio.c/h` | click ADPCM via audsrv |
| `sio2man.irx`, `padman.irx` | moduli IOP, **incorporati nell'ELF** (bin2c) |
| `assets/*.raw` | due immagini 640×448 CT32, incorporate (≈2.3 MB) |

Flusso di `main()`:

```
gfx_ps2_init(mode)            // gsKit, doppio clear dei 2 framebuffer
frame "STARTING..."           // feedback visivo
stage 1 → pad_ps2_init()      // moduli + padInit + padPortOpen
stage 2 → audio_init()
stage 3
splash 90 frame
gfx_ps2_begin_controller()
loop: pad_ps2_poll(), draw_ui(), flip
```

Le fasi 1/2/3 e 21…27 sono disegnate a schermo: servono a capire dove si blocca.

---

## 6. Fatti verificati nei sorgenti (importanti)

### 6.1 gsKit / VRAM (spiega il "bianco" iniziale)

`gsKit_init_screen()` termina con `gsKit_clear()` + `gsKit_sync_flip()`. Il primo
`sync_flip` punta `DISPFB` al framebuffer **che non è stato disegnato**
(`ScreenBuffer[1]`). La VRAM reale del GS all'accensione si legge `0xFF` →
**schermo bianco**; PCSX2 la azzera → **schermo nero**. Da qui la differenza di
sintomo tra console ed emulatore.
→ Risolto disegnando **entrambi** i framebuffer subito dopo `gsKit_init_screen`.

### 6.2 OPL spegne padman prima di cedere il controllo

`src/opl.c`, `deinit()`:

```c
unloadPads();                    // -> padEnd()
deinitAllSupport(exception, modeSelected);
audioEnd(); ioEnd(); guiEnd(); menuEnd(); ...
```

`src/pad.c`, `unloadPads()` → `padEnd()` (libpad, EE side).

OPL carica in `sysInit` (src/system.c ~236-260) le **proprie** copie di
`sio2man`, `padman`, `libsd`, `audsrv` e **non le scarica** in `deinit()`.

### 6.3 Cosa fa `padEnd()` dentro padman (IOP)

`ps2sdk/iop/input/padman/src/padMiscFuncs.c`:

```c
s32 padEnd(void) {
    ... padPortClose(...) ...
    SetEventFlag(vblankData.eventflag, EF_EXIT_THREAD);
    vblankData.padEnd = 1;
    WaitEventFlag(... EF_VB_WAIT_THREAD_EXIT ...);
    DeleteThreadsEventFlag(&vblankData);
    ReleaseVblankHandler(0, VblankStart);
    ReleaseVblankHandler(1, VblankEnd);
    pad_ee_addr = NULL;
    padman_init = 0;          // <-- modulo residente ma MORTO
}
```

Il server RPC resta in piedi (quindi `padPortOpen` "risponde"), ma i thread e
l'handler del vblank sono stati cancellati: **nessuna transfer SIO2 parte più**.

### 6.4 libpad: `padGetState()` restituisce 5 mentre una richiesta è pendente

`ps2sdk/ee/rpc/pad/src/libpad.c`:

```c
state = pdata->state;                       // dal buffer condiviso, via DMA
if (state == PAD_STATE_STABLE && padGetReqState(port, slot) == PAD_RSTAT_BUSY)
    return PAD_STATE_EXECCMD;               // 5, non 6
```

`padInfoMode()` fa `padSetReqState(port, slot, PAD_RSTAT_BUSY)` quando la RPC
risponde 1. Quindi chiamare `padInfoMode()` a ogni frame tiene `reqState` BUSY
quasi sempre → `padGetState()` restituisce 5 → chi legge solo su `6` non legge
mai → **deadlock** (osservato: `5/5 b/b`).

### 6.5 libpad: `padInit()` NON riaggancia se è già agganciato

```c
int padInit(int mode) {
    if (padsif[0].server && padsif[1].server)
        return 0;             // 0 = "già inizializzato", NON un errore
    ...
    for (;;) { sceSifBindRpc(...); ... nopdelay(); }   // loop infinito se assente
}
```

`padsif[]` è statico nel processo EE. Se il modulo padman viene **sostituito**
(scaricato e ricaricato), libpad continua a usare l'handle del **vecchio server
morto** e `padInit()` restituisce 0 senza riagganciare.

`padCleanup()` (statica) azzera `padsif` e `padInitialised`; viene chiamata da
`padInit()` solo se `HasIopRebootedSinceLastCall()`, oppure da `padEnd()` se la
RPC END risponde 1.

→ **Questa è l'ipotesi principale del guasto residuo** (vedi §9).

### 6.6 `padRead()` non fa RPC

```c
case 2: pdata = padGetDmaStrNew(port, slot);
        memcpy(data, pdata->data, pdata->length);
        return pdata->length;
```

Legge solo dal buffer condiviso (`padArea` passato a `padPortOpen`), con
`SyncDCache` in `padGetDmaStrNew`. Restituisce 0 se `length == 0`.

### 6.7 `mass:` non è visibile al loader dei moduli dentro un'app OPL

- Il loader dei moduli (`SifLoadModule`) passa dall'`ioman` "vecchio".
- OPL usa lo stack `iomanX`/`fileXio` (BDM/USB). Da un'app OPL quindi
  `SifLoadModule("mass:/…")` risponde **"Unknown device 'mass'"** (osservato nel
  log di ps2link quando la periferica non c'era, e dedotto per OPL).
- Verificato: da uLaunchELF invece `mass:` **è** visibile (la foto mostra
  `SIO mass:/APPS/SIO2MAN.IRX`).

Conseguenza: in OPL i moduli vanno caricati **dalla RAM** (incorporati), non da
file.

### 6.8 `SifInitIopHeap()` va in loop infinito se manca il server

`ps2sdk/ee/kernel/src/iopheap.c`:

```c
while ((res = sceSifBindRpc(&_ih_cd, 0x80000003, 0)) >= 0 && !_ih_cd.server)
```

Se l'IOP non ha il modulo `iopheap`, resta appeso → osservato come blocco su
`STARTING...`. Va chiamata solo dopo aver verificato che `0x80000003` risponda.

### 6.9 Il loader "gioco" di OPL e l'ELF grande

Percorso ISO: OPL avvia il suo EE core, che fa `LoadElf()` (loader IOP, con
ring buffer) e poi `CleanExecPS2()`. Lo storage dei moduli OPL è a
`0x00097000` (include/iosupport.h), l'ELF si carica a `0x00100000` → nessuna
sovrapposizione.

Lo schermo bianco della ISO **si presenta anche con un ELF minimo** (`VTEST.iso`,
176 KB, solo `gsKit_clear` a colori) → non dipende dal nostro programma.

---

## 7. Modifiche già applicate (git log)

```
62abcdd Pad: use the resident padman, reload it only when it does not answer
06a33ea Pad: only touch the IOP heap when it is really there
f1eb228 Embed sio2man/padman in the ELF and load them with SifExecModuleBuffer
b94b5f9 Pad: say clearly when the .IRX files are missing
52c390c Pad: read while a request is still busy (fixes the EXECCMD deadlock)
8d509fc Pad: initialise pads before the video mode is changed
020dc24 Pad: unload and reload sio2man/padman to revive them
6a2899d Pad: reuse the sio2man/padman OPL already has in the IOP
781aa1e Pad: fix the RPC probe false negative (duplicate padman)
113f24f Pad: retry padPortOpen, search modules everywhere, show why it fails
9e3b360 Pad: stop hammering padman, which put the port in PAD_STATE_ERROR
2a13c57 Fix white screen on real hardware: video mode + framebuffer init
cb83b52 Fix module path construction: the file name was dropped
dafb54c Shrink the ISO padding from 20 MB to 1 MB
6007fb7 README: document the working launch methods
```

Punti chiave del codice attuale (`pad_ps2.c`):

```c
static void pad_load_modules(void) {
    have_files = mod_can_open("PADMAN.IRX") && mod_can_open("SIO2MAN.IRX");

    if (mod_rpc_alive(PADMAN_RPC_ID)) {          /* padman già vivo */
        dbg_sio = dbg_pad = 0; dbg_sio_res = dbg_pad_res = 1;
        return;                                   /* non carica nulla */
    }
    if (!have_files) dbg_iopheap = mod_rpc_alive(IOPHEAP_RPC_ID);

    /* embedded (SifExecModuleBuffer) -> file -> rom0:XSIO2MAN/XPADMAN */
    ...
}

static int pad_reload_stack(void) {
    padEnd();                    /* <-- APPENA AGGIUNTO: resetta libpad */
    mod_unload("padman"); mod_unload("sio2man");
    /* embedded / file / rom0 */
}
```

Nel poll, se nessuna porta è connessa per ~90 frame (~1,5 s), chiama una volta
`pad_reload_stack()` e ripete `padInit()` + apertura porte.

`mod_unload()` usa una RPC "search module by name" (LF_F_SEARCH_MOD_BY_NAME = 9)
inviata in modalità *nowait* e attesa al massimo ~400 iterazioni (perché
`SifSearchModuleByName` di ps2sdk va in blocco su PCSX2).

---

## 8. Il problema residuo

Avviando **da `mass:`** (uLaunchELF; e presumibilmente OPL), i moduli vengono
trovati e caricati (`S:o P:o`, percorsi `mass:/APPS/…`), lo stack viene
ricaricato (`R`), ma:

- `I:-` → `padInit()` ha restituito **0**, che in libpad significa
  *"già inizializzato"* (non un errore);
- `O:-/-` → `padPortOpen()` ha restituito 0 su entrambe le porte
  (`0` può anche significare "porta già aperta");
- `5/5 b/b` → `padGetState()` = EXECCMD e `reqState` = BUSY: sono **i valori
  iniziali** che scrive `padPortOpen`, cioè padman non ha mai completato nulla.

Conclusione: i moduli si caricano ma il **pad** non viene mai letto; lo stato
sembra quello di un libpad agganciato a un padman che non fa transfer.

---

## 9. Ipotesi ordinate

**H1 — CONFERMATA PARZIALMENTE (build 3/11 12:22): la correzione con `padEnd()` non basta.**
`I:-` resta anche dopo il reload: quindi `padEnd()` non ha eseguito `padCleanup()`
(libpad lo fa solo se la RPC END risponde 1; con padman già spento la RPC
fallisce) e `padInit()` continua a restituire 0 sul server vecchio.
`O:o/o` invece è migliorato (le porte risultano aperte), ma lo stato resta
`5/5 b/b`.

**H1-originale (forte) — libpad resta agganciato al vecchio padman.**
`padInit()` ritorna 0 se `padsif[]` è già valorizzato (6.5). Dopo
`mod_unload`+`mod_load` di padman, libpad continua a mandare le RPC al server
vecchio/morto: `padPortOpen` risponde 0 e le transfer non avvengono mai
(`5/5 b/b`).
*Fix appena applicato*: `padEnd()` prima di scaricare il modulo (fa `padCleanup()`
se la RPC END risponde 1) → il successivo `padInit()` riaggancia.
**Non ancora verificato su hardware.**

**H2 (media) — il modulo caricato da file non parte davvero.**
`mod_load` può restituire ≥0 anche se il modulo non si registra (nome duplicato
o `_start` che esce). Da verificare leggendo l'output `loadmodule: id N, ret R`
via ps2link.

**H3 (media) — `SifExecModuleBuffer` non supportato (patch sbv assente).**
OPL applica `sbv_patch_enable_lmb()` all'avvio, ma se l'IOP viene ricreato la
patch sparisce e `LF_F_MOD_BUF_LOAD` non risponde (possibile blocco).

**H4 (bassa) — problema hardware/tempi SIO2.**
Da escludere: gli stessi moduli, con IOP pulito (ps2link), leggono il pad
correttamente; i giochi funzionano.

**H5 (bassa) — il controller è un clone con handshake atipico.**
Da escludere per lo stesso motivo (funziona via ps2link).

---

## 10. Esperimenti già fatti e risultati

| Esperimento | Esito |
|---|---|
| ELF in PCSX2 (host:) | funziona sempre (P1:ANA) — PCSX2 **emula padman**, quindi non è un banco di prova valido per questo bug |
| ELF via ps2link (host:) | **funziona** (log: `loadmodule: fname host:sio2man.irx -> id 33`, `padman -> id 34`) |
| ISO da OPL (`-fastboot` in PCSX2) | funziona in emulatore |
| ISO da OPL su console | schermo bianco |
| VTEST.iso (ELF minimo) da OPL su console | schermo bianco → il percorso ISO è rotto a prescindere dal programma |
| OPL Apps con solo l'ELF | `COPY SIO2MAN.IRX+PADMAN.IRX IN APPS/` (file non visti) |
| OPL Apps con i 4 `.IRX` | `S:o P:o` ma `5/5 b/b` |
| uLaunchELF con ELF+IRX su `mass:` | `R S:o P:o I:- O:-/- 5/5 b/b` |
| Rimozione chiamate `padInfoMode` per frame | risolto il caso `7/1` / `5/5`, ma non basta |
| Lettura anche in stato 5 | necessaria, non sufficiente |
| Thread separato per l'init dei pad | peggiorativo (gp del thread) → rimosso |

---

## 11. Cosa non è stato possibile verificare

- Il **log TTY del programma mentre parte da `mass:`**: ps2link permette
  `ps2client execee mass:/APPS/PS2PAD-DIAG.ELF` (il file lo legge il PS2, l'output
  arriva al PC), ma il tentativo non è andato a buon fine perché il PS2 non era
  raggiungibile e il PC aveva perso l'indirizzo `192.168.1.200/24`.
- Se `padEnd()` (H1) risolve: fix compilato ma non testato.
- Il motivo dello schermo bianco del percorso ISO (indipendente dal programma).

---

### 9.1 Strada suggerita dopo H1

`padInit()` chiama `padCleanup()` anche quando `HasIopRebootedSinceLastCall()`
è vero. Quindi **un reset dell'IOP azzera libpad in modo affidabile**:

```c
SifIopReset("", 0);
while (!SifIopSync()) ;
/* poi ricaricare sio2man + padman e chiamare padInit() */
```

Problema: dopo `SifIopReset` la periferica `mass:` sparisce (nessun driver USB)
e il loader dei moduli non supporta il caricamento da buffer senza la patch
`sbv_patch_enable_lmb()` (che OPL applica solo alla sua sessione). Le due
possibilità:

1. incorporare i moduli nell'ELF **e** riuscire a usare `SifExecModuleBuffer`
   dopo il reset → serve implementare/applicare la patch LMB (equivalente di
   `sbv_patch_enable_lmb`, non presente in `libpatches.a` di questa installazione);
2. tenere `padman` vivo: far sì che il launcher non chiami `padEnd()` (non
   controllabile per OPL).

## 12. Prossimi passi consigliati

1. **Catturare il TTY da `mass:`** (dà la risposta in 2 minuti):
   ```sh
   sudo ip addr add 192.168.1.200/24 dev eth0      # una volta sola
   # sul PS2: avviare mass:/PS2LINK/PS2LINK.ELF
   cd /home/fsmaker/ps2-pad-diag
   /home/fsmaker/PS2LINK/ps2client -h 192.168.1.10 execee mass:/APPS/PS2PAD-DIAG.ELF
   ```
   Guardare le righe `loadmodule: fname mass:/APPS/PADMAN.IRX args 0 arg` e il
   relativo `loadmodule: id N, ret R` (N<0 = non caricato; R≠0 = errore di start).

2. Se i moduli risultano caricati ma `padInit` continua a dare 0, verificare H1
   forzando il reset di libpad: chiamare `padEnd()` **prima** di `mod_unload`
   (già fatto) e, se non basta, far ripartire l'IOP con
   `SifIopReset("", 0); SifIopSync();` e ricaricare i moduli incorporati con
   `SifExecModuleBuffer` (attenzione: `mass:` sparisce, quindi solo moduli
   embedded e niente `open()`).

3. In alternativa, testare con `padPortOpen` una porta alla volta e leggere
   `pdata->length` (via `padRead`) per capire se il DMA dei dati avviene.

---

## 13. File e percorsi utili

| Percorso | Contenuto |
|---|---|
| `/home/fsmaker/ps2-pad-diag` | sorgenti + ELF + ISO (git) |
| `/home/fsmaker/ps2-pad-diag/ps2pad-diag.elf` | ELF (moduli incorporati) |
| `/home/fsmaker/ps2-pad-diag/ps2pad-diag.iso` | ISO PAL 4,2 MB |
| `/home/fsmaker/ps2-pad-diag/ps2pad-diag-usa.iso` | ISO NTSC |
| `/home/fsmaker/PS2LINK/` | `PS2LINK.ELF`, `IPCONFIG.DAT`, `ps2client`, `avvia.sh` |
| `/home/fsmaker/PS2PAD-USB/` | cartella pronta per la chiavetta (`APPS/`, `conf_apps.cfg`, `PS2LINK/`) |
| `/home/fsmaker/OPL-APPS/` | copia per OPL Apps + ISO |
| `/home/fsmaker/ps2-vtest/` | test video minimo (`vtest.iso`, `vtest-pal.iso`, `vtest-ntsc.iso`) |

Chiavetta USB (FAT32, label `MODULES`), layout usato:

```
mass:/APPS/PS2PAD-DIAG.ELF
mass:/APPS/SIO2MAN.IRX
mass:/APPS/PADMAN.IRX
mass:/APPS/LIBSD.IRX
mass:/APPS/AUDSRV.IRX
mass:/conf_apps.cfg            -> PS2 Pad Diag=mass:/APPS/PS2PAD-DIAG.ELF
mass:/PS2LINK/PS2LINK.ELF
mass:/PS2LINK/IPCONFIG.DAT     -> 192.168.2.10 255.255.255.0 192.168.2.1
```
