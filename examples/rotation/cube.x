# =============================================================================
# cube.x
#
# Rotation demo — solid body-centred cubic iron cube, spinning around Y.
#
# Scene:  Fe BCC  3x3x3 supercell (54 atoms) — visually a solid metal cube.
# Visual: gl_live_60fps | gl_spin | Y-axis | 40 deg/s
#
# Run:
#   xsim scene examples/rotation/cube.x
#
# Output:
#   out/cube_rotation/trajectory.xyz
#   out/cube_rotation/manifest.json
#
# stable+0.0.1
# =============================================================================

[package]
name    = "cube_rotation"
kind    = "xsim_scene"
version = "stable+0.0.1"

# ----------------------------------------------------------------------------
# Scene geometry
# ----------------------------------------------------------------------------
[scene]
formula     = "Fe"
prototype   = "A2_bcc"
phase       = "solid"
lattice     = "bcc"
count       = 54            # 3x3x3 BCC supercell: cube block of iron atoms
temperature = 0.0           # frozen scene — no dynamics

# ----------------------------------------------------------------------------
# Run control  (1 step — physics frozen, viewer side only)
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
gl_spin_axis      = "y"
gl_spin_deg_per_s = 40.0
gl_show_axes      = true
gl_window_width   = 960
gl_window_height  = 720

# ----------------------------------------------------------------------------
# Export
# ----------------------------------------------------------------------------
[export]
write_xyz          = true
write_manifest_json = true
output_dir         = "out/cube_rotation"
