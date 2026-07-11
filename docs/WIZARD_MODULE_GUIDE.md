# VSEPR-SIM Script Wizard — Module Developer Guide

> **File:** `docs/WIZARD_MODULE_GUIDE.md`  
> **Maintained by:** VSEPR-SIM development team  
> **Applies to:** `src/cli/cmd_new_wizard.hpp`  
> **Version:** v5.1.4 — WO-72K-EXT

---

## Overview

The VSEPR-SIM script wizard is an on-rails interactive prompt that activates
when the user runs `vsepr` with no arguments.  After viewing the welcome
screen the user presses `N` to enter the wizard.  The wizard walks them
through each `.vsim` language module step-by-step, collects their answers,
and writes a syntactically valid `.vsim` file to disk.  It then optionally
runs `vsepr validate` on the generated file before exiting.

The wizard is entirely contained in a single header:

```
src/cli/cmd_new_wizard.hpp
```

No other files need to be modified when a new module is added.

---

## Architecture

### Data model

The wizard is driven by a flat table of `WizardModule` descriptors:

```
WizardModule
├── section       string    "[run]"
├── title         string    "Run Parameters"
├── description   string    shown before prompts
├── required      bool      false → user asked "include this section?"
└── fields        vector<WizardField>
	├── key           string    INI key written to file
	├── description   string    prompt text
	├── default_value string    used when user presses Enter
	├── type          FieldType Text | Integer | Float | Choice | Bool
	├── required      bool      field must have a value
	├── choices       vector<WizardChoice>   populated when type = Choice
	├── tip_text      string    contextual tip printed above the prompt
	└── example       string    inline example shown next to prompt
```

### Execution flow

```
vsepr (no args)
  └─ show_welcome()
	   └─ prompt "Press N for wizard"
			└─ cmd_new_wizard()
				 for each WizardModule in wizard_modules():
				 │   if !required: ask_yn("Include [section]?")
				 │   collect_module() → accumulates .vsim text
				 └─ write_script() → <name>.vsim in cwd
					  └─ if user wants: cmd_validate(<file>)
```

### Return codes from `cmd_new_wizard()`

| Return | Meaning |
|--------|---------|
| `0`    | Success, file written |
| `1`    | Aborted by user or write failure |
| `2`    | Sentinel — caller should run `vsepr validate` on newest `.vsim` in cwd |

---

## How to add a new `.vsim` module to the wizard

All module registrations live in the `wizard_modules()` function inside
`cmd_new_wizard.hpp`.  The function returns a `const std::vector<WizardModule>&`
built from a single `static const` initialiser.

### Step-by-step

**1. Locate the insertion point in `wizard_modules()`**

Open `src/cli/cmd_new_wizard.hpp` and find the `MODULES` vector.
Modules are shown to the user in declaration order.  Insert your new entry
in the logical position (e.g. after `[run]` if it is a run-time option,
after `[export]` if it controls output).

**2. Write a `WizardModule` struct literal**

```cpp
// Example: adding a new [physics] module
{
	"[physics]",                              // .vsim section header
	"Physics Model",                          // wizard step title
	"Selects the force field and Coulomb treatment.",  // one-liner
	/*required=*/false,                       // optional → user asked first
	{
		// Each WizardField maps to one INI key
		{
			"coulomb",                        // key name in .vsim
			"Coulomb interaction model",      // prompt text
			"none",                           // default value
			FieldType::Choice,
			/*required=*/false,
			{                                 // choices (value, description)
				{"none",  "No Coulomb (LJ only)"},
				{"ewald", "Particle-mesh Ewald sum (ionic crystals)"},
			},
			"Use ewald for NaCl, MgO, and other ionic systems.",  // tip
			""                                                      // example
		},
		{
			"lj_cutoff_A",
			"Lennard-Jones cutoff radius (Angstrom)",
			"8.5",
			FieldType::Float,
			/*required=*/false, {},
			"Must be < L/2 for the smallest box dimension.", "8.5"
		},
	}
},
```

