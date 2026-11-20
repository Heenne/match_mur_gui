"""Gazebo world view is launched as an independently owned GUI client."""

from match_mur_gui.gazebo_client import gazebo_client_command


def test_client_command_uses_gazebo_sim_gui_only_mode():
    assert gazebo_client_command() == ["gz", "sim", "-g"]


def test_client_command_allows_an_explicit_gazebo_executable():
    assert gazebo_client_command("/opt/gz/bin/gz") == ["/opt/gz/bin/gz", "sim", "-g"]
