"""Headless command-line contracts for the RViz host."""

import os
import subprocess

from ament_index_python.packages import get_package_prefix


def _command_center():
    prefix = get_package_prefix("match_mur_gui")
    return os.path.join(prefix, "lib", "match_mur_gui", "mur_command_center")


def _run(*args):
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    return subprocess.run(
        [_command_center(), *args], env=env, text=True, capture_output=True, timeout=10
    )


def test_help_is_available_without_a_display():
    result = _run("--help")
    assert result.returncode == 0
    assert "--config FILE.rviz" in result.stdout + result.stderr
    assert "--context LABEL" in result.stdout + result.stderr


def test_previous_ros_command_starts_command_center():
    legacy_command = os.path.join(os.path.dirname(_command_center()), "mur_rviz_docking_prototype")
    result = subprocess.run([legacy_command, "--help"],
                            env=dict(os.environ, QT_QPA_PLATFORM="offscreen"),
                            text=True, capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr
    assert "Usage: mur_command_center" in result.stdout + result.stderr


def test_missing_rviz_config_is_rejected_before_opening_the_window():
    result = _run("--config", "/tmp/mur-command-center-config-that-does-not-exist.rviz")
    assert result.returncode == 2
    assert "configuration does not exist" in result.stderr


def test_unknown_arguments_are_rejected():
    result = _run("--nonsense")
    assert result.returncode == 2
    assert "Unknown or incomplete argument" in result.stderr


def test_remote_discovery_configures_domain_and_direct_peer_before_ros_init():
    result = _run("--contract-test-remote-discovery")
    assert result.returncode == 0, result.stderr
    assert "Remote discovery contract passed" in result.stderr + result.stdout


def test_gazebo_auto_open_preference_is_persistent_per_preset():
    result = _run("--contract-test-gazebo-preset")
    assert result.returncode == 0, result.stderr
    assert "Gazebo preset contract passed" in result.stderr + result.stdout
