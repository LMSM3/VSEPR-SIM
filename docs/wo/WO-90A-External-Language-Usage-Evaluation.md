# WO-90A - External Language Usage Evaluation

## Identity

| Field | Value |
|---|---|
| Work order | `WO-90A` |
| Status | Low-priority decision complete; no runtime integration authorized |
| Priority | Deferred research benchmark; not on the active VSIM delivery path |
| Scope | Decide the permitted VSIM relationship to Q#, Q+, and K |
| Owns | Naming, compatibility, and external-tool boundaries only |
| Does not own | Simulation truth, kernel physics, MolecularDB truth, or visual-layer semantics |

## Why WO-90A

The existing source work-order sequence reaches WO-88 and does not contain a
WO-90. The `A` suffix identifies this as the first decision gate in the new
number. It does not reopen WO-72 through WO-77, whose numbers already carry
other implementation histories.

## Letter A - Usage Evaluation

Letter A answers one question for each candidate: **what, if anything, may
VSIM use it for without adopting, copying, rebranding, or making it a hidden
source of scientific truth?**

| Candidate | Verified meaning | Local meaning / collision | Usage decision | Gate |
|---|---|---|---|---|
| Q# | Microsoft's open-source quantum programming language and Quantum Development Kit | No local Q# language exists | **Low-priority external benchmark for a parallel model** | It may run outside VSIM only after a parallel model and a measurable comparison question are named; it is not VSIM syntax, a runtime dependency, or a replacement for the classical kernel |
| Q+ | No public programming language was verified under this name | Project docs already reserve Q+ for an explicit visual/analysis quantity layer | **Not a programming-language slot** | Retain Q+ as a declared display-only layer; reject language syntax, file extensions, or runtime claims using this name until a separately approved language specification exists |
| K | KX's proprietary implementation language underneath q / KDB-X | No local K integration exists | **Excluded** | Do not add K source, an embedded K evaluator, or a K dependency; q/KDB-X interchange can only be assessed later as a separately licensed external data adapter |

### A.1 Policy Result

Q# being a pre-existing language is a reason to keep it outside the proprietary
VSIM/Chem+ language boundary. Its only retained role is a low-priority
benchmark for a separately defined parallel research model. The project may
compare declared outputs against an external Q# program, but it must not
present Q# constructs as original VSIM, copy Q# grammar or standard-library
material into Chem+, or rebrand the Q# toolchain as a project subsystem.

The same boundary is stronger for K: KX documents K as an internal language
with no public documentation and explicitly discourages its use in q scripts.
That makes K unsuitable as an embedded dependency or as a target language for
this work order.

Q+ is a namespace decision, not a claim that a separate public language does
not exist anywhere. In the current project it already means a visual/analysis
quantity layer. The spelling remains reserved there until an independently
verified language proposal supplies ownership, grammar, license, toolchain,
and a reason it improves VSIM rather than duplicates it.

## Permitted Architecture

```text
VSIM / Chem+ run
  -> immutable exported inputs
  -> separately defined parallel research model
     -> optional Q# benchmark adapter + QDK result artifact
     -> future approved data adapter (not K source)
  -> typed comparison record
  -> validation/reward/report overlay

No external language writes directly to SimState, MolecularDB best-known
properties, calibration records, or renderer-owned state.
```

The adapter records source revision, tool version, input hash, command,
output hash, units, uncertainty, diagnostics, and license/status. Its output
is validation evidence, not automatic ground truth.

## Q# As A Deeper-System Design Benchmark

The purpose of the deferred Q# lane is not to turn VSIM into Q# or to claim
that a Q# circuit is automatically a molecular model. Its purpose is to help
develop a deeper **parallel research model** through disciplined comparison.

The parallel model is a separate, derived computation with its own explicit
state, encoder, decoder, and evidence records. It may consume a frozen
projection of a State Carrier, but it does not replace or mutate the atomistic
kernel state. Q# contributes a second implementation of a deliberately narrow
mathematical question after the question has been declared.

The design loop is:

```text
1. State a narrow model question.
2. Freeze a canonical system snapshot and projection rule.
3. Implement the parallel-model calculation from that snapshot.
4. Implement the equivalent declared proxy in external Q#.
5. Run both against the same immutable benchmark bundle.
6. Compare only the observables that have an explicit shared meaning.
7. Record agreement, disagreement, coverage limits, and diagnostics.
8. Use the report to improve the parallel-model hypothesis or fixture.
```

