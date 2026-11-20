"""Standalone Debian package contents and metadata."""

import importlib.util
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('mur_gui_deb', ROOT / 'scripts/package_deb.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


def test_deb_has_menu_entry_icon_launcher_and_ros_dependencies(tmp_path):
    install_root = tmp_path / 'fake-install'
    lib = install_root / 'lib/match_mur_gui'
    python_package = install_root / 'lib/python3.12/site-packages/match_mur_gui'
    share = install_root / 'share/match_mur_gui'
    lib.mkdir(parents=True)
    python_package.mkdir(parents=True)
    share.mkdir(parents=True)
    (lib / 'general_mur_gui.py').write_text('#!/usr/bin/env python3\n')
    (python_package / '__init__.py').write_text('')
    (share / 'package.xml').write_text((ROOT / 'package.xml').read_text())
    output = tmp_path / 'match-mur-gui.deb'

    PACKAGE.build_deb(ROOT, install_root, output, version='1.2.3', architecture='amd64')

    contents = subprocess.run(['dpkg-deb', '--contents', str(output)], check=True,
                              capture_output=True, text=True).stdout
    assert './usr/bin/mur-base-gui' in contents
    assert './usr/share/applications/mur-base-gui.desktop' in contents
    assert './usr/share/icons/hicolor/128x128/apps/mur-base-gui.png' in contents
    assert './usr/lib/match_mur_gui/general_mur_gui.py' in contents
    assert './usr/lib/python3/dist-packages/match_mur_gui/__init__.py' in contents

    fields = subprocess.run(['dpkg-deb', '--field', str(output)], check=True,
                            capture_output=True, text=True).stdout
    assert 'Package: match-mur-gui' in fields
    assert 'Version: 1.2.3' in fields
    assert 'Architecture: amd64' in fields
    assert 'ros-jazzy-rclpy' in fields
    assert 'ros-jazzy-ewellix-interfaces' in fields
    assert 'qml-module-qtquick-controls2' in fields

    extracted = tmp_path / 'extracted'
    subprocess.run(['dpkg-deb', '--extract', str(output), str(extracted)], check=True)
    launcher = (extracted / 'usr/bin/mur-base-gui').read_text()
    assert 'ros_setup="/opt/ros/${ros_distro}/setup.bash"' in launcher
    assert 'source "$ros_setup"' in launcher
    assert 'export AMENT_PREFIX_PATH="/usr${AMENT_PREFIX_PATH:+:$AMENT_PREFIX_PATH}"' in launcher
    assert 'exec ros2 run match_mur_gui general_mur_gui.py "$@"' in launcher
    assert '/home/' not in launcher
    desktop = (extracted / 'usr/share/applications/mur-base-gui.desktop').read_text()
    assert 'Name=MuR Basis-GUI' in desktop
    assert 'Exec=/usr/bin/mur-base-gui' in desktop
    assert 'Icon=mur-base-gui' in desktop


def test_deb_rejects_missing_staged_ros_package(tmp_path):
    with __import__('pytest').raises(ValueError, match='Paketinstallation fehlt'):
        PACKAGE.build_deb(ROOT, tmp_path / 'empty-install', tmp_path / 'out.deb', version='1.0')
