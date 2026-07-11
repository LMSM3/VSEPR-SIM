#!/usr/bin/env python3
"""
gas2_monitor.py — Steady-State Tkinter Monitoring Window
=========================================================

Reads JSON snapshots from `vsepr gas2 monitor <FORMULA>` via stdin pipe
and displays:
  - 8-channel potential decomposition (bar chart, live updating)
  - Landau-Ginzburg free-energy functional F[phi] result
  - Density profile phi(x)
  - Temperature sweep indicator

Usage:
    vsepr gas2 monitor Ar -T 100 -N 200 | python tools/gas2_monitor.py

Or standalone with a file:
    python tools/gas2_monitor.py < snapshots.jsonl

Requires: Python 3.6+, tkinter (standard library)
"""

import sys
import json
import threading
import tkinter as tk
from tkinter import ttk
import math

# ============================================================================
# Constants
# ============================================================================

CHANNEL_NAMES = ["U_bond", "U_angle", "U_tors", "U_vdW",
                 "U_Coul", "U_pol", "U_many", "U_total"]

CHANNEL_COLORS = ["#4CAF50", "#2196F3", "#FF9800", "#F44336",
                  "#9C27B0", "#00BCD4", "#795548", "#FFFFFF"]

BG_COLOR = "#1e1e1e"
FG_COLOR = "#e0e0e0"
ACCENT   = "#bb86fc"
BAR_BG   = "#333333"


# ============================================================================
# Main Window
# ============================================================================

