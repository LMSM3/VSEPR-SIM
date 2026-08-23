# Deprecation Policy — VSEPR-SIM

This document explains what makes a file, directory, or component eligible for
deprecation and how contributors should handle the transition from active to legacy.

---

## What "active" means

A file or component is **active** if it is:

1. Compiled or packaged by the root `CMakeLists.txt` **or** `installer/setup.iss`, **and**
2. Reachable by a runtime code path (i.e., something calls or loads it), **and**
3. Covered by at least one entry in `tests/`.

If a file fails any of those three checks it is a candidate for deprecation review.

---

## Deprecation criteria

A file or component is eligible for deprecation when:

| Condition | Description |
|-----------|-------------|
| **Orphaned** | No longer referenced by any build rule, installer entry, or import/include |
| **Superseded** | Replaced by a newer implementation in the same or a sibling module |
| **API-breaking** | Its interface was broken by a refactor and was not updated |
| **Duplicate** | Another file provides the same functionality and is more up to date |
| **Version-locked** | The file was written against a now-abandoned architecture (e.g., v2, v3, pre-v4) |

---

## How to deprecate a file

1. **Do not delete immediately.** Move the file to one of the designated archive
   directories below, or add a `DEPRECATED:` header comment explaining why and
   pointing to the replacement.

2. **Update the archive directory's `README.md`** to list the file, its original
   purpose, and why it was deprecated.

3. **Remove it from the build.** Ensure `CMakeLists.txt` and `installer/setup.iss`
   do not reference it.  Run a build to verify no ghost entries remain.

4. **Update `VSIM_REFERENCE.md`** if the file was part of the VSIM language or
   schema surface area.

5. **Commit with tag prefix `deprecate(...):`** so the change is easy to find in
   git history.

---

## Archive directories

| Directory | Use for |
|-----------|---------|
| `legacy/` | Pre-v4 C API code, old data files, superseded interfaces |
| `archive/` | Frozen build-system snapshots (CMakeLists, CI configs, etc.) |
| `bridge_beta/` | Experimental scaffolding that is not yet ready for the active build |
| `v4/` | v4-era standalone experiments absorbed into the v5 kernel |
| `v5/` | Early v5 prototype demos superseded by production code in `src/` |

Each of these directories has a `README.md` that lists its contents and explains
its status.  Do not add new code to `legacy/` or `archive/` — those are frozen.
Use `bridge_beta/` for work-in-progress scaffolding that is intentionally isolated.

---

## "Legacy fallback" vs. "active component"

| Term | Meaning |
|------|---------|
| **Active component** | Compiled, packaged, tested, and part of the shipped installer |
| **Legacy fallback** | Preserved in an archive directory; may be referenced for historical context but is never compiled or shipped |
| **Experimental scaffold** | Lives in `bridge_beta/`; under development but not wired into the active build |

A legacy fallback is **never** a safe substitute for an active component in new
code.  If you need behavior from a legacy file, port the logic into the active
codebase and write a test for it.

---

## When NOT to deprecate

- A file that is only missing from one configuration (e.g., optional visualization
  target).  Use `skipifsourcedoesntexist` in the installer instead.
- A file that is unused in the main executable but exercised directly by tests.
- A header that is part of the public SDK surface (under `include/vsim/`) even if
  no internal code imports it yet — external consumers may depend on it.

---

## Questions?

Open an issue on [GitHub](https://github.com/LMSM3/VSEPR-SIM/issues) with the
label `maintenance` or `deprecation`.