Q# can therefore instruct the design process only through a difference report:
which part of the projection, model assumption, numerical implementation, or
observable contract fails to agree. It never instructs the project by copying
its grammar, runtime, or standard library.

### Parallel-Model Boundary

The parallel model must declare all of the following before a Q# benchmark is
accepted:

| Concern | Required declaration |
|---|---|
| Purpose | The specific theoretical or computational question being explored |
| Input | Immutable State Carrier projection, source hashes, units, seed, time/frame selection, and selected entities |
| State | Parallel-model state is separate from `SimState` and is versioned independently |
| Encoder | Exact map from the frozen projection to the Q#-comparable representation |
| Decoder | Exact map from each model's output to named shared observables |
| Limits | What the projection omits, approximates, discretizes, or cannot compare |
| Evidence | Runner versions, commands, artifacts, diagnostics, timing, and output hashes |

A model that cannot name its encoder and decoder has no valid Q# comparison.
It is a concept sketch, not a benchmarkable system.

## Automatic Same-System Benchmark Protocol

“Same system” means the two runners receive the same frozen benchmark bundle,
not merely the same molecule label or formula. Both must prove the same bundle
hash before their results can be compared.

```text
BenchmarkCase_v0
  -> system_snapshot.json      (immutable State Carrier projection)
  -> benchmark_manifest.json   (case ID, entities, units, seed, frame/time)
  -> projection.json           (encoder/decoder version and declared limits)
  -> parallel-model runner     -> parallel_result.json
  -> isolated Q# runner        -> qsharp_result.json
  -> normalizer + comparator   -> comparison.json
  -> append-only ledger        -> benchmark_report.md
```

### Required Benchmark Manifest

```json
{
  "schema_version": "vsim_parallel_benchmark_v0",
  "benchmark_case_id": "named-case",
  "system_identity": "stable-system-or-molecule-id",
  "input_bundle_hash": "sha256:...",
  "projection_version": "parallel_projection_v0",
  "parallel_model_revision": "git-or-policy-revision",
  "qsharp_program_hash": "sha256:...",
  "qsharp_qdk_version": "declared-version",
  "seed": 0,
  "frame_or_time": "declared-frame-or-time",
  "observables": ["declared_shared_observable"],
  "units": {"declared_shared_observable": "declared-unit"},
  "tolerances": {"declared_shared_observable": 0.0}
}
```

The manifest is invalid when the system identity, bundle hash, projection
version, observable list, units, or tolerance profile is absent. A molecule
formula alone is not a sufficient `system_identity`; the case must distinguish
the canonical molecule/system, state, frame, and projection.

### Automatic Runner Rules

1. The coordinator writes one immutable input bundle before either runner
   starts.
2. The parallel model and Q# runner receive paths to that bundle and may only
   write inside separate result directories.
3. Each runner emits its own status, diagnostics, version, command, elapsed
   time, output hash, and result payload.
4. The comparator refuses to score a pair with different bundle hashes,
   projection versions, observable names, units, or entity/frame identity.
5. A missing Q# executable, an unavailable QDK, or an unsupported projection
   produces `blocked` or `unsupported`, never a synthetic passing score.
6. The coordinator appends a ledger record for every attempt, including
   failures and skips.
7. The benchmark report may create a review task or a new fixture proposal,
   but it may not tune kernel constants, change calibration, promote a
   MolecularDB property, or rewrite either runner's source.

### Accuracy And Agreement

For a shared observable `o`, the comparator first records the raw values,
units, and declared tolerance. It then computes normalized benchmark error:

```text
error_o = abs(parallel_o - qsharp_o) / max(tolerance_o, uncertainty_o, epsilon)
agreement_o = 1 / (1 + error_o)
```

The case report carries the vector of per-observable errors and agreement
scores, not only one flattering aggregate. An aggregate may be used for queue
ordering only when every required observable is present and unit-compatible.

This is **benchmark agreement**, not a blanket accuracy claim about chemistry
or physics. A Q# result is a valid comparison reference only for the declared
proxy and observable. Empirical data and established physics remain separate
validation mirrors.

### Triggering And Replay

