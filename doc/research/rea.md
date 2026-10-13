# Checking original Settlers II behaviour with REA

REA ("Reverse Engineer Anything", MIT, npm `rea-agents`, github.com/morluto/rea) wraps
Ghidra headless and answers one question per call with evidence: pseudocode, assembly,
callers, callees, referenced strings. Use only the genuine package; lookalike repos with
"Download rea" exist.

Use it when RTTR's reimplementation is unsure what the original did: campaign and
mission logic, save formats, unlock rules. Use the RTX mission scripts and the data files
first; they often answer the question once the binary has told you where to look.

## Rules

- Analyse your own copy of the game locally. Never commit S2.EXE, its converted ELF, game
  data, or decompiled code. Record findings in our own words: behaviour, file formats,
  addresses as evidence, and the REA/Ghidra versions they rely on.
- Each REA call imports and analyses the binary again: about two minutes for S2.EXE on
  two cores. Run independent questions in parallel.

## Setup

Ghidra has no loader for DOS/4GW Linear Executables, so S2.EXE cannot be opened as
shipped. `tools/re/le2elf.py` converts it into a 32-bit i386 ELF: every object stays at
its own linear address, internal fixups are applied, selector fixups are dropped.

    python3 tools/re/le2elf.py <S2>/ROOT/S2.EXE s2.elf
    # S2.EXE (1939570 bytes, GOG copy): 15 objects, code at 0x10000, data at 0x170000,
    # entry 0x93c2c, 72689 fixups applied, none skipped
    export JAVA_HOME=<jdk-21> GHIDRA_INSTALL_DIR=<ghidra_12.1.4_PUBLIC>
    rea function s2.elf 0x322f7 --format json   # any address inside a function works

To find code for a feature, look for its strings: the address of a string in the data
object, then a scan of the ELF for that 32-bit address gives the instructions using it.

Pitfalls:

- S2.EXE is Watcom-compiled: arguments go in EAX, EDX, EBX, ECX. Ghidra assumes cdecl,
  so the pseudocode loses every argument (`in_EAX`, `extraout_CX`). Read the assembly in
  the result for calls; the pseudocode is fine for control flow and field offsets.
- File I/O goes through a stream class (vtables near 0x1757c0); open, seek, read and
  write are separate calls with the buffer in EDX and the size in EBX.

## Finding 1: campaign progress and mission unlocking (2026-10-12)

REA 6.4.0 / Ghidra 12.1.4 on S2.EXE as above.

- Progress of the Roman campaign is `SAVE\MISSION.DAT`: one ASCII byte per mission,
  `'1'` = may be played. The game reads 30 bytes, sets flags, and writes 30 bytes back.
  The shipped file is `1100000000` (CR LF, ^Z): missions 0 and 1 are open from the start.
  This matches RTTR's default `chaptersEnabled = {0, 1}`.
- The function at 0x320a0 evaluates the mission's goal records (up to 40, one per
  `MET_*` event). When the final event fires it opens MISSION.DAT and marks up to three
  missions (record fields +0x13/+0x15/+0x17, indices below 30) as open.
- Those three come from the mission script: `!MET_SET_FINAL_EVENT <event> <a> <b> <c>`
  in `DATA/MISSIONS/MIS_000n.RTX`, 255 = none. For the Roman campaign:
  mission n opens n+1 for n = 0..8, and mission 9 opens nothing. Strictly linear.
- Difference to our Lua: `MISS205.lua` opens chapters 6 AND 7 (upstream comment "remove
  this when chapter 7 is playable", i.e. MISS206). The original opens only 6. Before
  removing the extra unlock, MISS206 has to be shown winnable in RTTR, not just loadable.
- `SAVE\MISSION2.DAT` belongs to the World campaign and is written elsewhere (function at
  0x1726e, with the continent map `gfx\pics\world.lbm`/`worldmsk.lbm` at 0x5e70d). The
  MIS_1xxx scripts have no `MET_SET_FINAL_EVENT`, so World unlocking is a separate,
  still open question.
