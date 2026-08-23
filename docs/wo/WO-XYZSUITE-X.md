# WO-XYZSUITE-X — .X Saved Executable Suite Format
<!-- WO-XYZSUITE-X | VSEPR-SIM v5.1.3~2 | branch: v5.0.0-main -->

## Decision

`.X` is the saved-run suite descriptor for VSEPR-SIM.

It is not a coordinate file.  
It is not a trajectory.  
It is not a checkpoint.  
It is not the script language.

It is the file that says:

> Here is the saved system.  
> Here are the files.  
> Here is what to compile.  
> Here is what to run.  
> Here is what to validate.  
> Here is what to replay.

---

## XYZSuite Model

```
XYZSuite
│
├── input
│   └── .vsim              script language entry point
│
├── minimal state
│   └── .xyz               screenshot: atom count + element + x y z
│
├── enriched state
│   └── .xyza              screenshot + position/velocity/force/charge/energy
│
├── checkpoint
│   └── .xyzc              restartable: frame, time, dt, seed, integrator state
│
├── trajectory
│   └── .xyzf              frame sequence for replay, RDF, MSD, diffusion
│
├── rich replay
│   └── .xyzFull           trajectory + metadata + energy layers + CAD tags
│
└── suite descriptor
	└── .X                 executable manifest: points to all of the above
```

---

## .X File Format

INI-style sections. `key = value`. `#` comments.  
Quoted strings optional. Unknown keys silently ignored.

### Required sections

**[xsuite]**  
```ini
[xsuite]
name = "nacl_pbc_supercell"
version = "5.2"
mode = "saved_run_suite"
created_by = "vsepr"
```

**[run]**  
```ini
[run]
entry = "nacl_pbc_supercell.vsim"
mode = "crystal"
num_steps = 10000
dt = 1.0e-15
```

**[files]**  
```ini
[files]
script = "nacl_pbc_supercell.vsim"
snapshot = "state/nacl.xyz"
checkpoint = "state/nacl.xyzc"
trajectory = "state/nacl.xyzf"
rich = "state/nacl.xyzFull"
```

### Optional sections

**[build]**  
```ini
[build]
enabled = true
compiler = auto
target = vsepr
config = release
```

**[hash]** (all fields optional; future integrity gate)  
```ini
[hash]
script_hash = ...
snapshot_hash = ...
suite_hash = ...
```

**[actions]**  
```ini
[actions]
compile = false
run = true
validate = true
replay = false
export = true
```

**[outputs]**  
```ini
[outputs]
report = reports/nacl.md
json = reports/nacl.json
log = logs/nacl.log
```

---

## CLI Commands

```
vsepr x inspect  <file.X>   show suite contents and file status
vsepr x validate <file.X>   check file presence and contracts
vsepr x run      <file.X>   compile (if enabled), then run entry script
vsepr x replay   <file.X>   open trajectory/rich replay in desktop viewer
vsepr x export   <file.X>   regenerate reports and artifacts
vsepr x open     <file.X>   double-click launcher: 2 CMD + Qt desktop
```

### `x run` flow

```
1. parse .X
2. validate file presence
3. cmake --preset release  (if build.enabled = true)
4. vsepr run <entry_script>
5. export outputs  (if actions.export = true)
```

### `x open` (double-click gate)

Spawns three windows simultaneously:

| Window | Process | Content |
|---|---|---|
| 1 | `cmd.exe` | `vsepr x run <file.X>` — simulation console |
| 2 | `cmd.exe` | `vsepr x validate <file.X>` — status console |
| 3 | `vsepr-desktop.exe` | Qt viewer, `--xsuite <file.X>` |

Register `.X` as the default handler:

```bat
apps\launcher\register_x_extension.bat   (run as Administrator)
```

---

## Source Files

| File | Role |
|---|---|
| `include/vsim/xsuite.hpp` | `XSuiteFile`, parser, `xsuite_inspect()`, `xsuite_validate()` |
| `src/cli/cmd_x_suite.hpp` | `cmd_x_suite()` declaration |
| `src/cli/cmd_x_suite.cpp` | 6 sub-command implementations |
| `apps/vsepr.cpp` | `if (cmd == "x")` routing block |
| `apps/launcher/xsuite_launcher.bat` | Double-click launcher |
| `apps/launcher/register_x_extension.bat` | Windows file-type registration |
| `tests/test_xsuite_parse.cpp` | 5 parse/validate tests (XP1–XP5) |

---

## Lean File Doctrine

| Extension | What it is |
|---|---|
| `.xyz` | Frozen visible state — positions only |
| `.xyza` | State + dynamic fields (velocity, force, charge) |
| `.xyzc` | Restartable checkpoint |
| `.xyzf` | Replay frame sequence |
| `.xyzFull` | Rich replay + metadata + energy + CAD tags |
| `.X` | Executable manifest pointing to all of the above |

### What `.xyz` is not

No modulus. No inferred chemistry. No macro labels.  
It stores what you can see. Derived conclusions live outside the file.

### What `.X` is not

Not raw state. Not trajectory. Not checkpoint. Not script.  
It is the file that compiles, runs, validates, and replays the suite.

---

## Relationship to WO-ISOLATE-01

`WO-ISOLATE-01` is the background recovery plan:  
`collect → classify → preserve → modernize → test → promote`

`.X` is the forward-facing operational layer:  
`save → hash → run → validate → replay → export`

Both are live simultaneously.  
The isolate catches historical artifacts.  
`.X` runs current and future suites.

---

## Test Cases

| ID | Description | Expected |
|---|---|---|
| XP1 | Minimal valid .X | parse ok, fields populated |
| XP2 | Missing entry script | parse fails, error non-empty |
| XP3 | Missing optional trajectory | parse ok, warning issued |
| XP4 | Hash field present | parse ok, values captured, no crash |
| XP5 | Action flags | compile/run/validate/replay/export all parsed correctly |

---

*WO-XYZSUITE-X — v5.1.3~2*
