# Post-Day Tracking

This directory is the continuity ledger for Day 89 onward. It is designed to grow to roughly 50 Markdown records without turning the repository root or general `docs/` tree into the daily work queue.

## Authority order

1. Current user direction.
2. The day’s authoritative `.tex` overview.
3. The day map and individual letter records in this directory.
4. Verified implementation, tests, artifacts, commits, and pull requests.
5. Historical root/docs material as design lineage only.

## Naming

```text
DAY##_OVERVIEW.md
DAY##_A_short_name.md
DAY##_B_short_name.md
...
DAY##_V_short_name.md
```

Each day uses letters `A` through `V` for active work. The final four letters are always reserved:

```text
W  reserved for late critical work
X  reserved for cross-cutting correction
Y  reserved for release/yield evidence
Z  reserved for final close or emergency recovery
```

Do not pre-assign W–Z during ordinary planning. Their unused state is intentional capacity.

## Record header

Every record begins with:

```text
[W] Day/letter work identity | Authority: path/to/authoritative.tex
[D] Day and continuity state
[I] TODO | ACTIVE | PARTIAL | DONE | BLOCKED
[V] NONE | DISCOVERY | BUILD | TEST | PASS | FAIL
[P] LOCAL | COMMITTED <hash> | PUSHED <branch> | PR <url>
[N] One next action or blocker
```

## Required evidence lineage

```text
requirement
  -> authoritative two-page TeX
  -> implementation paths
  -> validation
  -> artifacts
  -> commit/PR or carry-forward record
```

Each active letter item must gain its own approximately two-page authoritative TeX before it can be marked complete. The day overview TeX governs the map but does not replace item-level scientific or technical authority.

## Continuity

- Completed evidence is append-only.
- Corrections identify the original claim and replacement evidence.
- Detours are marked `INTEGRATED`, `PARALLEL`, or `OUT OF SCOPE`.
- Day rollover requires a close/open ledger.
- Historical files under `docs/` may guide direction but are not automatically current authority.
