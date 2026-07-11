#!/usr/bin/env python3
"""
visualize_bead_fire_trajectory.py — BeadFIRE Trajectory Visualization

Generates publication-quality plots showing:
  - XY and XZ plane projections of bead trajectories
  - Energy convergence plots (per-channel decomposition)
  - Force evolution (RMS and max)
  - FIRE algorithm parameter evolution (α, Δt)

Usage:
    python visualize_bead_fire_trajectory.py <history_file.csv> [output_dir]

Input Format (CSV):
    step,U_total,U_steric,U_elec,U_disp,Frms,Fmax,alpha,dt,dU_per_bead,
    bead0_x,bead0_y,bead0_z,bead1_x,bead1_y,bead1_z,...

Output:
    - trajectory_xy.png
    - trajectory_xz.png
    - energy_convergence.png
    - force_evolution.png
    - fire_parameters.png
    - combined_report.png (all panels)

Anti-black-box: All data is explicitly plotted. Every bead, every step visible.

Requirements:
    numpy, matplotlib
"""

import sys
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from matplotlib.patches import Circle
import csv


# ============================================================================
# Data Loading
# ============================================================================

def load_fire_history(filepath):
    """
    Load BeadFIRE history from CSV file.

    Returns:
        dict with keys:
            - steps: array of step numbers
            - U_total, U_steric, U_elec, U_disp: energy arrays
            - Frms, Fmax: force arrays
            - alpha, dt: FIRE parameter arrays
            - dU_per_bead: energy change array
            - positions: list of (N_beads, 3) arrays for each step
            - n_beads: number of beads
    """
    data = {
        'steps': [],
        'U_total': [],
        'U_steric': [],
        'U_elec': [],
        'U_disp': [],
        'Frms': [],
        'Fmax': [],
        'alpha': [],
        'dt': [],
        'dU_per_bead': [],
        'positions': []
    }

    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        header = next(reader)

        # Determine number of beads from header
        # Format: step,U_total,...,bead0_x,bead0_y,bead0_z,bead1_x,...
        fixed_cols = 9  # step through dU_per_bead
        n_coord_cols = len(header) - fixed_cols
        n_beads = n_coord_cols // 3

        data['n_beads'] = n_beads

        for row in reader:
            data['steps'].append(int(row[0]))
            data['U_total'].append(float(row[1]))
            data['U_steric'].append(float(row[2]))
            data['U_elec'].append(float(row[3]))
            data['U_disp'].append(float(row[4]))
            data['Frms'].append(float(row[5]))
            data['Fmax'].append(float(row[6]))
            data['alpha'].append(float(row[7]))
            data['dt'].append(float(row[8]))
            data['dU_per_bead'].append(float(row[9]))

            # Extract bead positions
            positions = np.zeros((n_beads, 3))
            for i in range(n_beads):
                positions[i, 0] = float(row[fixed_cols + i*3 + 0])  # x
                positions[i, 1] = float(row[fixed_cols + i*3 + 1])  # y
                positions[i, 2] = float(row[fixed_cols + i*3 + 2])  # z
            data['positions'].append(positions)

    # Convert to numpy arrays
    for key in ['steps', 'U_total', 'U_steric', 'U_elec', 'U_disp', 
                'Frms', 'Fmax', 'alpha', 'dt', 'dU_per_bead']:
        data[key] = np.array(data[key])

    return data


