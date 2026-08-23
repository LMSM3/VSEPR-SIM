# VSIM Precompiler Design
## `.vsim-pre` → `.vsim` Transformation Pipeline

**Version:** v5.1.13.5 / precompiler-0.1-design  
**Status:** DESIGN ONLY — Not yet implemented  
**Tool name:** `vsim-precompile`  
**Part of:** Day 75 WO-75C / WO-75D

---

## 1. Overview

The VSIM precompiler is a standalone tool that transforms a modular `.vsim-pre` script into a flat `.vsim` script readable by the current VSIM runtime parser.

The runtime never changes. It always receives flat `.vsim`. The precompiler handles everything before that: interactive questions, variable substitution, conditional blocks, and external helper calls.

```
user writes:              vsim-precompile runs:        vsim runtime runs:
  .vsim-pre          →        flat .vsim           →      simulation
  (modular)               (standard sections)           (kernel + analysis)
```

This separation is intentional and permanent. The precompiler is not a parser upgrade. It is a code-generation front-end.

---

## 2. File Format

### Extension

| Extension | Meaning |
|---|---|
| `.vsim` | Standard VSIM script. Parsed directly by the runtime. |
| `.vsim-pre` | Modular precompiler input. Never passed to the runtime directly. |

A `.vsim-pre` file that is accidentally passed to the VSIM runtime will fail gracefully: the `[language]` block will be captured in `raw_sections`, and the `[module.*]` sections will be unrecognized. This is intentional — fail-safe, not silent-corrupt.

### Required header

Every `.vsim-pre` file must open with:

```toml
[language]
syntax_version               = "vsim-pre-0.1"
feature_status               = "design_only"
supported_by_current_parser  = false
requires_parser_upgrade      = false   # precompiler handles it, not the parser
```

`requires_parser_upgrade = false` is the correct value. The runtime does not need to change. Only the precompiler needs to exist.

---

## 3. Variable System

### Declaration

Variables are declared in `[module.variables]`:

```toml
[module.variables]
formation_parent_molecule = "CH4"
reaction_mode             = false
temperature_K             = 0.0
formation_reactants       = ["C", "H", "H", "H", "H"]
```

### Types

| TOML type | Variable type | Example |
|---|---|---|
| string | `str` | `"CH4"` |
| bool | `bool` | `false` |
| float/int | `num` | `300.0` |
| array of strings | `str[]` | `["C", "H"]` |

### Substitution rules

A value in any section that matches a declared variable name is substituted at compile time:

```toml
# In .vsim-pre:
[module.material]
formula = formation_parent_molecule     # bare name → substituted

# In compiled .vsim:
[material]
formula = "CH4"                         # literal value inserted
```

Substitution is applied after the asking loop resolves all variables. Order of operations:

1. Parse `[module.variables]` → build initial variable table
2. Run asking loop (if `[module.ask].interactive = true`) → update table
3. Evaluate all `enabled_if` conditions against final table
4. Substitute variables into all section values
5. Emit flat `.vsim`

### Scope

All variables are global within the file. There is no block-local scope in V1.

---

## 4. Asking Loop

### Declaration

```toml
[module.ask]
enabled            = true
interactive        = true
loop_until_confirmed = true
```

`loop_until_confirmed = true` means: after all questions are answered, show a summary and ask "Confirm? [true]". If the user answers `false`, restart the loop from question 1.

### Question blocks

Each `?` identifier declares one question:

```toml
? formation_parent_molecule
prompt   = "Parent molecule / target product?"
default  = formation_parent_molecule
examples = ["CH4", "NH3", "H2O", "CO2", "C2H6"]

? reaction_mode
prompt   = "Enable reaction pathway mode?"
default  = false
choices  = [true, false]

? formation_reactants
prompt      = "Reactants?"
default     = formation_reactants
enabled_if  = "reaction_mode == true"
```

### `?` block fields

| Field | Type | Required | Meaning |
|---|---|---|---|
| `prompt` | string | yes | Text shown to user before `[default]` |
| `default` | variable ref or literal | yes | Value shown in brackets; used if user presses Enter |
| `choices` | list | no | If set, input must be one of these values |
| `examples` | list | no | Shown as hints; not a validation constraint |
| `enabled_if` | expression | no | Skip this question if expression is false |
| `type` | `str`/`bool`/`num`/`str[]` | no | Inferred from default if omitted |

### Terminal protocol

```
VSIM Pre-run Configuration
──────────────────────────
[1/4] Parent molecule / target product? [CH4]
      Examples: CH4, NH3, H2O, CO2, C2H6
> NH3

[2/4] Enable reaction pathway mode? [false] (true/false)
> true

[3/4] Reactants? [N,H,H,H]
> N,H,H,H

[4/4] Temperature in Kelvin? [0.0]
> (enter)

──────────────────────────
Summary:
  formation_parent_molecule = "NH3"
  reaction_mode             = true
  formation_reactants       = ["N", "H", "H", "H"]
  temperature_K             = 0.0

Confirm? [true] (true/false)
> true

Compiling: day75_flat_nh3_compiled.vsim
```

### Non-interactive mode

`--non-interactive` flag skips all prompts, uses all defaults. For CI and batch runs. The compiled output is identical to what you'd get if the user pressed Enter for every question.

### Input validation

- `choices` constraint: if input is not in the list, re-prompt once, then use default.
- `str[]` type: accepts `A,B,C` comma-separated or `["A","B","C"]` TOML-style.
- Empty input: always treated as "use default."
- Ctrl+C: abort with exit code 2, no output file written.

