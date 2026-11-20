"""Contract checks for the hardware preflight and owned launch boundaries."""

import os
from pathlib import Path
import subprocess

from ament_index_python.packages import get_package_prefix


def test_preflight_uses_target_workspace_and_blocks_unknown_arm_state(tmp_path):
    fake_ssh = tmp_path / "fake-hardware-ssh"
    args_file = tmp_path / "ssh-args"
    fake_ssh.write_text(
        "#!/usr/bin/env python3\n"
        "import os, pathlib, sys\n"
        "args = sys.argv[1:]\n"
        "pathlib.Path(os.environ['MUR_FAKE_HARDWARE_SSH_ARGS']).write_text(repr(args))\n"
        "command = ' '.join(args)\n"
        "required = ['lab-host', 'bash -lc', 'setup_mur_hardware_host.sh', '--check --profile', '${HOME}/colcon_ws', 'MUR_CHECK_UR_NETWORK=true', 'UR10_l', 'UR10_r']\n"
        "if not all(item in command for item in required): sys.exit(71)\n"
        "print('MUR_HOST_CHECK: summary=ok blocking=0 warnings=0', flush=True)\n"
        "print('MUR_HOST_CHECK: status=ok issue=ur_dashboard_state host=UR10_l', flush=True)\n"
    )
    fake_ssh.chmod(0o755)
    executable = Path(get_package_prefix("match_mur_gui")) / "lib/match_mur_gui/mur_rviz_docking_prototype"
    environment = os.environ.copy()
    environment.update({
        "MUR_FAKE_HARDWARE_SSH": str(fake_ssh),
        "MUR_FAKE_HARDWARE_SSH_ARGS": str(args_file),
        "MUR_COMMAND_CENTER_SETTINGS": str(tmp_path / "settings.ini"),
        "QT_QPA_PLATFORM": "offscreen",
    })
    result = subprocess.run(
        [str(executable), "--contract-test-hardware-start"],
        env=environment, text=True, capture_output=True, timeout=10,
    )
    assert result.returncode == 0, result.stderr + result.stdout
    assert "Hardware start contract passed" in result.stderr + result.stdout
    assert args_file.exists()