def generate_demo_data(n_beads=6, n_steps=200):
    """
    Generate synthetic BeadFIRE trajectory data for demonstration.

    Simulates a simple minimization:
      - Beads start in a line
      - Relax toward equilibrium configuration
      - Energy decreases
      - Forces decay
    """
    data = {
        'steps': np.arange(n_steps),
        'n_beads': n_beads,
        'positions': []
    }

    # Initial configuration: beads in a line along x-axis
    initial_pos = np.zeros((n_beads, 3))
    for i in range(n_beads):
        initial_pos[i, 0] = i * 2.5  # 2.5 Å spacing

    # Target configuration: hexagonal arrangement
    target_pos = np.zeros((n_beads, 3))
    for i in range(n_beads):
        angle = 2 * np.pi * i / n_beads
        target_pos[i, 0] = 5.0 * np.cos(angle)
        target_pos[i, 1] = 5.0 * np.sin(angle)
        target_pos[i, 2] = 0.5 * np.sin(3 * angle)  # Small z-variation

    # Interpolate with exponential relaxation
    for step in range(n_steps):
        t = 1.0 - np.exp(-step / 30.0)  # Relaxation timescale
        pos = initial_pos * (1 - t) + target_pos * t
        # Add thermal noise that decreases over time
        noise_scale = 0.5 * np.exp(-step / 20.0)
        pos += np.random.randn(*pos.shape) * noise_scale
        data['positions'].append(pos)

    # Energy trajectory (decreasing with fluctuations)
    U0 = -50.0
    Uf = -200.0
    data['U_total'] = Uf + (U0 - Uf) * np.exp(-data['steps'] / 30.0)
    data['U_total'] += np.random.randn(n_steps) * 2.0  # Noise

    # Per-channel decomposition
    data['U_steric'] = 0.40 * data['U_total'] + np.random.randn(n_steps) * 0.5
    data['U_elec'] = 0.50 * data['U_total'] + np.random.randn(n_steps) * 0.5
    data['U_disp'] = 0.10 * data['U_total'] + np.random.randn(n_steps) * 0.5

    # Force trajectory (decaying)
    F0 = 2.0
    Ff = 0.0001
    data['Frms'] = Ff + (F0 - Ff) * np.exp(-data['steps'] / 25.0)
    data['Fmax'] = data['Frms'] * 2.5

    # FIRE parameters
    data['alpha'] = 0.1 * np.exp(-data['steps'] / 40.0) + 0.01
    data['dt'] = 1.0 + (10.0 - 1.0) * (1.0 - np.exp(-data['steps'] / 50.0))
    data['dU_per_bead'] = np.abs(np.diff(data['U_total'], prepend=U0)) / n_beads

    return data


# ============================================================================
# Visualization Functions
# ============================================================================

def plot_trajectory_xy(data, output_path):
    """
    XY plane projection of bead trajectories.
    Shows initial positions, final positions, and trajectory paths.
    """
    fig, ax = plt.subplots(figsize=(10, 10))

    n_beads = data['n_beads']
    n_steps = len(data['steps'])

    # Color map for beads
    colors = plt.cm.tab10(np.linspace(0, 1, n_beads))

    # Plot trajectory paths
    for bead_idx in range(n_beads):
        x_traj = [data['positions'][step][bead_idx, 0] for step in range(n_steps)]
        y_traj = [data['positions'][step][bead_idx, 1] for step in range(n_steps)]

        # Path with fading alpha
        ax.plot(x_traj, y_traj, '-', color=colors[bead_idx], alpha=0.3, 
                linewidth=1, label=f'Bead {bead_idx}' if bead_idx < 3 else '')

    # Plot initial positions (open circles)
    for bead_idx in range(n_beads):
        pos = data['positions'][0][bead_idx]
        ax.plot(pos[0], pos[1], 'o', markersize=12, color=colors[bead_idx],
                fillstyle='none', markeredgewidth=2)

    # Plot final positions (filled circles)
    for bead_idx in range(n_beads):
        pos = data['positions'][-1][bead_idx]
        ax.plot(pos[0], pos[1], 'o', markersize=12, color=colors[bead_idx])

    # Annotations
    ax.text(0.02, 0.98, 'Initial positions: ○', transform=ax.transAxes,
            fontsize=10, va='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    ax.text(0.02, 0.93, 'Final positions: ●', transform=ax.transAxes,
            fontsize=10, va='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    ax.set_xlabel('X (Å)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Y (Å)', fontsize=14, fontweight='bold')
    ax.set_title('Bead Trajectories: XY Plane Projection', fontsize=16, fontweight='bold')
    ax.grid(True, alpha=0.3)
    ax.set_aspect('equal')

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {output_path}")


def plot_trajectory_xz(data, output_path):
    """
    XZ plane projection of bead trajectories.
    """
    fig, ax = plt.subplots(figsize=(12, 6))

    n_beads = data['n_beads']
    n_steps = len(data['steps'])

    colors = plt.cm.tab10(np.linspace(0, 1, n_beads))

    # Plot trajectory paths
    for bead_idx in range(n_beads):
        x_traj = [data['positions'][step][bead_idx, 0] for step in range(n_steps)]
        z_traj = [data['positions'][step][bead_idx, 2] for step in range(n_steps)]

        ax.plot(x_traj, z_traj, '-', color=colors[bead_idx], alpha=0.3, linewidth=1)

    # Plot initial and final positions
    for bead_idx in range(n_beads):
        pos_init = data['positions'][0][bead_idx]
        pos_final = data['positions'][-1][bead_idx]

        ax.plot(pos_init[0], pos_init[2], 'o', markersize=12, color=colors[bead_idx],
                fillstyle='none', markeredgewidth=2)
        ax.plot(pos_final[0], pos_final[2], 'o', markersize=12, color=colors[bead_idx])

    ax.set_xlabel('X (Å)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Z (Å)', fontsize=14, fontweight='bold')
    ax.set_title('Bead Trajectories: XZ Plane Projection', fontsize=16, fontweight='bold')
    ax.grid(True, alpha=0.3)
    ax.set_aspect('equal')

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {output_path}")


def plot_energy_convergence(data, output_path):
    """
    Energy convergence with per-channel decomposition.
    """
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))

    steps = data['steps']

    # Panel 1: Total energy and per-channel
    ax1.plot(steps, data['U_total'], 'k-', linewidth=2, label='Total')
    ax1.plot(steps, data['U_steric'], '--', color='red', linewidth=1.5, label='Steric')
    ax1.plot(steps, data['U_elec'], '--', color='blue', linewidth=1.5, label='Electrostatic')
    ax1.plot(steps, data['U_disp'], '--', color='green', linewidth=1.5, label='Dispersion')

    ax1.set_xlabel('Step', fontsize=12)
    ax1.set_ylabel('Energy (kcal/mol)', fontsize=12)
    ax1.set_title('Energy Convergence: Per-Channel Decomposition', fontsize=14, fontweight='bold')
    ax1.legend(loc='best', fontsize=10)
    ax1.grid(True, alpha=0.3)

    # Panel 2: Energy change per bead (log scale)
    ax2.semilogy(steps, np.abs(data['dU_per_bead']) + 1e-12, 'b-', linewidth=1.5)
    ax2.axhline(1e-8, color='r', linestyle='--', linewidth=2, label='Convergence threshold (εᵤ)')

    ax2.set_xlabel('Step', fontsize=12)
    ax2.set_ylabel('|ΔU/N| (kcal/mol)', fontsize=12)
    ax2.set_title('Per-Bead Energy Change (Log Scale)', fontsize=14, fontweight='bold')
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(True, alpha=0.3, which='both')

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {output_path}")


