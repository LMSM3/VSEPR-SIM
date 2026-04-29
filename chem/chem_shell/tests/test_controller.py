"""chem_shell controller tests -- classification, energy parsing, presets, dispatch."""
import sys, json, pytest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import controller as ctrl


class TestClassify:
    def test_combustion(self):
        assert ctrl.classify_reaction("CH4 + 2O2 -> CO2 + 2H2O") == "combustion"

    def test_combustion_with_energy(self):
        assert ctrl.classify_reaction("C3H8 + 5O2 -> 3CO2 + 4H2O + 2219 kJ") == "combustion"

    def test_decomposition(self):
        assert ctrl.classify_reaction("2KClO3 -> 2KCl + 3O2") == "decomposition"

    def test_decomposition_carbonate(self):
        assert ctrl.classify_reaction("CaCO3 -> CaO + CO2") == "decomposition"

    def test_acid_base(self):
        assert ctrl.classify_reaction("HCl + NaOH -> NaCl + H2O") == "acid-base"

    def test_general_fallback(self):
        assert ctrl.classify_reaction("N2 + 3H2 -> 2NH3") == "general"


class TestEnergy:
    def test_positive_energy(self):
        val, mode = ctrl.parse_energy("CH4 + 2O2 -> CO2 + 2H2O + 891 kJ")
        assert val == 891.0
        assert mode == "exothermic"

    def test_large_energy(self):
        val, mode = ctrl.parse_energy("C3H8 + 5O2 -> 3CO2 + 4H2O + 2219 kJ")
        assert val == 2219.0

    def test_no_energy(self):
        val, mode = ctrl.parse_energy("2AgCl -> 2Ag + Cl2")
        assert val is None
        assert mode is None

    def test_fractional_energy(self):
        val, _ = ctrl.parse_energy("X -> Y + 12.5 kJ")
        assert val == 12.5


class TestBuildState:
    def test_state_keys(self):
        st = ctrl.build_state("H2 + Cl2 -> 2HCl")
        for key in ("timestamp", "reaction", "class", "energy_kj", "mode",
                     "shell_anim", "web_anim", "source_class"):
            assert key in st

    def test_combustion_anims(self):
        st = ctrl.build_state("CH4 + 2O2 -> CO2 + 2H2O + 891 kJ")
        assert st["shell_anim"] == "flash_arrow"
        assert st["web_anim"] == "heat_burst"

    def test_decomposition_anims(self):
        st = ctrl.build_state("2KClO3 -> 2KCl + 3O2")
        assert st["shell_anim"] == "split_burst"
        assert st["web_anim"] == "fragment_scatter"


class TestDispatch:
    def test_help_commands(self):
        for cmd in ("help", "!", "!!", "9"):
            resp = ctrl.handle_command(cmd)
            assert "Commands" in resp

    def test_extended_help(self):
        for cmd in ("!!!", "99"):
            resp = ctrl.handle_command(cmd)
            assert "Extended Help" in resp

    def test_preset_998(self):
        resp = ctrl.handle_command("998")
        assert "Preset 998" in resp
        assert "decomposition" in resp

    def test_preset_999(self):
        resp = ctrl.handle_command("999")
        assert "Preset 999" in resp
        assert "combustion" in resp

    def test_direct_reaction(self):
        resp = ctrl.handle_command("Zn + 2HCl -> ZnCl2 + H2")
        assert "Reaction" in resp
        assert "Class" in resp

    def test_unrecognized(self):
        resp = ctrl.handle_command("gibberish")
        assert "Unrecognized" in resp


class TestStateIO:
    def test_write_and_read(self):
        st = ctrl.build_state("Fe + S -> FeS")
        ctrl.write_state(st)
        loaded = json.loads(ctrl.STATE_PATH.read_text(encoding="utf-8"))
        assert loaded["reaction"] == "Fe + S -> FeS"

    def test_history_append(self):
        before = ctrl.HISTORY_PATH.read_text(encoding="utf-8").count("\n")
        ctrl.write_state(ctrl.build_state("Mg + O2 -> MgO"))
        after = ctrl.HISTORY_PATH.read_text(encoding="utf-8").count("\n")
        assert after >= before + 1


class TestShellAnim:
    def test_combustion_frames(self):
        st = ctrl.build_state("CH4 + 2O2 -> CO2 + 2H2O + 891 kJ")
        frames = ctrl.shell_anim_frames(st)
        assert "ignition" in frames
        assert "heat released" in frames

    def test_decomposition_frames(self):
        st = ctrl.build_state("CaCO3 -> CaO + CO2")
        frames = ctrl.shell_anim_frames(st)
        assert "===>" in frames
        assert "---->" in frames
