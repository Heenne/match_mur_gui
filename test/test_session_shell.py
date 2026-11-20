"""Contract checks for the persistent desktop session shell."""
import json
from pathlib import Path

from match_mur_gui.session_shell import SessionStore


def test_session_selection_and_each_preset_view_survive_restart(tmp_path):
    path = tmp_path / "user-config" / "session.json"
    store = SessionStore(path)
    assert store.session == {
        "preset": "Local simulation", "target": "localhost",
        "robot": "mur620d", "view": "Overview", "mode": "Simulation",
        "connected": False,
    }

    store.select_view("Visualization")
    store.select_preset("Trial field", target="lab-host", robot="mur620b")
    assert store.session["view"] == "Overview"
    store.select_view("Processes")
    store.select_robot("mur620c")

    restored = SessionStore(path)
    assert restored.session == {
        "preset": "Trial field", "target": "lab-host", "robot": "mur620c",
        "view": "Processes", "mode": "Hardware", "connected": False,
    }
    restored.select_preset("Local simulation")
    assert restored.session["view"] == "Visualization"


def test_new_preset_defaults_to_overview_and_startup_is_disconnected(tmp_path):
    path = tmp_path / "session.json"
    path.write_text(json.dumps({"active_preset": "Brand new", "presets": {}}))
    store = SessionStore(path)
    assert store.session["view"] == "Overview"
    assert store.session["connected"] is False


def test_session_contract_is_independent_of_ros_and_process_startup():
    root = Path(__file__).resolve().parents[1]
    source = (root / "match_mur_gui/session_shell.py").read_text()
    assert "subprocess" not in source
    assert "rclpy" not in source
    assert "def create_session_bridge" in source