---

## 5. `enabled_if` Conditional Evaluation

### Syntax

```toml
enabled_if = "<variable> <op> <literal>"
```

### Supported operators

| Op | Example | Meaning |
|---|---|---|
| `==` | `reaction_mode == true` | Equality |
| `!=` | `reaction_mode != false` | Inequality |
| `>` | `temperature_K > 0.0` | Greater than (numeric only) |
| `<` | `temperature_K < 1000.0` | Less than (numeric only) |

### Where `enabled_if` can appear

- On `?` question blocks — skips the question if false
- On `[module.*]` section headers — omits the entire section from compiled output if false
- On individual key-value pairs inside a module section (V2, not V1)

### Evaluation timing

All `enabled_if` conditions are evaluated once, after the asking loop completes, using the final variable table. They are not re-evaluated mid-compilation.

---

## 6. Module Namespace → Flat Section Translation

### Translation table

| `.vsim-pre` section | Compiled `.vsim` section |
|---|---|
| `[module.project]` | `[project]` |
| `[module.variables]` | *(consumed by precompiler, not emitted)* |
| `[module.ask]` | *(consumed by precompiler, not emitted)* |
| `[module.material]` | `[material]` |
| `[module.formation]` | `[simulation]` + `[[simulation.molecule]]` |
| `[module.reaction]` | *(consumed — drives helper call, not emitted directly)* |
| `[module.simulation]` | `[simulation]` (merged with formation output) |
| `[module.observe]` | `[observe]` |
| `[module.verify]` | `[verify]` + `[verify.*]` subsections |
| `[module.export]` | `[export]` |
| `[module.report]` | `[report]` |

### Merge rules

When both `[module.formation]` and `[module.simulation]` contribute to `[simulation]` in the output, the merge order is:

1. `[module.formation]` provides: `formation_preset`, molecule entry
2. `[module.simulation]` provides: `fire_max_steps`, `fire_dt_fs`, `periodic`, `use_ewald`, etc.
3. Explicit values in `[module.simulation]` override defaults from `[module.formation]`

### Provenance block

The compiled `.vsim` always receives a `[report.provenance]` block generated by the precompiler:

```toml
[report.provenance]
source_file         = "day75_modular.vsim-pre"
compiled_file       = "day75_flat_compiled.vsim"
compiler_version    = "vsim-precompile-0.1"
compiled_at         = "2026-06-02T09:00:00"
formation_parent    = "CH4"
reaction_mode_used  = false
pathway_helper_used = false
asking_loop_used    = true
variables_set_by_user = ["formation_parent_molecule"]
variables_used_default = ["reaction_mode", "temperature_K", "formation_reactants"]
```

---

## 7. CLI Specification

```
vsim-precompile <input.vsim-pre>
                [--out <output.vsim>]
                [--non-interactive]
                [--dry-run]
                [--set <key>=<value> ...]
                [--helper-path <path>]
                [--no-helper]
                [--verbose]
```

| Flag | Meaning |
|---|---|
| `--out <path>` | Output file path. Default: `<input_stem>_compiled.vsim` in same directory |
| `--non-interactive` | Skip asking loop, use all defaults |
| `--dry-run` | Parse and validate, print compiled output to stdout, do not write file |
| `--set key=value` | Override a variable before the asking loop. Stackable. |
| `--helper-path <path>` | Override default search path for `vsim_formation_pathway_helper` |
| `--no-helper` | Disable reaction helper even if `reaction_mode = true`; degrade to relaxation |
| `--verbose` | Print variable table, substitution log, and section translation steps |

### Exit codes

| Code | Meaning |
|---|---|
| 0 | Success, output file written |
| 1 | Parse or validation error |
| 2 | User aborted (Ctrl+C) |
| 3 | Helper call failed and `on_missing_helper` was not `warn_and_continue` |

---

## 8. Implementation Order (V1)

This is the minimum viable precompiler:

```
Phase 1 — Parser
  Read .vsim-pre file
  Parse [language] block → reject if not vsim-pre-0.1
  Parse [module.variables] → build variable table
  Parse [module.ask] + ? blocks → build question list
  Parse all [module.*] sections → store as raw section map

Phase 2 — Asking Loop
  If interactive: run terminal asking loop, update variable table
  If non-interactive: skip, use defaults

Phase 3 — Conditionals
  Evaluate all enabled_if expressions against final variable table
  Mark disabled sections and questions

Phase 4 — Helper Call
  If reaction_mode == true and helper is available:
    Build helper command from module.reaction.helper_call
    Execute, capture output JSON
    Store pathway data for injection into report
  If helper missing: log warning, continue as relaxation

Phase 5 — Substitution + Emit
  For each enabled [module.*] section:
    Apply variable substitution to all values
    Translate to flat section name
    Write to output buffer
  Append [report.provenance] block
  Write output .vsim file
```

---

## 9. What V1 Does Not Include

- GUI asking loop (terminal only)
- Back-navigation in asking loop (forward-only)
- Block-local variable scope
- Import / include directives (`include "other.vsim-pre"`)
- Looping constructs in the precompiler (batch sweep belongs in `[batch]` in the flat output)
- Runtime variable substitution (all substitution happens at compile time, before the VSIM parser sees the file)
- The `?` operator inside non-`[module.ask]` sections (V2 stretch)
