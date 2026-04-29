"""Persistent chemistry controller -- stays alive, handles all parsing/state/animation."""

import sys
import json
import time
import re
import random
from pathlib import Path

MOTD_LINES = [
    "The universe is under no obligation to make sense to you. -- Neil deGrasse Tyson",
    "Nothing in life is to be feared, it is only to be understood. -- Marie Curie",
    "Every aspect of the world today has been affected by chemistry. -- Linus Pauling",
    "The meeting of two personalities is like the contact of two chemical substances. -- C.G. Jung",
    "Chemistry begins in the stars. -- Peter Atkins",
    "All things are poison, and nothing is without poison; the dosage alone makes it so. -- Paracelsus",
    "Thermodynamics is a funny subject. The first time you go through it, you don't understand it at all.",
    "In chemistry, as in architecture: the foundation determines what can be built above.",
    "Exothermic reactions: nature's way of saying 'you're welcome.'",
    "If you're not part of the solution, you're part of the precipitate.",
]

def motd() -> str:
    line = random.choice(MOTD_LINES)
    return (
        "\n"
        "  ========================================\n"
        "   chem_shell v0.1\n"
        "  ========================================\n"
        "  " + line + "\n"
        "  ========================================\n"
        "  Type help for commands, Ctrl-C to quit.\n"
    )

HELP_SHORT = """\
  Commands
  ----------
  help / ! / !!      Syntax help & examples
  !!! / 99           Extended help (thermo categories, energy notation)
  998                 Classroom preset library (balanced reactions)
  999                 NIST / empirical thermochemical preset library

  Or type a reaction directly:
    2AgCl -> 2Ag + Cl2
    CH4 + 2O2 -> CO2 + 2H2O + 891 kJ
"""

HELP_EXTENDED = """\
  Extended Help
  -------------
  Energy terms:  append '+ ### kJ' or '- ### kJ' after products.
  Arrow:         use '->' to separate reactants from products.
  Coefficients:  prefix species with an integer  (2H2O, 3Mg).

  Thermochemical categories
  -------------------------
  combustion      CxHy + O2 -> CO2 + H2O + energy
  decomposition   AB -> A + B
  synthesis        A + B -> AB
  acid-base       HA + BOH -> salt + H2O
  redox           electron-transfer patterns
  general         anything else

  Preset 998 = common classroom balanced reactions
  Preset 999 = NIST / empirical thermochemical references
"""

PRESET_998 = [
    "2AgCl -> 2Ag + Cl2",
    "CaC2 + 2H2O -> C2H2 + Ca(OH)2",
    "Na2S2O3 + I2 -> Na2S4O6 + 2NaI",
    "3Mg + N2 -> Mg3N2",
    "2Fe + 3Cl2 -> 2FeCl3",
    "Zn + 2HCl -> ZnCl2 + H2",
    "2KClO3 -> 2KCl + 3O2",
    "CaCO3 -> CaO + CO2",
]

PRESET_999 = [
    "CH4 + 2O2 -> CO2 + 2H2O + 891 kJ",
    "C2H6 + 3.5O2 -> 2CO2 + 3H2O + 1561 kJ",
    "C3H8 + 5O2 -> 3CO2 + 4H2O + 2219 kJ",
    "2H2 + O2 -> 2H2O + 572 kJ",
    "P2O5 + 3H2O -> 2H3PO4 + 177 kJ",
    "N2 + 3H2 -> 2NH3 + 92 kJ",
    "SO3 + H2O -> H2SO4 + 130 kJ",
    "C + O2 -> CO2 + 394 kJ",
]

STATE_DIR = Path(__file__).parent / "state"
STATE_DIR.mkdir(parents=True, exist_ok=True)
STATE_PATH = STATE_DIR / "current_state.json"
HISTORY_PATH = STATE_DIR / "history.log"

COMMANDS = {
    "help": "show_help",
    "!":    "show_help",
    "!!":   "show_help",
    "9":    "show_help",
    "!!!":  "show_extended_help",
    "99":   "show_extended_help",
    "998":  "run_preset_998",
    "999":  "run_preset_999",
}

_ENERGY_RE = re.compile(r'([+-]?\s*[\d.]+)\s*kJ', re.IGNORECASE)

def parse_energy(text):
    m = _ENERGY_RE.search(text)
    if m:
        val = float(m.group(1).replace(" ", ""))
        return val, ("exothermic" if val > 0 else "endothermic")
    return None, None

def classify_reaction(text):
    t = text.lower()
    lhs = t.split("->")[0] if "->" in t else t
    rhs = t.split("->")[1] if "->" in t else ""
    if "o2" in lhs and "co2" in rhs:
        return "combustion"
    if "+" not in lhs.strip() and "+" in rhs:
        return "decomposition"
    if any(a in t for a in ("hcl", "h2so4", "hno3")) and any(b in t for b in ("naoh", "koh", "ca(oh)2")):
        return "acid-base"
    return "general"

