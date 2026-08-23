# Day 88 Worktree Inventory

Refreshed: 2026-07-21 (local working tree)

- Branch: day84t-chemplus-declarative-vsepr
- HEAD: b4d6d9de6f1ae0bfa5c3f33979874b3d69b2e38c
- Current porcelain records: 19136
- Prior 2026-07-11 snapshot: 19428 records; 292 prior records are no longer present in the current status output and remain preserved by the earlier audit snapshot.
- Status split: 17941 tracked deletions, 677 tracked modifications, 518 untracked records
- No reset, clean, checkout, deletion, or staging action was performed.

## Classification

| Class | Count | Treatment |
|---|---:|---|
| deletion | 14888 | Require owner and reference check |
| generated | 3053 | Regenerate/exclude; do not stage by default |
| legacy | 14 | Archive/migrate deliberately |
| scoped | 10 | Validate and isolate for Day 88 |
| unresolved | 1171 | Preserve; ownership unresolved |

## Largest top-level sets

| Path | Count |
|---|---:|
| benchmark_v2 | 7176 |
| benchmark_100 | 7146 |
| build_test | 2921 |
| fire_smooth_trace | 200 |
| tests | 197 |
| src | 189 |
| include | 173 |
| examples | 127 |
| apps | 124 |
| reports | 112 |
| docs | 109 |
| scripts | 109 |
| coarse_grain | 76 |
| atomistic | 75 |
| build_vs | 75 |
| pykernel | 73 |
| build_56c | 35 |
| runs | 35 |
| sim | 26 |
| FinalChapter | 25 |
| figures | 15 |
| tools | 15 |
| build-linux | 14 |
| chem | 13 |
| data | 11 |

## Risk statement

The deletion count is dominated by benchmark_v2 and benchmark_100. Newly appearing paths are classified as unresolved unless they are part of the Day 88 audit or methodology record. No deletion, reset, clean, checkout, or staging action was performed during this refresh. Unresolved source, test, documentation, and script paths require owner/scope review before finalization.
