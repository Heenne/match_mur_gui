"""Acceptance checks for the standalone QML layout prototype."""
from pathlib import Path
import os
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def test_command_center_qml_uses_persistent_session_and_disconnected_status():
    qml = (ROOT / "prototype/qml/Main.qml").read_text()
    for binding in ("session.preset", "session.target", "session.robot", "session.view"):
        assert binding in qml
    assert 'label: "Disconnected"' in qml
    assert "Key_1" in qml and "selectPage" in qml
    assert "New preset" in qml


def test_qml_engine_loads_prototype():
    qmlscene = shutil.which("qmlscene")
    if not qmlscene:
        return
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    try:
        result = subprocess.run(
            [qmlscene, str(ROOT / "prototype/qml/Main.qml")],
            env=env, capture_output=True, text=True, timeout=3,
        )
    except subprocess.TimeoutExpired as error:
        # A running window is the expected result; QML parse/runtime errors exit early.
        output = (error.stdout or b"") + (error.stderr or b"")
        assert b"Error" not in output
        return
    if "could not find a Qt installation" in result.stderr:
        # qtchooser is present in some ROS containers without its Qt 5 runtime.
        return
    assert result.returncode == 0, result.stderr
