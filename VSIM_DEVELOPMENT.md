# Adding a new VSIM feature — Development Checklist

> **Purpose:** Quick reference for adding a new `.vsim` script capability.  
> **Audience:** Future developers on VSEPR-SIM.  
> **Example:** We added `render_interval` in WO-57D. This is how to do it.

---

## The workflow (5 steps)

### Step 1: Define the field in `vsim_document.hpp`

**File:** `include/vsim/vsim_document.hpp`

1. Find the struct that owns the field (e.g., `VisualSection`).
2. Add the field with a sensible default:
   ```cpp
   struct VisualSection {
	   // ... existing fields ...

	   int render_interval = 1;   // steps; <= 0 is treated as 1

	   bool should_render(int step) const {
		   int ri = render_interval > 0 ? render_interval : 1;
		   return (step % ri) == 0;
	   }
   };
   ```
3. Add a helper method if the field controls branching logic.
4. Update the struct docstring to note which WO introduced it (e.g., `WO-57D`).

**Key rule:** Every field must have a default. No uninitialized state.

---

### Step 2: Wire the parser

**File:** `src/vsim/vsim_parser.cpp`

1. Find the applier function for your section (e.g., `apply_visual_key()`).
2. Add a case for your field:
   ```cpp
   else if (key == "render_interval")  doc_.visual.render_interval = static_cast<int>(as_num());
   ```
3. Test the round-trip:
   ```toml
   [visual]
   render_interval = 25
   ```
   Parse it. Check `doc.visual.render_interval == 25`.

**Key rule:** The parser should accept your field. No errors. Bad values → defensive defaults (e.g., `<= 0` treated as `1`).

---

### Step 3: Wire the runtime

**File(s):** `apps/beta10_demo.cpp`, `src/vis/viz_router.cpp`, `include/vsim/vsim_runtime.hpp`, etc.

**Depends on what your field does:**

#### If it controls a boolean flag (e.g., a feature toggle):
```cpp
if (doc.some_section.my_flag) {
	// do something
}
```

#### If it's a numeric parameter (e.g., cadence):
```cpp
if (doc.visual.should_render(current_step)) {
	VsimRenderLayer::dispatch(doc, payload);
}
```

#### If it's a list or configuration:
```cpp
for (const auto& target : doc.visual_external.render_targets) {
	execute_render_target(target, doc);
}
```

**Key rule:** Keep runtime usage simple. Defensive: check bounds, handle zero/negative, never assume a value is set.

---

### Step 4: Write tests

**File:** `tests/test_<feature>.cpp` (new or existing)

Test categories:

| Category | What to check |
|---|---|
| **Parse** | Script → struct field round-trip |
| **Default** | Field has correct default value |
| **Guard** | Defensive: `<= 0` treated as `1`, etc. |
| **Logic** | Branching / helper method behavior |
| **Edge case** | Boundary values, empty lists, etc. |
| **Interaction** | Field works with related fields |

Example (WO-57D `render_interval`):

```cpp
// RI1: Default = 1 renders every step
assert(vis.render_interval == 1);
auto frames = collect_render_steps(vis, 5);
assert(frames.size() == 6);  // 0,1,2,3,4,5

// RI2: render_interval = 10 renders multiples of 10
vis.render_interval = 10;
frames = collect_render_steps(vis, 50);
assert(frames == {0, 10, 20, 30, 40, 50});

// RI3: render_interval = 0 guarded → behaves as 1
vis.render_interval = 0;
frames = collect_render_steps(vis, 3);
assert(frames.size() == 4);  // guarded to 1

// RI4: Parser round-trip
VsimDocument doc = VsimParser::parse_string("[visual]\nrender_interval = 25");
assert(doc.visual.render_interval == 25);
```

**Register the test** in `tests/CMakeLists.txt`:

```cmake
# Group XX: WO-XXXX — feature name
add_executable(test_feature test_feature.cpp)
target_include_directories(test_feature PRIVATE ...)
vsepr_add_test(NAME FeatureTest TARGET test_feature LABELS vsim feature beta8 quick)
```

**Key rule:** Test the happy path, the guard path, and the parser. Don't test the entire kernel — just your feature's logic.

---

### Step 5: Document

#### A. Update `VSIM_REFERENCE.md`

**File:** `VSIM_REFERENCE.md` (workspace root)

1. Find the section table that owns your field.
2. Add a row:
   ```markdown
   | `render_interval` | int | `1` | ✅ | Emit a render / export frame every N simulation steps. |
   ```
3. Status symbol:
   - ✅ = wired to kernel (Step 3 done, test passes)
   - 🔶 = parsed and stored, not wired
   - ❌ = not yet parsed (planned only)

4. Example `.toml` snippet in the section.

#### B. Update `VSIM_LANGUAGE.md`

**File:** `docs/VSIM_LANGUAGE.md`

