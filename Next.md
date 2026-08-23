VSIM 5.16 architectural cut
Old architecture
atomistic::State
        ↕
SceneDocument
        ↕
EngineAdapter
        ↕
Qt desktop workstation
        +
separate ImGui viewer
Revised 5.16 architecture
.vsim source
    ↓
ResolvedRunPlan
    ↓
RuntimeSession
    ├── RuntimeState             scientific authority
    ├── CommandQueue             accepted mutations
    ├── ObserverHub              sampled observations
    │     ├── IntegratedLiveView
    │     ├── TerminalDashboard
    │     ├── AnalysisPipeline
    │     ├── TrajectoryWriter
    │     └── ReportWriter
    └── EventLedger

The dedicated visualizer disappears as an application.

The rendering code survives as an integrated observer backend invoked by:

[export.live]

No separate project loading, no separate desktop document, no second copy of the simulation pretending to be the first.

What survives from the old document

Keep:

atomistic::State as the mutable scientific state.
Structure-of-arrays storage for positions, velocities, forces, masses, and charges.
Kernel opacity.
The energy ledger.
Minimum-image PBC calculations.
Provenance.
Immutable sampled frames.
Renderer ignorance of force models.
File readers and writers.
The principle that visualization cannot mutate kernel memory directly.

Retire or demote:

The dedicated Qt workstation.
The standalone ImGui viewer as a user-facing executable.
SceneDocument as the sole inter-layer currency.
EngineAdapter as the global choke point.
KernelOp as the authoritative operation registry.
Round-tripping through XYZMolecule for every operation.
Copying full trajectory history merely to append one frame.
Per-atom std::string symbol in live frames.
std::map<std::string,double> in the hot live-update path.
std::future<KernelResult> as the live-runtime abstraction.

SceneDocument can remain as:

legacy import format
trajectory artifact
replay document
compatibility-test fixture

It simply stops being the bloodstream of the running engine.

The frame math

The old State contains, per atom:

position       3 × 8 = 24 bytes
velocity       3 × 8 = 24
temperature        8
charge             8
mass               8
species ID         4
force          3 × 8 = 24
                         ───
approximately     100 bytes/atom

This excludes vector metadata, capacity, bonds, events, and allocator overhead.

For 512 Argon atoms:

M
state
	​

≈512(100)=51,200 bytes=50 KiB

The old FrameData is heavier. A plausible 64-bit ABI estimate is:

AtomRecord
    int Z                 4 bytes
    alignment             4
    Vec3d position       24
    std::string symbol   32
                        ───
                         64 bytes

velocity                 24
force                    24
charge                    8
                        ───
approximately            120 bytes/atom

Thus:

M
frame
	​

≈512(120)=61,440 bytes=60 KiB

That is perfectly harmless for one frame. The problem appears when the live system treats complete documents as continually copied currency.

Frame count

For K simulation steps and sample stride s:

F=⌊
s
K
	​

⌋+1

For the Argon run with 2,000 steps:

Sample stride	Frames	Raw frame storage	Playback at 30 FPS
100 steps	21	1.23 MiB	0.70 s
10 steps	201	11.78 MiB	6.70 s
5 steps	401	23.50 MiB	13.37 s
1 step	2,001	117.25 MiB	66.70 s

The present render_interval = 100 gives only 21 frames including the initial state. That is a preview, not an animation. The atoms barely get a chance to appear before the credits roll.

For a roughly ten-second 30 FPS demonstration, the desired frame count is approximately:

F
d
	​

=30(10)=300

A suitable automatic stride is:

s=⌈
F
d
	​

−1
K
	​

⌉=⌈
299
2000
	​

⌉=7

That produces:

F=⌊
7
2000
	​

⌋+1=286

or about:

286/30=9.53 seconds

So [export.live] can automatically select a visual sampling stride from the step count, requested FPS, and desired playback duration.

Why copying SceneDocument must end

The old document says the resulting frame is appended to a copy of the input document.

If that behavior were extended naïvely to every live frame, cumulative copied data would scale as:

M
copied
	​

=M
frame
	​

k=1
∑
F
	​

k=M
frame
	​

2
F(F+1)
	​


For the Argon case:

Frames	Resident trajectory	Cumulative copy traffic
21	1.23 MiB	13.54 MiB
201	11.78 MiB	1.16 GiB
401	23.50 MiB	4.61 GiB
2,001	117.25 MiB	114.61 GiB

The final trajectory might occupy only 117 MiB, yet the program could move more than 114 GiB while repeatedly rebuilding it.

That is the mathematical reason SceneDocument cannot remain the live transport type.

Specialized integrated render frames

The live renderer does not need masses, forces, velocities, strings, provenance maps, or double-precision coordinates in every visual frame.

Use a compact packet:

struct RenderAtom {
    std::uint64_t id;       // 8 bytes
    float x, y, z;          // 12 bytes
    std::uint16_t species;  // 2 bytes
    std::uint16_t flags;    // 2 bytes
};                          // 24 bytes total

For 512 atoms:

M
render
	​

=512(24)=12,288 bytes=12 KiB

At 30 FPS:

12 KiB×30=360 KiB/s

Compare that with complete FrameData packets:

60 KiB×30=1.76 MiB/s

That is a fivefold reduction before compression, thresholding, or GPU persistence.

With a three-frame latest queue:

3(12 KiB)=36 KiB

The integrated renderer therefore needs bounded kilobytes, not an ever-growing in-memory trajectory.