The benchmark coordinator is eligible to rerun automatically when a declared
benchmark input changes:

- parallel-model revision;
- Q# program or QDK version;
- projection encoder/decoder version;
- benchmark fixture or immutable input-bundle hash; or
- observable/tolerance contract.

It does not rerun merely because a visual representation, report theme, or
MolecularDB display record changed. Every rerun receives a new attempt ID and
preserves earlier result artifacts, so agreement regressions remain visible.

## First Deferred Proof Slice

The first slice must be intentionally small and mathematical. It is not a
claim to model an entire molecule in Q#.

1. Select one canonical system snapshot and one frame.
2. Define one projection with a written loss/limit statement.
3. Define one to three shared observables with units and tolerances.
4. Run the independent parallel model and isolated Q# proxy from the exact
   same bundle hash.
5. Emit comparison, diagnostics, and a replay command without touching
   `SimState` or MolecularDB best-known properties.
6. Review whether disagreement teaches something about the projection/model;
   otherwise close the case as non-informative.

The slice is rejected if it silently substitutes a Q# simulator result for a
physical measurement, hides an unsupported feature, or calls agreement an
empirical validation.

## Priority And Activation Gate

This work order is **low priority**. It creates no scheduled implementation,
parser, dependency, or benchmark run.

WO-90A may advance only when all of the following are written and reviewed:

1. A parallel model is named, including its independent state and ownership
   boundary.
2. A narrow comparison question identifies an output that Q# can benchmark.
3. The comparison does not duplicate an existing classical-kernel validator.
4. Inputs, outputs, units, uncertainty, and provenance are defined as an
   immutable adapter contract.
5. The outcome is useful whether Q# agrees, disagrees, or is unavailable.

Until then, Q# remains a researched reference point rather than an active
project dependency.

## Follow-On Gates

| Letter | Work item | Required evidence | Status |
|---|---|---|---|
| B | Freeze `BenchmarkCase_v0` schema | Immutable bundle, manifest, projection, and paired-result contracts | Deferred; only after the activation gate passes |
| C | Name a parallel-model Q# benchmark question | One proxy, explicit encoder/decoder, shared observables, limits, and no duplicate classical validator | Deferred |
| D | Implement isolated paired runners | Parallel model and Q# executable consume the same bundle and write separate artifacts | Deferred |
| E | Implement automatic comparator and replay ledger | Hash, unit, identity, tolerance, agreement, blocked, and unsupported states are machine-checkable | Deferred |
| F | Review licensing, attribution, naming, and Q+ ownership | External-tool record is complete; visual schema rejects an ambiguous Q+ language/runtime declaration | Deferred |
| G | Review benchmark learning value | Report distinguishes proxy agreement, disagreement, unsupported scope, and empirical validation | Deferred |
| H | Close or promote | Decision records external-only, deferred, non-informative, or accepted benchmark use | Deferred pending activation evidence |

## Non-Goals

- Do not implement a quantum simulation kernel in VSIM.
- Do not import, translate, or mimic Q# syntax as Chem+ syntax.
- Do not embed K or depend on undocumented K behavior.
- Do not reinterpret the existing Q+ visual layer as a language.
- Do not let Q#, q/KDB-X, or any external tool mutate simulation truth or
  promoted MolecularDB records.
- Do not use an external-language result to justify Pd-H fitting.

## Evidence Sources

- Microsoft describes [Q# as a high-level, open-source language for quantum
  programs](https://learn.microsoft.com/en-us/azure/quantum/qsharp-overview)
  and documents its QDK execution paths.
- The [modern Microsoft QDK repository](https://github.com/microsoft/qsharp)
  contains the Q# compiler, language service, standard library, and resource
  estimator under the MIT license; its trademark terms still apply.
- KX documents that [q is implemented in K, while K itself is undocumented,
  version-varying, and for KX system programmers](https://code.kx.com/kdb-x/reference/exposed-infrastructure.html).
- Historical Q+ visual-routing notes are preserved in the
  [`legacy viewer archive`](../../archive/viewers/legacy-2026-07-22/docs/visual_stack/07_scene_render_payload_v0.md).
  The active visual authority is
  [`12_live_simulation_density_domains_v0.md`](../visual_stack/12_live_simulation_density_domains_v0.md).
