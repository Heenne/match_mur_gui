"""Process ownership contract using a fake launcher; never starts Gazebo or ROS."""
import os
from pathlib import Path
import subprocess

from ament_index_python.packages import get_package_prefix


def test_fake_launcher_captures_argv_environment_streams_exit_and_owned_stop(tmp_path):
    fake = tmp_path / "fake-launcher"
    fake.write_text(
        "#!/usr/bin/env python3\n"
        "import os, signal, subprocess, sys, time\n"
        "if sys.argv[1] == 'exit':\n"
        " print('fake stdout argv=' + repr(sys.argv[1:]) + ' domain=' + os.getenv('ROS_DOMAIN_ID'), flush=True)\n"
        " print('fake stderr', file=sys.stderr, flush=True)\n"
        " sys.exit(7)\n"
        "if sys.argv[1:2] == ['launch']:\n"
        " print('fake simulation argv=' + repr(sys.argv[1:]), flush=True)\n"
        " sys.exit(0)\n"
        "if sys.argv[1:2] == ['sim']:\n"
        " print('fake gz argv=' + repr(sys.argv[1:]), flush=True)\n"
        " subprocess.Popen(['sleep', '60'])\n"
        " while True: time.sleep(1)\n"
        "print('fake hold ready', flush=True)\n"
        "subprocess.Popen(['sleep', '60'])\n"
        "while True: time.sleep(1)\n"
    )
    fake.chmod(0o755)
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir()
    (fake_bin / "ros2").symlink_to(fake)
    (fake_bin / "gz").symlink_to(fake)
    executable = Path(get_package_prefix("match_mur_gui")) / "lib/match_mur_gui/mur_rviz_docking_prototype"
    environment = os.environ.copy()
    environment.update({
        "MUR_FAKE_LAUNCHER": str(fake),
        "ROS_DOMAIN_ID": "47",
        "PATH": str(fake_bin) + os.pathsep + environment["PATH"],
        "QT_QPA_PLATFORM": "offscreen",
    })
    external = subprocess.Popen(["sleep", "30"])
    try:
        result = subprocess.run(
            [str(executable), "--contract-test-process-manager"],
            env=environment, text=True, capture_output=True, timeout=20,
        )
        assert result.returncode == 0, result.stderr + result.stdout
        assert "Process manager contract passed" in result.stderr + result.stdout
        assert external.poll() is None
    finally:
        external.terminate()
        external.wait(timeout=5)


def test_remote_simulation_uses_selected_host_and_stops_only_its_owned_run(tmp_path):
    fake_ssh = tmp_path / "fake-ssh"
    stop_file = tmp_path / "remote-stop"
    args_file = tmp_path / "ssh-args"
    fake_ssh.write_text(
        "#!/usr/bin/env python3\n"
        "import os, pathlib, sys, time\n"
        "args = sys.argv[1:]\n"
        "with open(os.environ['MUR_FAKE_SSH_ARGS_FILE'], 'a') as f: f.write(repr(args) + '\\n')\n"
        "script = ' '.join(args)\n"
        "if 'kill -INT --' in script:\n"
        " if 'MUR_GUI_RUN_ID=' not in script or '/proc/' not in script or not any(host in script for host in ('sim-host', 'lab-host')): sys.exit(51)\n"
        " pathlib.Path(os.environ['MUR_FAKE_REMOTE_STOP_FILE']).write_text(script)\n"
        " sys.exit(0)\n"
        "is_sim = 'ros2 launch mur_launch_sim' in script\n"
        "is_hardware = 'start_mur620_hardware_logged.sh' in script\n"
        "if not (is_sim or is_hardware): sys.exit(52)\n"
        "if is_hardware and ('CLEAN_START=false' not in script or 'BUILD_BEFORE_LAUNCH=false' not in script or 'launch_ur_l:=false' not in script or 'launch_mir:=true' not in script or '${HOME}/colcon_ws' not in script): sys.exit(53)\n"
        "print('__MUR_GUI_PID__=4242', flush=True)\n"
        "print('remote launch ready', flush=True)\n"
        "while not pathlib.Path(os.environ['MUR_FAKE_REMOTE_STOP_FILE']).exists(): time.sleep(.02)\n"
        "print('remote launch stopped', flush=True)\n"
    )
    fake_ssh.chmod(0o755)
    executable = Path(get_package_prefix("match_mur_gui")) / "lib/match_mur_gui/mur_rviz_docking_prototype"
    environment = os.environ.copy()
    environment.update({
        "MUR_FAKE_SSH": str(fake_ssh),
        "MUR_FAKE_SSH_ARGS_FILE": str(args_file),
        "MUR_FAKE_REMOTE_STOP_FILE": str(stop_file),
        "QT_QPA_PLATFORM": "offscreen",
    })
    result = subprocess.run(
        [str(executable), "--contract-test-remote-process-manager"],
        env=environment, text=True, capture_output=True, timeout=20,
    )
    assert result.returncode == 0, result.stderr + result.stdout + (args_file.read_text() if args_file.exists() else "")
    assert "Remote process manager contract passed" in result.stderr + result.stdout
    assert stop_file.exists()
    assert args_file.exists() and "sim-host" in args_file.read_text()
    assert "ros2 launch mur_launch_sim" in args_file.read_text()
    assert "start_mur620_hardware_logged.sh" in args_file.read_text()
    assert "mir_localization_source:=mocap" in args_file.read_text()
    assert "CLEAN_START=false" in args_file.read_text()
    assert "BUILD_BEFORE_LAUNCH=false" in args_file.read_text()
    assert "${HOME}/colcon_ws" in args_file.read_text()
    assert "/home/rosmatch" not in args_file.read_text()