def plot_force_evolution(data, output_path):
    """
    Force evolution: RMS and max forces.
    """
    fig, ax = plt.subplots(figsize=(12, 6))

    steps = data['steps']

    ax.semilogy(steps, data['Frms'], 'b-', linewidth=2, label='F_RMS')
    ax.semilogy(steps, data['Fmax'], 'r--', linewidth=2, label='F_max')
    ax.axhline(1e-4, color='g', linestyle='--', linewidth=2, label='Convergence threshold (εF)')

    ax.set_xlabel('Step', fontsize=14, fontweight='bold')
    ax.set_ylabel('Force (kcal/mol·Å)', fontsize=14, fontweight='bold')
    ax.set_title('Force Evolution', fontsize=16, fontweight='bold')
    ax.legend(loc='best', fontsize=12)
    ax.grid(True, alpha=0.3, which='both')

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {output_path}")


def plot_fire_parameters(data, output_path):
    """
    FIRE algorithm parameter evolution (α and Δt).
    """
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))

    steps = data['steps']

    # Panel 1: α (velocity mixing parameter)
    ax1.plot(steps, data['alpha'], 'b-', linewidth=2)
    ax1.set_ylabel('α (velocity mixing)', fontsize=12, fontweight='bold')
    ax1.set_title('FIRE Algorithm Parameters', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)

    # Panel 2: Δt (timestep)
    ax2.plot(steps, data['dt'], 'r-', linewidth=2)
    ax2.set_xlabel('Step', fontsize=12, fontweight='bold')
    ax2.set_ylabel('Δt (fs)', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {output_path}")


def plot_combined_report(data, output_path):
    """
    Combined multi-panel report with all key metrics.
    Similar to fig6_lattice_initial.png style.
    """
    fig = plt.figure(figsize=(20, 12))
    gs = GridSpec(3, 3, figure=fig, hspace=0.3, wspace=0.3)

    n_beads = data['n_beads']
    n_steps = len(data['steps'])
    steps = data['steps']
    colors = plt.cm.tab10(np.linspace(0, 1, n_beads))

    # ========================================================================
    # Top-left: XY trajectory
    # ========================================================================
    ax_xy = fig.add_subplot(gs[0, 0])
    for bead_idx in range(n_beads):
        x_traj = [data['positions'][step][bead_idx, 0] for step in range(n_steps)]
        y_traj = [data['positions'][step][bead_idx, 1] for step in range(n_steps)]
        ax_xy.plot(x_traj, y_traj, '-', color=colors[bead_idx], alpha=0.3, linewidth=1)

        pos_init = data['positions'][0][bead_idx]
        pos_final = data['positions'][-1][bead_idx]
        ax_xy.plot(pos_init[0], pos_init[1], 'o', markersize=8, color=colors[bead_idx],
                   fillstyle='none', markeredgewidth=2)
        ax_xy.plot(pos_final[0], pos_final[1], 'o', markersize=8, color=colors[bead_idx])

    ax_xy.set_xlabel('X (Å)', fontsize=10, fontweight='bold')
    ax_xy.set_ylabel('Y (Å)', fontsize=10, fontweight='bold')
    ax_xy.set_title('XY Plane', fontsize=12, fontweight='bold')
    ax_xy.grid(True, alpha=0.3)
    ax_xy.set_aspect('equal')

    # ========================================================================
    # Top-center: XZ trajectory
    # ========================================================================
    ax_xz = fig.add_subplot(gs[0, 1])
    for bead_idx in range(n_beads):
        x_traj = [data['positions'][step][bead_idx, 0] for step in range(n_steps)]
        z_traj = [data['positions'][step][bead_idx, 2] for step in range(n_steps)]
        ax_xz.plot(x_traj, z_traj, '-', color=colors[bead_idx], alpha=0.3, linewidth=1)

        pos_init = data['positions'][0][bead_idx]
        pos_final = data['positions'][-1][bead_idx]
        ax_xz.plot(pos_init[0], pos_init[2], 'o', markersize=8, color=colors[bead_idx],
                   fillstyle='none', markeredgewidth=2)
        ax_xz.plot(pos_final[0], pos_final[2], 'o', markersize=8, color=colors[bead_idx])

    ax_xz.set_xlabel('X (Å)', fontsize=10, fontweight='bold')
    ax_xz.set_ylabel('Z (Å)', fontsize=10, fontweight='bold')
    ax_xz.set_title('XZ Plane', fontsize=12, fontweight='bold')
    ax_xz.grid(True, alpha=0.3)
    ax_xz.set_aspect('equal')

    # ========================================================================
    # Top-right: 3D visualization (isometric)
    # ========================================================================
    ax_3d = fig.add_subplot(gs[0, 2], projection='3d')
    for bead_idx in range(n_beads):
        x_traj = [data['positions'][step][bead_idx, 0] for step in range(n_steps)]
        y_traj = [data['positions'][step][bead_idx, 1] for step in range(n_steps)]
        z_traj = [data['positions'][step][bead_idx, 2] for step in range(n_steps)]
        ax_3d.plot(x_traj, y_traj, z_traj, '-', color=colors[bead_idx], alpha=0.3, linewidth=1)

        pos_final = data['positions'][-1][bead_idx]
        ax_3d.scatter(pos_final[0], pos_final[1], pos_final[2], 
                     color=colors[bead_idx], s=100, edgecolors='k', linewidths=1)

    ax_3d.set_xlabel('X (Å)', fontsize=9)
    ax_3d.set_ylabel('Y (Å)', fontsize=9)
    ax_3d.set_zlabel('Z (Å)', fontsize=9)
    ax_3d.set_title('3D Trajectory', fontsize=12, fontweight='bold')

    # ========================================================================
    # Middle-left: Energy convergence
    # ========================================================================
    ax_energy = fig.add_subplot(gs[1, :])
    ax_energy.plot(steps, data['U_total'], 'k-', linewidth=2, label='Total')
    ax_energy.plot(steps, data['U_steric'], '--', color='red', linewidth=1.5, label='Steric')
    ax_energy.plot(steps, data['U_elec'], '--', color='blue', linewidth=1.5, label='Electrostatic')
    ax_energy.plot(steps, data['U_disp'], '--', color='green', linewidth=1.5, label='Dispersion')

    ax_energy.set_xlabel('Step', fontsize=12, fontweight='bold')
    ax_energy.set_ylabel('Energy (kcal/mol)', fontsize=12, fontweight='bold')
    ax_energy.set_title('Energy Convergence', fontsize=14, fontweight='bold')
    ax_energy.legend(loc='best', fontsize=10)
    ax_energy.grid(True, alpha=0.3)

    # ========================================================================
    # Bottom-left: Force evolution
    # ========================================================================
    ax_force = fig.add_subplot(gs[2, 0])
    ax_force.semilogy(steps, data['Frms'], 'b-', linewidth=2, label='F_RMS')
    ax_force.semilogy(steps, data['Fmax'], 'r--', linewidth=2, label='F_max')
    ax_force.axhline(1e-4, color='g', linestyle='--', linewidth=2, label='εF threshold')

    ax_force.set_xlabel('Step', fontsize=10, fontweight='bold')
    ax_force.set_ylabel('Force (kcal/mol·Å)', fontsize=10, fontweight='bold')
    ax_force.set_title('Force Evolution', fontsize=12, fontweight='bold')
    ax_force.legend(loc='best', fontsize=9)
    ax_force.grid(True, alpha=0.3, which='both')

    # ========================================================================
    # Bottom-center: α parameter
    # ========================================================================
    ax_alpha = fig.add_subplot(gs[2, 1])
    ax_alpha.plot(steps, data['alpha'], 'b-', linewidth=2)
    ax_alpha.set_xlabel('Step', fontsize=10, fontweight='bold')
    ax_alpha.set_ylabel('α', fontsize=10, fontweight='bold')
    ax_alpha.set_title('Velocity Mixing Parameter', fontsize=12, fontweight='bold')
    ax_alpha.grid(True, alpha=0.3)

    # ========================================================================
    # Bottom-right: Δt parameter
    # ========================================================================
    ax_dt = fig.add_subplot(gs[2, 2])
    ax_dt.plot(steps, data['dt'], 'r-', linewidth=2)
    ax_dt.set_xlabel('Step', fontsize=10, fontweight='bold')
    ax_dt.set_ylabel('Δt (fs)', fontsize=10, fontweight='bold')
    ax_dt.set_title('Timestep', fontsize=12, fontweight='bold')
    ax_dt.grid(True, alpha=0.3)

    # Overall title
    fig.suptitle('BeadFIRE Minimization Report: Complete Trajectory Analysis', 
                 fontsize=18, fontweight='bold', y=0.995)

    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {output_path}")


# ============================================================================
# CSV Export (for C++ integration)
# ============================================================================

def export_history_csv(data, output_path):
    """
    Export history data to CSV format for archival/analysis.

    Format matches the expected input format for this script.
    """
    n_beads = data['n_beads']
    n_steps = len(data['steps'])

    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)

        # Header
        header = ['step', 'U_total', 'U_steric', 'U_elec', 'U_disp',
                  'Frms', 'Fmax', 'alpha', 'dt', 'dU_per_bead']
        for i in range(n_beads):
            header.extend([f'bead{i}_x', f'bead{i}_y', f'bead{i}_z'])
        writer.writerow(header)

        # Data rows
        for step_idx in range(n_steps):
            row = [
                data['steps'][step_idx],
                data['U_total'][step_idx],
                data['U_steric'][step_idx],
                data['U_elec'][step_idx],
                data['U_disp'][step_idx],
                data['Frms'][step_idx],
                data['Fmax'][step_idx],
                data['alpha'][step_idx],
                data['dt'][step_idx],
                data['dU_per_bead'][step_idx]
            ]

            # Append bead positions
            positions = data['positions'][step_idx]
            for i in range(n_beads):
                row.extend([positions[i, 0], positions[i, 1], positions[i, 2]])

            writer.writerow(row)

    print(f"✓ Exported CSV: {output_path}")