SHELL_ANIMS = {
    "combustion":    "flash_arrow",
    "decomposition": "split_burst",
    "synthesis":     "merge_glow",
    "acid-base":     "neutralize_fade",
    "general":       "sparkline",
}

WEB_ANIMS = {
    "combustion":    "heat_burst",
    "decomposition": "fragment_scatter",
    "synthesis":     "molecule_merge",
    "acid-base":     "ph_gradient",
    "general":       "molecule_flow",
}

def build_state(text):
    energy, mode = parse_energy(text)
    cls = classify_reaction(text)
    return {
        "timestamp":     time.time(),
        "reaction":      text.strip(),
        "class":         cls,
        "energy_kj":     energy,
        "mode":          mode or "unknown",
        "shell_anim":    SHELL_ANIMS.get(cls, "sparkline"),
        "web_anim":      WEB_ANIMS.get(cls, "molecule_flow"),
        "source_class":  "direct",
    }

def write_state(state):
    STATE_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")
    with open(HISTORY_PATH, "a", encoding="utf-8") as f:
        f.write(json.dumps(state) + "\n")

def shell_anim_frames(state):
    rxn = state["reaction"]
    cls = state["class"]
    if cls == "combustion":
        return (
            "  " + rxn + "\n"
            "  " + rxn.replace("->", " .  .  . ") + "\n"
            "  " + rxn.replace("->", " * * * ignition *") + "\n"
            "  >>> heat released  (" + str(state.get("energy_kj", "?")) + " kJ)"
        )
    if cls == "decomposition":
        parts = rxn.split("->")
        if len(parts) == 2:
            return (
                "  [" + parts[0].strip() + "] ==>  " + parts[1].strip() + "\n"
                "  [" + parts[0].strip() + "] ===>  " + parts[1].strip() + "\n"
                "  [" + parts[0].strip() + "] ---->  " + parts[1].strip()
            )
    return (
        "  " + rxn.replace("->", "  ==>  ") + "\n"
        "  " + rxn.replace("->", "  ===>  ") + "\n"
        "  " + rxn.replace("->", "  ---->  ")
    )

def handle_command(cmd):
    c = cmd.strip()
    action = COMMANDS.get(c)
    if action == "show_help":
        return HELP_SHORT
    if action == "show_extended_help":
        return HELP_EXTENDED
    if action == "run_preset_998":
        return _run_preset("998", PRESET_998)
    if action == "run_preset_999":
        return _run_preset("999", PRESET_999)
    if "->" in c:
        st = build_state(c)
        st["source_class"] = "direct"
        write_state(st)
        lines = [
            "  Reaction : " + c,
            "  Class    : " + st["class"],
        ]
        if st["energy_kj"] is not None:
            lines.append("  Energy   : " + str(st["energy_kj"]) + " kJ  (" + st["mode"] + ")")
        lines.append("  Shell    : " + st["shell_anim"])
        lines.append("  Web      : " + st["web_anim"])
        lines.append("")
        lines.append(shell_anim_frames(st))
        return "\n".join(lines)
    return "  Unrecognized command. Type help."

def _run_preset(tag, library):
    out = ["  Preset " + tag + " loaded (" + str(len(library)) + " reactions):"]
    for rxn in library:
        st = build_state(rxn)
        st["source_class"] = "preset_" + tag
        write_state(st)
        energy_note = "  " + str(st["energy_kj"]) + " kJ" if st["energy_kj"] else ""
        out.append("    " + rxn.ljust(50) + " [" + st["class"] + "]" + energy_note)
    return "\n".join(out)

def main():
    if len(sys.argv) < 3:
        print("Usage: controller.py <pipe_in> <pipe_out>")
        sys.exit(1)
    pipe_in = sys.argv[1]
    pipe_out = sys.argv[2]
    while True:
        try:
            with open(pipe_in, "r", encoding="utf-8") as f:
                cmd = f.read().strip()
            if cmd:
                response = handle_command(cmd)
                with open(pipe_out, "w", encoding="utf-8") as f:
                    f.write(response)
                with open(pipe_in, "w", encoding="utf-8") as f:
                    f.write("")
            time.sleep(0.12)
        except KeyboardInterrupt:
            break
        except Exception as e:
            try:
                with open(pipe_out, "w", encoding="utf-8") as f:
                    f.write("  Controller error: " + str(e))
            except Exception:
                pass
            time.sleep(0.3)

if __name__ == "__main__":
    main()
