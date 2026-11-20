"""The Hardware robot view exposes bounded MiR jog and checked light actions."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_hardware_robot_view_routes_to_mir_control_panel():
    main_qml = (ROOT / "prototype/qml/Main.qml").read_text()
    assert 'Panels.MirHardwareControl' in main_qml
    assert 'visible: session.mode === "Hardware"' in main_qml


def test_mir_jog_requires_explicit_arm_and_stops_on_focus_loss():
    qml = (ROOT / "prototype/qml/MirHardwareControl.qml").read_text()
    assert "mirHardwareControl.setArmed(checked)" in qml
    assert "enabled: mirHardwareControl.armed" in qml
    assert 'panel.stopJog("window lost focus")' in qml
    assert 'panel.stopJog("application lost focus")' in qml
    assert "mirHardwareControl.release()" in qml
    assert "mirHardwareControl.stop(" in qml


def test_mir_lights_show_service_feedback_and_rgb_picker():
    qml = (ROOT / "prototype/qml/MirHardwareControl.qml").read_text()
    assert "mirHardwareControl.rainbowStart()" in qml
    assert "mirHardwareControl.rainbowStop()" in qml
    assert "mirHardwareControl.matchColor()" in qml
    assert "mirHardwareControl.setSolidColor(" in qml
    assert "mirHardwareControl.lightStatus" in qml
    assert "NativeDialogs.ColorDialog" in qml