**3. Rebuild**

```
cmake --build build --target vsepr
```

No other files need changing.  The new module will appear as a numbered
wizard step the next time `vsepr` is run with no arguments.

---

## Field type reference

| `FieldType` | User sees | Written to file |
|------------|-----------|-----------------|
| `Text`     | Free-text prompt with `[default]` | `key = <value>` |
| `Integer`  | Free-text prompt (validated) | `key = <integer>` |
| `Float`    | Free-text prompt (validated) | `key = <float>` |
| `Choice`   | Numbered menu, Enter = default | `key = <choice.value>` |
| `Bool`     | `[Y/n]` / `[y/N]` prompt | `key = true` or `key = false` |

---

## Special-cased modules

Some modules require richer interactive behaviour than the generic field
loop can provide.  They are identified by their `section` string and handled
in `collect_module()`:

| Section | Special behaviour |
|---------|------------------|
| `[objects]` | After the system-type choice, calls `build_objects_block()` which asks type-specific sub-questions (lattice, species, supercell, vacuum gap, etc.) |

To add a new special-cased module:

1. Add the module entry normally in `wizard_modules()` with a single
   sentinel field (or no fields).
2. Add an `if (mod.section == "[your_section]")` branch in
   `collect_module()` that calls your own builder function.
3. Return the completed block string from the builder.

---

## Colour and formatting helpers (`wiz::` namespace)

All formatting is done through lightweight helpers in the `wiz::` namespace
inside the header.  Do not use `Display::*` from `display.hpp` — the wizard
is intentionally self-contained.

| Helper | Effect |
|--------|--------|
| `wiz::section_bar(title, step, total)` | Prints the `┌─ STEP n/N  │  title` bar |
| `wiz::field_prompt(key, desc, def, ex)` | Prints `key  │  desc  [default]  e.g.` |
| `wiz::read_line(default_value)` | Returns user input or default on empty Enter |
| `wiz::ask_yn(question, def_yes)` | Returns `true` for y/Y/yes inputs |
| `wiz::tip(text)` | Prints an indented `ℹ text` line |

ANSI colours are declared as `constexpr const char*` at the top of the
`wiz::` namespace.  VT processing is enabled by `show_welcome()` before the
wizard is called.

---

## Acceptance checklist for a new module

Before merging a new module entry, verify:

- [ ] Section name exactly matches the `[section]` header in `vsim_document.hpp`
- [ ] All `default_value` strings are valid for their field type
- [ ] All `Choice` entries are values the parser accepts in that field
- [ ] Optional module has `required = false`
- [ ] `VSIM_REFERENCE.md` is updated if new fields are being documented
- [ ] `vsepr validate <generated_file>` reports `OK` after filling in the new section
- [ ] `vsepr doctor integratedtest` still reports `PASS  37/37`

---

## Testing the wizard locally

Run the wizard with piped input to test without manual interaction:

```powershell
# Accepts all defaults: project name "smoke", crystal, relax, no optional sections
"N`nsmoke`n`n`n`n`ny`n1`n`n`n`n`n`ny`n`n`n`n`n`nn`nn`nn`nn`nsmoke.vsim`nn" |
	.\build\vsepr.exe
```

Then verify the output:

```powershell
Get-Content .\build\smoke.vsim
.\build\vsepr.exe validate .\build\smoke.vsim
```

---

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `FieldType::Choice` with empty `choices` vector | Choice menu shows nothing, index always 0 | Populate `choices` |
| `default_value` not in `choices` | Default index falls to 0 silently | Match one `choices[i].value` exactly |
| `required = true` on optional section | User cannot skip the section | Set `required = false` |
| Adding `FieldType::Bool` with default `"yes"` | Reads as `false` (only `"true"` works) | Use `"true"` or `"false"` as default_value |
| Unicode fill char in `std::string(N, c)` | Compiler warning + overflow | Use ASCII `-` as fill; embed box chars as UTF-8 string literals |

---

*Last updated: v5.1.4 — WO-72K-EXT*
