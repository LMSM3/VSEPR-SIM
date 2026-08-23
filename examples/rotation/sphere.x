# =============================================================================
# sphere.x
#
# Rotation demo — noble-gas bead cluster arranged in a spherical shell.
#
# Scene:  ~50 Ar beads placed on a sphere surface  (premacro_sphere_shell).
#         The cluster reads as a sphere at glance.  No bonding, no dynamics.
# Visual: gl_live_60fps | gl_spin | X-axis | 25 deg/s (slow roll)
#
# Run:
#   xsim scene examples/rotation/sphere.x
#
# Output:
#   out/sphere_rotation/trajectory.xyz
#   out/sphere_rotation/manifest.json
#
# stable+0.0.1
# =============================================================================

[package]
name    = "sphere_rotation"
kind    = "xsim_scene"
version = "stable+0.0.1"

# ----------------------------------------------------------------------------
# Scene geometry
# ----------------------------------------------------------------------------
[scene]
formula     = "Ar"
prototype   = "noble_gas"
phase       = "gas"
lattice     = "none"
count       = 50            # ~50 noble-gas beads  (runtime places on sphere shell)
temperature = 0.0

[scene.geometry]
type   = "sphere_shell"     # bead placement: surface of a sphere
radius = 4.0                # Angstrom

# ----------------------------------------------------------------------------
# Run control
# ----------------------------------------------------------------------------
[run]
mode      = "md"
max_steps = 1
dt_fs     = 1.0
converge  = false

# ----------------------------------------------------------------------------
# Visualizer
# ----------------------------------------------------------------------------
[visual]
output_type       = "gl_live_60fps"
gl_spin           = true
gl_spin_axis      = "x"
gl_spin_deg_per_s = 25.0
gl_show_axes      = true
gl_window_width   = 960
gl_window_height  = 720

# ----------------------------------------------------------------------------
# Export
# ----------------------------------------------------------------------------
[export]
write_xyz           = true
write_manifest_json = true
output_dir          = "out/sphere_rotation"
