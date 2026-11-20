"""Behavior contract for the native session log store and process log jump."""
import os
from pathlib import Path
import subprocess

from ament_index_python.packages import get_package_prefix


def test_log_store_keeps_filters_independent_exports_views_and_redacts(tmp_path):
    executable = Path(get_package_prefix("match_mur_gui")) / "lib/match_mur_gui/mur_rviz_docking_prototype"
    environment = os.environ.copy()
    environment["XDG_DATA_HOME"] = str(tmp_path / "data")
    environment["QT_QPA_PLATFORM"] = "offscreen"
    result = subprocess.run(
        [str(executable), "--contract-test-log-store"],
        env=environment, text=True, capture_output=True, timeout=10,
    )
    assert result.returncode == 0, result.stderr + result.stdout
    assert "Log store contract passed" in result.stderr + result.stdout


def test_native_log_dock_exposes_source_tabs_filters_exports_and_process_jump():
    root = Path(__file__).resolve().parents[1]
    host = (root / "prototype/rviz_docking_main.cpp").read_text()
    panel = (root / "prototype/log_panel.cpp").read_text()
    store = (root / "prototype/log_store.cpp").read_text()
    processes = (root / "prototype/qml/ProcessesPanel.qml").read_text()
    assert '"ROS /rosout"' in store and '"All logs"' in store
    assert '"Export full recording…"' in panel and '"Export left filtered view…"' in panel
    assert "QDockWidget::DockWidgetFloatable" in host
    assert 'logs.showSource(root.processManager.logSource(modelData.name))' in processes