# ============================================================================
# Main
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: python visualize_bead_fire_trajectory.py <history_file.csv> [output_dir]")
        print("\nGenerating demo data for demonstration...")
        data = generate_demo_data(n_beads=8, n_steps=200)
        output_dir = "bead_fire_plots"
    else:
        history_file = sys.argv[1]
        output_dir = sys.argv[2] if len(sys.argv) > 2 else "bead_fire_plots"

        if not os.path.exists(history_file):
            print(f"Error: File not found: {history_file}")
            sys.exit(1)

        print(f"Loading history from: {history_file}")
        data = load_fire_history(history_file)

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    print(f"\nOutput directory: {output_dir}")
    print(f"Beads: {data['n_beads']}, Steps: {len(data['steps'])}\n")

    # Generate all plots
    print("Generating visualizations...")
    plot_trajectory_xy(data, os.path.join(output_dir, "trajectory_xy.png"))
    plot_trajectory_xz(data, os.path.join(output_dir, "trajectory_xz.png"))
    plot_energy_convergence(data, os.path.join(output_dir, "energy_convergence.png"))
    plot_force_evolution(data, os.path.join(output_dir, "force_evolution.png"))
    plot_fire_parameters(data, os.path.join(output_dir, "fire_parameters.png"))
    plot_combined_report(data, os.path.join(output_dir, "combined_report.png"))

    # Export CSV if demo data was generated
    if len(sys.argv) < 2:
        export_history_csv(data, os.path.join(output_dir, "demo_history.csv"))

    print(f"\n✓ All visualizations complete!")
    print(f"View results in: {output_dir}/")


if __name__ == "__main__":
    main()
