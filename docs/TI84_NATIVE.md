# TI-84+ Native C Build System

Speed to iteration: **one command from C source to .8xp on the calculator.**

---

## Quick Start

```powershell
# 1. Install z88dk (once)
cd ti84\native
.\install_z88dk.ps1

# 2. Build hello world (Commandment 17: start minimal)
.\tibuild.ps1 src\hello.c HELLO

# 3. Send out\HELLO.8xp to calculator via TI Connect
# 4. Run from Ion/MirageOS shell
# 5. See "HELLO Z80" on screen

# 6. Build real program
.\tibuild.ps1 src\fatigue.c FATIGUE
```

---

## Pipeline (Three Layers — Commandment 12)

```
Layer 1: source.c          Your C code
    |
    v  [zcc +ti83p]     Z80 cross-compiler (z88dk)
    |
Layer 2: NAME.bin          Linked Z80 machine code
    |
    v  [appmake/bin2var]   TI container wrapper
    |
Layer 3: NAME.8xp          Sendable TI variable file
```

For Flash apps (.8xk), Layer 3 uses `-create-app` (Commandment 13).

---

## Directory Layout

```
ti84/native/
  tibuild.ps1           One-command build script
  bin2var.ps1           Binary-to-.8xp wrapper (fallback)
  install_z88dk.ps1     z88dk installer
  include/
    ti84.h              Common header (clrhome, waitkey, finput)
  src/
    hello.c             Minimal test (Commandment 17)
    fatigue.c           S-N fatigue calculator
  build/                Intermediate .bin files
  out/                  Final .8xp / .8xk files (send these)
```

---

## The 18 Commandments

### 1. .8xp ≠ .8xk

| Extension | Content            | Use for                    |
|-----------|--------------------|----------------------------|
| .8xp      | Program/variable   | Normal programs             |
| .8xk      | Flash application  | Large apps, persistent     |

### 2. Choose execution model first

| Model     | tibuild flag   | Requires                   |
|-----------|---------------|----------------------------|
| Ion       | `-shell ion`  | Ion shell on calculator    |
| MirageOS  | `-shell mirageos` | MirageOS on calculator |
| Doors CS  | `-shell dcs`  | Doors CS on calculator     |
| Raw ASM   | `-shell none` | Direct Asm( execution      |
| Flash app | `-target 8xk` | Nothing extra              |

### 3. Z80 only

TI-84+ SE = Z80 processor. Not eZ80. Not CE. No .ez80 output.

### 4-6. Source → Binary → Container

```
.c is just source
.bin is just machine code (not sendable)
.8xp is the sendable container with header + checksum
```

### 7. Naming rules

- Max 8 characters
- Uppercase letters and digits only
- Must start with a letter
- tibuild enforces this automatically

### 8. RAM vs Archive

Default: RAM execution. Ion/MirageOS handle archive-to-RAM copy.

### 9-10. Shell format matters

Ion header ≠ MirageOS header ≠ Doors CS header ≠ raw.
tibuild sets the correct `-startup=N` flag per shell choice.

### 11. Correct origin

z88dk `+ti83p` handles memory origin automatically.
Don't override unless you know what you're doing.

### 12. Three layers

Source → Machine code → TI container. Keep them separate in your head.

### 13. -create-app is for Flash apps only

For .8xp: compile to binary, then wrap with appmake/bin2var.
For .8xk: use `-create-app`.

### 14-15. Binary packaging

The wrapper stage must not reinterpret bytes. bin2var.ps1 does
byte-exact binary packaging with no encoding conversions.

### 16. Verify header before transfer

A valid .8xp starts with `**TI83F*`. tibuild checks this automatically.

### 17. Test minimal binaries first

hello.c exists for this reason. Build it. Send it. Run it.
Then debug your actual program.

### 18. Sendable ≠ Runnable

A file can be valid .8xp, accepted by TI Connect, visible on calc,
and still crash. Test on the actual calculator. That's the only truth.

---

## Z80 C Constraints

| Constraint        | Rule                                    |
|-------------------|-----------------------------------------|
| Float precision   | Use `float` (32-bit), not `double`      |
| Stack             | ~400 bytes — minimize local variables   |
| Screen            | 16×8 (83+) or 26×10 (84+) characters   |
| String length     | Keep short, screen wraps at 16/26 chars |
| Math library      | z88dk provides `math.h` for Z80         |
| Printf format     | `%g` is safest for floats via z88dk     |
| Input             | `scanf` works but limited               |
| Keyboard          | `getk()` polls — no interrupt-driven    |

---

## tibuild.ps1 Reference

```
tibuild.ps1 <source.c> <NAME> [options]

Options:
  -Target 8xp|8xk     Output format (default: 8xp)
  -Shell  ion|mirageos|dcs|none   Shell format (default: ion)
  -Clean               Remove build artifacts first
  -Verbose             Show compiler commands
```

---

## Workflow

```
Edit src\program.c
  |
.\tibuild.ps1 src\program.c PROGNAME
  |
Send out\PROGNAME.8xp to calculator
  |
Run on calculator
  |
Observe behavior
  |
Edit src\program.c  (loop)
```

That loop should be as fast as possible. That's the whole point.
