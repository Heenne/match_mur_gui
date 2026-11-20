#!/usr/bin/env python3
"""Assemble a standalone Ubuntu ROS 2 Jazzy .deb from a colcon install tree."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET


PACKAGE = 'match-mur-gui'
ROS_PACKAGE = 'match_mur_gui'
ROS_DEPENDENCY_PREFIX = 'ros-jazzy-'
NATIVE_DEPENDENCIES = {
    'libqt5gui5', 'libqt5network5', 'libqt5qml5', 'libqt5quick5',
    'libqt5quickwidgets5', 'libqt5widgets5', 'python3-pyqt5',
    'qml-module-qtquick-controls2', 'qml-module-qtquick-dialogs',
    'qml-module-qtquick-layouts', 'qmlscene',
}
APP = ('mur-base-gui', 'MuR Basis-GUI', 'MuR-Roboter bedienen',
       'match_mur_gui', 'general_mur_gui.py', 'MuR;Roboter;Basis;ROS;')
MARKER = 'match_mur_gui/scripts/package_deb.py'


def runtime_dependencies(package_xml: Path) -> list[str]:
    root = ET.parse(package_xml).getroot()
    dependencies = set(NATIVE_DEPENDENCIES)
    for element in root.findall('depend') + root.findall('exec_depend'):
        key = (element.text or '').strip()
        if not key:
            continue
        if key.startswith(('python3-', 'qml-module-', 'lib')) or key == 'qmlscene':
            dependencies.add(key)
        else:
            dependencies.add(ROS_DEPENDENCY_PREFIX + key.replace('_', '-'))
    dependencies.discard(ROS_DEPENDENCY_PREFIX + ROS_PACKAGE.replace('_', '-'))
    return sorted(dependencies)


def _launcher() -> bytes:
    return ('''#!/usr/bin/env bash
# Managed by match_mur_gui/scripts/package_deb.py
set -e
ros_distro="${ROS_DISTRO:-jazzy}"
ros_setup="/opt/ros/${ros_distro}/setup.bash"
if [[ ! -r "$ros_setup" ]]; then
  printf 'MuR GUI: ROS 2 %s setup not found at %s\\n' "$ros_distro" "$ros_setup" >&2
  exit 1
fi
source "$ros_setup"
export AMENT_PREFIX_PATH="/usr${AMENT_PREFIX_PATH:+:$AMENT_PREFIX_PATH}"
export WS="${WS:-${XDG_DATA_HOME:-$HOME/.local/share}/match_mur_gui}"
mkdir -p "$WS"
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-62}"
exec ros2 run match_mur_gui general_mur_gui.py "$@"
''').encode()


def _desktop_entry() -> bytes:
    slug, title, comment, _package, _executable, keywords = APP
    return (f'[Desktop Entry]\n'
            f'Type=Application\n'
            f'Name={title}\n'
            f'GenericName=Roboter-GUI\n'
            f'Comment={comment}\n'
            f'Exec=/usr/bin/{slug}\n'
            f'Icon={slug}\n'
            f'StartupWMClass={slug}\n'
            f'Terminal=false\n'
            f'Categories=Utility;\n'
            f'Keywords={keywords}\n'
            f'StartupNotify=true\n'
            f'X-MuR-Gui-Managed={MARKER}\n').encode('utf-8')


def build_deb(source: Path, install_root: Path, output: Path, *, version: str,
              architecture: str | None = None) -> Path:
    source, install_root, output = Path(source), Path(install_root), Path(output)
    package_lib = install_root / 'lib' / ROS_PACKAGE
    package_share = install_root / 'share' / ROS_PACKAGE
    python_site_packages = next((path for path in sorted((install_root / 'lib').glob('python*/site-packages'))
                                 if (path / ROS_PACKAGE).is_dir()), None)
    if not package_lib.is_dir() or not package_share.is_dir() or python_site_packages is None:
        raise ValueError(f'Paketinstallation fehlt: {install_root} muss lib/{ROS_PACKAGE}, '
                         f'lib/python*/site-packages/{ROS_PACKAGE} und share/{ROS_PACKAGE} enthalten')
    if not re.fullmatch(r'[0-9][A-Za-z0-9.+:~\-]*', version):
        raise ValueError(f'Ungültige Debian-Version: {version}')
    if architecture is None:
        architecture = subprocess.run(['dpkg', '--print-architecture'], check=True,
                                      capture_output=True, text=True).stdout.strip()
    if not architecture:
        raise ValueError('dpkg konnte keine Paketarchitektur bestimmen')

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mur-gui-deb-') as temporary:
        root = Path(temporary)
        shutil.copytree(package_lib, root / 'usr/lib' / ROS_PACKAGE)
        shutil.copytree(install_root / 'share', root / 'usr/share', dirs_exist_ok=True)
        python_destination = root / 'usr/lib/python3/dist-packages'
        python_destination.mkdir(parents=True)
        shutil.copytree(python_site_packages / ROS_PACKAGE,
                        python_destination / ROS_PACKAGE)
        for metadata in python_site_packages.glob(f'{ROS_PACKAGE}-*.egg-info'):
            shutil.copytree(metadata, python_destination / metadata.name)
        launcher = root / 'usr/bin' / APP[0]
        launcher.parent.mkdir(parents=True, exist_ok=True)
        launcher.write_bytes(_launcher())
        launcher.chmod(0o755)
        applications = root / 'usr/share/applications'
        applications.mkdir(parents=True, exist_ok=True)
        (applications / f'{APP[0]}.desktop').write_bytes(_desktop_entry())
        for size, suffix in (('scalable', '.svg'), ('48x48', '.png'), ('128x128', '.png')):
            src_suffix = '' if size == 'scalable' else '-' + size.split('x')[0]
            icon_source = source / 'assets/icons' / f'{APP[0]}{src_suffix}{suffix}'
            if icon_source.is_symlink() or not icon_source.is_file():
                raise ValueError(f'Icon fehlt oder ist ein Symlink: {icon_source}')
            icon_dir = root / 'usr/share/icons/hicolor' / size / 'apps'
            icon_dir.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(icon_source, icon_dir / f'{APP[0]}{suffix}')

        control = root / 'DEBIAN/control'
        control.parent.mkdir()
        depends = ', '.join(runtime_dependencies(package_share / 'package.xml'))
        control.write_text(
            f'Package: {PACKAGE}\nVersion: {version}\nArchitecture: {architecture}\n'
            'Maintainer: MuR GUI maintainers <rosmatch@match.uni-hannover.de>\n'
            'Section: science\nPriority: optional\n'
            'Description: MuR robot control desktop GUI for ROS 2 Jazzy\n'
            ' Desktop launcher for the MuR robot control GUI.\n'
            f'Depends: {depends}\n', encoding='utf-8')
        subprocess.run(['dpkg-deb', '--build', '--root-owner-group', str(root), str(output)],
                       check=True)
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--install-root', type=Path, required=True,
                        help='colcon install prefix containing lib/ and share/')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    print(build_deb(args.source, args.install_root, args.output, version=args.version))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