Scientific trajectory frames remain double precision and stream directly to disk. Render frames are disposable visual products. Apparently an atom can be drawn without attaching its entire autobiography.

Gas-kernel math

The 512-particle Argon case also exposes why 5.16 needs a proper neighbor list.

Without spatial filtering, the number of unique pairs per step is:

P
all
	​

=
2
N(N−1)
	​

=
2
512(511)
	​

=130,816

Across 2,000 steps:

130,816(2000)=261,632,000

That is more than 261 million pair checks.

The box volume is:

V=60
3
=216,000
A
˚
3

Number density:

ρ=
216,000
512
	​

=2.37037×10
−3
A
˚
−3

For a uniform distribution, the expected neighbor count within cutoff r
c
	​

 is approximately:

n
neighbor
	​

=ρ
3
4πr
c
3
	​

	​


Using the old document’s 10 Å cutoff:

n
neighbor
	​

≈9.93

Expected unique candidate pairs:

P
cutoff
	​

≈
2
512(9.93)
	​

≈2,542

Reduction:

2,542
130,816
	​

≈51.5

Using an 8.5 Å Argon cutoff:

n
neighbor
	​

≈6.10
P
cutoff
	​

≈1,561
1,561
130,816
	​

≈83.8

Actual counts will differ because of the Verlet skin and radial correlations, but the result is decisive: the neighbor list matters vastly more than micro-optimizing the cartoon renderer.

Integrated replacement for the current visual syntax

The existing graphite demo uses:

[visual.workspace]
enabled        = true
default_layout = "detached"
live           = true

show "scene.cg_bead" target = "run.cg_state"
show "overlay.cycle" target = "run.history"
show "data.events.timeline" target = "kernel.events"
show "data.events.bar_chart" target = "kernel.events"
show "live.runtime" target = "run.history"

That detached workspace grammar should translate in 5.16 to:

[export.live]

The empty header enables every compiled-in live capability:

[export.live]
enabled          = true
scene            = true
metrics          = true
events           = true
terminal         = true
controls         = true
recording        = true
integrated       = true
target_fps       = 30
queue_policy     = "latest"
queue_depth      = 3
sampling         = "automatic"

The explicit form remains available:

[export.live]
terminal  = false
controls  = false
recording = true

[export.live.scene]
style         = "cg_bead"
auto_orbit    = true
show_cell     = true
show_bonds    = true
show_trails   = true
trail_length  = 30

[export.live.metrics]
enabled = true
show    = [
    "energy.total",
    "rms_force",
    "temperature",
    "pressure"
]

[export.live.events]
timeline  = true
bar_chart = true

[export.live.overlay]
sequence = [
    "density",
    "coordination",
    "memory",
    "orient_order"
]
hold = 2.5 second

No default_layout = "detached" because there is no dedicated workstation to detach from.

Integrated live data contracts
struct RenderFrame {
    std::uint64_t sequence;
    std::uint64_t step;
    double simulation_time_fs;

    CellRenderData cell;
    std::span<const RenderAtom> atoms;
    std::span<const RenderBond> bonds;
};

struct MetricFrame {
    std::uint64_t sequence;
    std::uint64_t step;

    double temperature_K;
    double pressure_Pa;
    double kinetic_energy;
    double potential_energy;
    double total_energy;
    double force_rms;
};

struct RuntimeEvent {
    EventId id;
    EventType type;
    std::uint64_t step;
    ObjectId subject;
    EventPayload payload;
};

These are separate because they have different rates:

RenderFrame     30 wall-clock frames/s, latest wins
MetricFrame     perhaps every 10 simulation steps
RuntimeEvent    every accepted event, never silently dropped
Trajectory      scientific sampling interval, ordered
Checkpoint      sparse and durable

A single bloated FrameData cannot serve all five purposes well.

New authority rules
Runtime authority

Only RuntimeSession may mutate RuntimeState.

Visual authority

The integrated renderer owns:

camera;
selected object;
colors;
trails;
overlays;
window state.

It does not own atom existence or position.

Command authority

Interactive physical changes travel through:

IntegratedLiveView
    → RuntimeCommand
    → CommandQueue
    → safe simulation boundary
    → validation
    → accepted event
    → RuntimeState mutation
Persistence authority

Trajectory and report writers consume observations. They never become the live state.

5.16 implementation order
Extract sphere, bond, CPK, camera, and overlay rendering from the dedicated visualizers.
Create RuntimeSession.
Create a bounded ObserverHub.
Add RenderFrame, MetricFrame, and RuntimeEvent.
Connect the renderer directly to vsim run.
Add [export.live] profile expansion.
Translate legacy [visual.workspace] declarations.
Stream trajectories to disk instead of accumulating copied documents.
Retain SceneDocument only for import, replay, and compatibility.
Remove dedicated visualizer binaries after feature parity.
5.16 acceptance gates
[export.live] opens the integrated viewer from the normal run command.
Omitting [export.live] remains completely headless.
No SceneDocument → XYZMolecule → State conversion occurs during integration.
Live rendering can drop stale frames without changing scientific output.
Event records are never dropped.
The renderer uses stable particle IDs rather than vector indices.
Particle deletion becomes visible within one rendered frame.
Live-on and live-off runs produce identical scientific hashes.
Render memory is bounded by queue depth.
Trajectory memory does not grow unless explicitly requested.
The old [visual.workspace] syntax remains readable through a compatibility translator.
No standalone visualizer is installed by default.