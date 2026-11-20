"""Acceptance checks for the QML command center view."""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_command_center_qml_uses_persistent_session_and_disconnected_status():
    qml = (ROOT / "prototype/qml/Main.qml").read_text()
    for binding in ("session.preset", "session.target", "session.robot", "session.view"):
        assert binding in qml
    assert "session.connectionState" in qml
    assert "Key_1" in qml and "selectPage" in qml
    assert "New preset" in qml
