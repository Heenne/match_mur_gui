"""Contract checks for the native Qt session/QML seam."""
import os
from pathlib import Path
import subprocess

from ament_index_python.packages import get_package_prefix


ROOT = Path(__file__).resolve().parents[1]


def test_native_session_persists_user_choices_and_starts_disconnected():
    host = (ROOT / "prototype/rviz_docking_main.cpp").read_text()
    qml = (ROOT / "prototype/qml/Main.qml").read_text()
    assert "QSettings::UserScope" in host and '"MATCH", "MuRCommandCenter"' in host
    assert 'value("activePreset", "Local simulation")' in host
    for field in ("view", "target", "robot", "mode"):
        assert f'group + "{field}"' in host
    assert 'void selectMode(const QString &mode)' in host
    assert 'session.selectMode(currentText)' in qml
    assert 'QString connection_state_{"Disconnected"}' in host
    assert "rclcpp::init(0, nullptr)" in host
    assert host.index("setCentralWidget(command_center)") < host.index("void connect_ros()")
    connect_action = host.index("void connect_ros()")
    assert "rclcpp::init(0, nullptr)" in host[connect_action:]
    assert "session.connectToTarget()" in qml
    assert "session.selectView" in qml


def test_new_presets_default_to_overview_and_qml_is_embedded_in_native_host():
    host = (ROOT / "prototype/rviz_docking_main.cpp").read_text()
    qml = (ROOT / "prototype/qml/Main.qml").read_text()
    assert 'setValue(group + "view", "Overview")' in host
    assert "QQuickWidget" in host and 'setContextProperty("session", session_)' in host
    assert qml.startswith("import QtQuick")
    assert "Key_1" in qml and "Key_7" in qml
    assert "New preset" in qml


def test_session_choices_restore_after_restart_without_starting_ros(tmp_path):
    executable = Path(get_package_prefix("match_mur_gui")) / "lib/match_mur_gui/mur_rviz_docking_prototype"
    environment = os.environ.copy()
    environment["MUR_COMMAND_CENTER_SETTINGS"] = str(tmp_path / "session.ini")
    environment["QT_QPA_PLATFORM"] = "offscreen"
    result = subprocess.run(
        [str(executable), "--contract-test-session"],
        env=environment, text=True, capture_output=True, timeout=10,
    )
    assert result.returncode == 0, result.stderr
    assert "Session contract passed" in result.stderr + result.stdout


def test_connect_action_exposes_live_ros_graph_diagnostics(tmp_path):
    executable = Path(get_package_prefix("match_mur_gui")) / "lib/match_mur_gui/mur_rviz_docking_prototype"
    environment = os.environ.copy()
    environment["MUR_COMMAND_CENTER_SETTINGS"] = str(tmp_path / "session.ini")
    environment["ROS_DOMAIN_ID"] = "87"
    environment["QT_QPA_PLATFORM"] = "offscreen"
    result = subprocess.run(
        [str(executable), "--contract-test-ros-session-monitor"],
        env=environment, text=True, capture_output=True, timeout=10,
    )
    assert result.returncode == 0, result.stderr
    assert "ROS session monitor contract passed" in result.stderr + result.stdout
