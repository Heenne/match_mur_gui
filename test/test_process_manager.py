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
        "print('fake hold ready', flush=True)\n"
        "subprocess.Popen(['sleep', '60'])\n"
        "while True: time.sleep(1)\n"
    )
    fake.chmod(0o755)
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir()
    (fake_bin / "ros2").symlink_to(fake)
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