class Gas2Monitor:
    def __init__(self, root):
        self.root = root
        self.root.title("VSEPR-SIM gas2 Monitor")
        self.root.configure(bg=BG_COLOR)
        self.root.geometry("1100x700")
        self.root.minsize(900, 600)

        self.latest = None
        self.running = True

        self._build_ui()
        self._start_reader()
        self._poll()

    def _build_ui(self):
        # Title bar
        title_frame = tk.Frame(self.root, bg="#2d2d2d", height=40)
        title_frame.pack(fill=tk.X)
        title_frame.pack_propagate(False)
        tk.Label(title_frame, text="  gas2 Steady-State Monitor",
                 font=("Consolas", 14, "bold"),
                 fg=ACCENT, bg="#2d2d2d").pack(side=tk.LEFT, padx=10)
        self.lbl_status = tk.Label(title_frame, text="Waiting for data...",
                                    font=("Consolas", 10),
                                    fg="#888", bg="#2d2d2d")
        self.lbl_status.pack(side=tk.RIGHT, padx=10)

        # Main content: left = bars, right = F[phi]
        content = tk.Frame(self.root, bg=BG_COLOR)
        content.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # ---- Left panel: 8 U channels ----
        left = tk.LabelFrame(content, text=" 8-Channel Potential Decomposition ",
                              font=("Consolas", 11, "bold"),
                              fg=ACCENT, bg=BG_COLOR, labelanchor="n")
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))

        self.bar_canvas = tk.Canvas(left, bg=BG_COLOR, highlightthickness=0)
        self.bar_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # ---- Right panel: F[phi] + info ----
        right = tk.Frame(content, bg=BG_COLOR, width=400)
        right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=False, padx=(5, 0))
        right.pack_propagate(False)

        # Info panel
        info_frame = tk.LabelFrame(right, text=" State ",
                                    font=("Consolas", 10, "bold"),
                                    fg=ACCENT, bg=BG_COLOR)
        info_frame.pack(fill=tk.X, pady=(0, 5))

        self.info_labels = {}
        for key in ["Formula", "T (K)", "Cycle",
                     "F_bulk (J)", "F_grad (J)", "F_total (J)", "F_total (Eh)",
                     "a", "b", "κ", "φ_eq"]:
            row = tk.Frame(info_frame, bg=BG_COLOR)
            row.pack(fill=tk.X, padx=5, pady=1)
            tk.Label(row, text=key + ":", font=("Consolas", 9),
                     fg="#999", bg=BG_COLOR, width=14, anchor="w").pack(side=tk.LEFT)
            lbl = tk.Label(row, text="—", font=("Consolas", 9, "bold"),
                           fg=FG_COLOR, bg=BG_COLOR, anchor="w")
            lbl.pack(side=tk.LEFT, fill=tk.X, expand=True)
            self.info_labels[key] = lbl

        # Density profile canvas
        phi_frame = tk.LabelFrame(right, text=" Density Profile φ(x) ",
                                   font=("Consolas", 10, "bold"),
                                   fg=ACCENT, bg=BG_COLOR)
        phi_frame.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

        self.phi_canvas = tk.Canvas(phi_frame, bg="#111111",
                                     highlightthickness=0, height=200)
        self.phi_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

    def _start_reader(self):
        """Read JSON lines from stdin in a background thread."""
        def reader():
            try:
                for line in sys.stdin:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        data = json.loads(line)
                        self.latest = data
                    except json.JSONDecodeError:
                        pass
            except Exception:
                pass
            finally:
                self.running = False

        t = threading.Thread(target=reader, daemon=True)
        t.start()

    def _poll(self):
        """Poll for new data and redraw."""
        if self.latest is not None:
            self._update(self.latest)
            self.latest = None

        if self.running:
            self.root.after(50, self._poll)
        else:
            self.lbl_status.config(text="Stream ended", fg="#F44336")

    def _update(self, data):
        self.lbl_status.config(
            text=f"Cycle {data.get('cycle', '?')}  |  "
                 f"{data.get('formula', '?')} @ {data.get('T_K', '?'):.1f} K",
            fg="#4CAF50")

        # Info panel
        self.info_labels["Formula"].config(text=data.get("formula", "?"))
        self.info_labels["T (K)"].config(text=f"{data.get('T_K', 0):.2f}")
        self.info_labels["Cycle"].config(text=str(data.get("cycle", 0)))
        self.info_labels["F_bulk (J)"].config(text=f"{data.get('F_bulk', 0):.4e}")
        self.info_labels["F_grad (J)"].config(text=f"{data.get('F_gradient', 0):.4e}")
        self.info_labels["F_total (J)"].config(text=f"{data.get('F_total', 0):.4e}")
        self.info_labels["F_total (Eh)"].config(text=f"{data.get('F_total_Eh', 0):.4e}")
        self.info_labels["a"].config(text=f"{data.get('a', 0):.4e}")
        self.info_labels["b"].config(text=f"{data.get('b', 0):.4e}")
        self.info_labels["κ"].config(text=f"{data.get('kappa', 0):.4e}")

        a_val = data.get("a", 0)
        b_val = data.get("b", 1e-30)
        phi_eq = math.sqrt(abs(a_val) / (2.0 * abs(b_val) + 1e-30))
        self.info_labels["φ_eq"].config(text=f"{phi_eq:.4e}")

        # Bar chart
        U = data.get("U", [0]*8)
        while len(U) < 8:
            U.append(0.0)
        self._draw_bars(U)

        # Density profile
        phi = data.get("phi", [])
        self._draw_phi(phi)

    def _draw_bars(self, U):
        c = self.bar_canvas
        c.delete("all")
        w = c.winfo_width()
        h = c.winfo_height()
        if w < 50 or h < 50:
            return

        n = 8
        margin_top = 20
        margin_bot = 30
        margin_left = 80
        margin_right = 20
        bar_area_w = w - margin_left - margin_right
        bar_area_h = h - margin_top - margin_bot
        bar_h = max(1, (bar_area_h - (n - 1) * 4) // n)

        # Find max absolute value for scaling
        max_val = max(abs(v) for v in U) if any(U) else 1.0
        if max_val == 0:
            max_val = 1.0

        for i in range(n):
            y0 = margin_top + i * (bar_h + 4)
            y1 = y0 + bar_h

            # Background bar
            c.create_rectangle(margin_left, y0,
                               margin_left + bar_area_w, y1,
                               fill=BAR_BG, outline="")

            # Value bar
            frac = U[i] / max_val
            bw = int(abs(frac) * bar_area_w)
            if frac >= 0:
                c.create_rectangle(margin_left, y0,
                                   margin_left + bw, y1,
                                   fill=CHANNEL_COLORS[i], outline="")
            else:
                c.create_rectangle(margin_left + bar_area_w - bw, y0,
                                   margin_left + bar_area_w, y1,
                                   fill=CHANNEL_COLORS[i], outline="")

            # Label
            c.create_text(margin_left - 5, (y0 + y1) // 2,
                          text=CHANNEL_NAMES[i], anchor="e",
                          fill=FG_COLOR, font=("Consolas", 9))

            # Value text
            c.create_text(margin_left + bar_area_w + 5, (y0 + y1) // 2,
                          text=f"{U[i]:.2e}", anchor="w",
                          fill="#aaa", font=("Consolas", 8))

    def _draw_phi(self, phi):
        c = self.phi_canvas
        c.delete("all")
        w = c.winfo_width()
        h = c.winfo_height()
        if w < 20 or h < 20 or len(phi) < 2:
            c.create_text(w // 2, h // 2, text="No profile data",
                          fill="#555", font=("Consolas", 9))
            return

        margin = 10
        plot_w = w - 2 * margin
        plot_h = h - 2 * margin

        max_phi = max(abs(v) for v in phi) if phi else 1.0
        if max_phi == 0:
            max_phi = 1.0

        # Zero line
        y_zero = margin + plot_h // 2
        c.create_line(margin, y_zero, margin + plot_w, y_zero,
                      fill="#444", dash=(2, 4))

        # Plot phi(x)
        points = []
        for i, v in enumerate(phi):
            x = margin + (i / (len(phi) - 1)) * plot_w
            y = y_zero - (v / max_phi) * (plot_h // 2)
            points.append((x, y))

        for j in range(len(points) - 1):
            c.create_line(points[j][0], points[j][1],
                          points[j + 1][0], points[j + 1][1],
                          fill="#bb86fc", width=2)

        # Labels
        c.create_text(margin, margin, text=f"+{max_phi:.2e}",
                      anchor="nw", fill="#666", font=("Consolas", 7))
        c.create_text(margin, h - margin, text=f"-{max_phi:.2e}",
                      anchor="sw", fill="#666", font=("Consolas", 7))


# ============================================================================
# Entry point
# ============================================================================

def main():
    root = tk.Tk()
    Gas2Monitor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
