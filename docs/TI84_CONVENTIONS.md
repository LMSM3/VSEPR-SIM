# TI-84 Programming Conventions

Authoritative rules for all TI-84 CE/Plus programs in this workspace.

---

## Reserved Variables

| Symbol   | Purpose                          | Scope  |
|----------|----------------------------------|--------|
| `K`      | Key buffer (`getKey->K`)         | Global |
| `LCONF`  | Config/defaults list             | Global |
| `999`    | Sentinel (triggers default load) | Global |
| `Str1-9` | Display labels only              | Local  |

**K means key buffer. Always. Nothing else. Ever.**

---

## 1. PAUSE Pattern

The built-in `Pause` token is **banned** for interactive use.

### Any-key pause (standard)

```
0->K
While K=0
 getKey->K
End
```

- `K=0` -> no key held -> keep spinning
- Any key -> `K!=0` -> exits that frame
- No ghost key carry-over (loop fires once per nonzero `getKey`)

### Enter-only pause (dialog-style)

```
0->K
While K!=105
 getKey->K
End
```

`105` = Enter key code.

**File:** `ti84/lib/PAUSE.ti`

---

## 2. DEFAULTS -- The 999 Sentinel

When the user types `999` at a `Prompt`, load a preset default.

```
Prompt A
If A=999
Then
 [DEFAULT_VALUE]->A
 Disp "DEFAULT:"
 Disp A
End
```

### Rules

1. Sentinel is `999` -- never a valid physical input for any quantity
2. After substitution, validation runs on the loaded value exactly as if typed
3. Always display the loaded default so the user sees it

**File:** `ti84/lib/DEFAULTS.ti`

---

## 3. Persistent Config -- LCONF

The TI-84 cannot open files. Named lists persist in RAM across runs.

```
; LCONF layout (set once in SETUP):
; LCONF(1) = default for variable A
; LCONF(2) = default for variable B
; LCONF(3) = default for variable C
```

### Reading

```
Prompt A
If A=999
Then
 LCONF(1)->A
 Disp "LOADED DEF:"
 Disp A
End
```

### Writing (SETUP program only)

```
{0,0,0,0,0}->LCONF
Prompt A
A->LCONF(1)
Disp "CONF SAVED"
```

**Only SETUP writes to LCONF. All other programs read.**

**Files:** `ti84/lib/CONFIG.ti`, `ti84/programs/SETUP.ti`

---

## 4. Strings

| Feature      | Reality                                        |
|--------------|------------------------------------------------|
| `Str0-Str9`  | Exist, max ~99 chars, NOT storable to lists    |
| `ord(/char(` | Convert char <-> integer (TI-ASCII 0-255)      |
| Named lists  | Can hold encoded strings as integer sequences  |

### Verdict

- `Str1-Str9` = display labels **only**
- `Disp "LITERAL"` = preferred for fixed output text
- Persistent string data = TI-ASCII integer lists

### Encoding (done once, manually or via `ti84_ascii` tool)

```
"OHMS" -> O=79, H=72, M=77, S=83 -> {79,72,77,83}->LUNIT
```

### Decoding (runtime)

```
""->Str1
For(I,1,dim(LUNIT))
 Str1+char(LUNIT(I))->Str1
End
Disp Str1
```

### Practical recommendation

Do NOT store strings in config. Store numeric codes. Use hardcoded
`Disp "LABEL"` strings. The ASCII encode/decode is only worth it for
user-entered text that must be saved.

**File:** `ti84/lib/STRINGS.ti`

---

## 5. getKey Code Reference

| Code | Key     |   | Code | Key   |   | Code | Key   |
|------|---------|---|------|-------|---|------|-------|
| 105  | Enter   |   | 22   | 2nd   |   | 45   | Clear |
| 25   | Up      |   | 34   | Down  |   | 55   | Del   |
| 24   | Left    |   | 26   | Right |   |      |       |
| 102  | 0       |   | 92   | 1     |   | 93   | 2     |
| 94   | 3       |   | 82   | 4     |   | 83   | 5     |
| 84   | 6       |   | 72   | 7     |   | 73   | 8     |
| 74   | 9       |   | 95   | (-)   |   | 104  | .     |

Full table: `tools/ti84_ascii table`

---

## 6. Program Template

Every program follows this skeleton:

```
; === HEADER ===
ClrHome
Disp "PROGRAM NAME"
Disp "v1.0"

; === INPUT (with sentinel defaults) ===
Disp "A (999=DEFAULT):"
Prompt A
If A=999
Then
 LCONF(1)->A
 Disp "LOADED:"
 Disp A
End

; === VALIDATION ===
If A<=0
Then
 Disp "ERR: A>0 REQ"
 Stop
End

; === COMPUTATION ===
; ... actual math here ...

; === OUTPUT ===
Disp "RESULT:"
Disp R

; === PAUSE (canonical) ===
Disp ""
Disp "[ANY KEY]"
0->K
While K=0
 getKey->K
End
ClrHome
```

---

## 7. Tools

| Tool                 | Purpose                           | Build                                        |
|----------------------|-----------------------------------|----------------------------------------------|
| `tools/ti84_ascii.c` | Encode/decode TI-ASCII strings   | `gcc -std=c11 -O2 -o ti84_ascii ti84_ascii.c` |

```bash
./ti84_ascii encode "OHMS"        # -> {79,72,77,83}
./ti84_ascii decode 79 72 77 83   # -> OHMS
./ti84_ascii validate "HELLO"     # -> OK: all chars printable
./ti84_ascii table                # -> full code table
```

---

## 8. File Layout

```
ti84/
  lib/
    PAUSE.ti       Canonical pause pattern
    DEFAULTS.ti    Sentinel default pattern
    CONFIG.ti      LCONF read/write pattern
    STRINGS.ti     String handling rules
  programs/
    SETUP.ti       Config initializer (writes LCONF)
tools/
  ti84_ascii.c     String <-> list encoder (C)
docs/
  TI84_CONVENTIONS.md   This file
```

---

## Rules Summary (copy to calculator notes)

```
PAUSE       -> 0->K : While K=0 : getKey->K : End       (K reserved)
ENTER-ONLY  -> 0->K : While K!=105 : getKey->K : End
DEFAULTS    -> Prompt X : If X=999 : Then : LCONF(N)->X : End
CONFIG FILE -> LCONF custom list, written by SETUP, read by all
STRINGS     -> Str1-Str9 for display only, lists for persistence
SENTINEL    -> 999 (never a valid physical value)
K           -> global key buffer, reserved, no other meaning
LCONF       -> global config list, index-mapped to variables
```
