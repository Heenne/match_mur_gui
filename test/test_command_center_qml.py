"""Acceptance checks for the QML command center view."""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_command_center_qml_uses_persistent_session_and_disconnected_status():
    qml = (ROOT / "command_center/qml/Main.qml").read_text()
    for binding in ("session.preset", "session.target", "session.robot", "session.view"):
        assert binding in qml
    assert "session.connectionState" in qml
    assert "Key_1" in qml and "selectPage" in qml
    assert "New preset" in qml


def test_mocap_is_an_optional_session_view_with_explicit_bridge_ownership():
    qml = (ROOT / "command_center/qml/Main.qml").read_text()
    panel = (ROOT / "command_center/qml/MocapPanel.qml").read_text()
    assert '"Mocap"' in qml
    assert "Panels.MocapPanel" in qml
    for binding in (
        "controller.poseStatus", "controller.poseFrame", "controller.dataAgeSeconds",
        "controller.frequencyHz", "controller.mapStatus", "controller.freezeStatus",
        "controller.bridgeState", "controller.startBridge()", "controller.stopBridge()",
        "controller.setMapOutput(true)", "controller.setFrozen(!root.controller.frozen)",
    ):
        assert binding in panel
    assert "app-owned" in panel and "external" in panel


def test_hardware_robot_view_has_selected_ur_arm_hold_to_run_and_visible_stop():
    qml = (ROOT / "command_center/qml/Main.qml").read_text()
    panel = (ROOT / "command_center/qml/UrArmJog.qml").read_text()
    assert "Panels.UrArmJog" in qml and 'visible: session.mode === "Hardware"' in qml
    for contract in (
        "controller.readiness", "controller.selectArm", "controller.setArmed",
        "controller.press(modelData.axis)", "controller.release()",
        "controller.stop(\"focus lost\")", "STOP · ",
        "Translation ≤ 0.05 m/s", "Rotation ≤ 0.20 rad/s",
    ):
        assert contract in panel