1. Find or create the section that documents your field's semantic meaning.
2. Add doctrine / explanation:
   ```markdown
   | `render_interval` | int | `1` | Emit a render frame every N simulation steps. Orthogonal to `display_fps`. |
   ```
3. Example usage.

#### C. Update `STAGE.md`

**File:** `STAGE.md` (workspace root)

In the appropriate work-in-progress section (e.g., "Beta-8 Stack" or "Phase N Gate"), add a gate check:

```markdown
| `render_interval` field added to `VisualSection` | ✅ |
| `render_interval` wired to parser | ✅ |
| `render_interval` gated in dispatch | ✅ |
| `render_interval` tests pass (Group 35) | ✅ |
| `render_interval` documented | ✅ |
```

When the feature is complete, create a summary block and mark it **COMPLETE**.

---

## Anatomy of a complete feature (WO-57D example)

### 1. Definition
**File:** `include/vsim/vsim_document.hpp` (lines 442–505)
```cpp
struct VisualSection {
	int render_interval = 1;
	bool should_render(int step) const { ... }
};

struct VisualExternalSection {
	int render_interval = 1;
	bool should_render(int step, int visual_interval = 1) const { ... }
};
```

### 2. Parser
**File:** `src/vsim/vsim_parser.cpp` (lines 324–380)
```cpp
void VsimParser::apply_visual_key(const std::string& key, const Value& val, ...) {
	else if (key == "render_interval") doc_.visual.render_interval = static_cast<int>(as_num());
}

void VsimParser::apply_visual_external_key(...) {
	else if (key == "render_interval") doc_.visual_external.render_interval = static_cast<int>(as_num());
}
```

### 3. Runtime
**Files:** `apps/beta10_demo.cpp` (line 409), `apps/kernel_demo.cpp` (lines 581, 629)
```cpp
if (doc.visual.should_render(doc.simulation.fire_max_steps))
	VsimRenderLayer::dispatch(doc, rp);
```

### 4. Tests
**File:** `tests/test_render_interval.cpp` (Group 35)
- RI1: Default behavior
- RI2: Step gating
- RI3: Guard path
- RI4: Parser round-trip
- RI5: Override logic
- RI6: Fallback logic

### 5. Documentation
- `VSIM_REFERENCE.md` — field table, status ✅, example
- `VSIM_LANGUAGE.md` — semantic explanation, orthogonal field docstring
- `STAGE.md` — gate table marking WO-57D complete

---

## Decision tree

**Q: Do I need a helper method?**  
A: Yes, if the field controls branching logic (like `should_render()`). No if it's just a parameter passed through.

**Q: Should the field be in multiple structs?**  
A: Yes, if it applies at different scopes (e.g., `render_interval` in both `VisualSection` and `VisualExternalSection` for hierarchy/override).

**Q: How many tests do I need?**  
A: Minimum: parse round-trip + default + guard. Add: edge cases, interaction with related fields.

**Q: What status symbol should I use?**  
A: ✅ only after the test passes AND the runtime uses it. Interim? 🔶. Planning? ❌.

**Q: Do I update STAGE.md immediately?**  
A: No. When the feature is complete (steps 1–5 done + tests pass), add a gate table and mark it **COMPLETE**.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Field has no default in struct | Add `= <sensible_value>` to every field. |
| Parser accepts field but doesn't assign | Add the `if` case in applier function. |
| No defensive check (e.g., `<= 0`) | Add guard: `int ri = value > 0 ? value : default;` |
| Test only happy path | Add guard test, edge cases, parser round-trip. |
| Documentation not updated | Update all three: `VSIM_REFERENCE.md`, `VSIM_LANGUAGE.md`, `STAGE.md`. |
| New struct not integrated into `VsimDocument` | Make sure the struct is a member of `struct VsimDocument` — no orphaned structs. |
| Helper method has side effects | Helpers should be pure (query logic only). Side effects belong in runtime dispatch. |

---

## Checklist (copy-paste template)

```
Feature: ____________________________________________

Step 1: Define in vsim_document.hpp
  [ ] Add field with default
  [ ] Add helper method (if needed)
  [ ] Update struct docstring

Step 2: Wire parser
  [ ] Add case in applier function
  [ ] Test round-trip

Step 3: Wire runtime
  [ ] Use field in runtime dispatch
  [ ] Defensive checks in place
  [ ] Builds without errors

Step 4: Write tests
  [ ] Parse round-trip test
  [ ] Default value test
  [ ] Guard / edge case test
  [ ] Logic / branching test
  [ ] Register in CMakeLists.txt
  [ ] Tests pass: `ctest --labels <feature>`

Step 5: Document
  [ ] VSIM_REFERENCE.md — field table + status ✅
  [ ] VSIM_LANGUAGE.md — semantic explanation
  [ ] STAGE.md — gate table + COMPLETE marker

Done!
  [ ] Feature is parsed, wired, tested, and documented
  [ ] No future developer needs to guess how it works
```

---

*Last updated: WO-57D (Day #57). Process validated through `render_interval` implementation.*
