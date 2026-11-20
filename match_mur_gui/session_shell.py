"""Persistent, disconnected session model for the native command center shell."""

import json
import os
from pathlib import Path


VIEWS = ("Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings")
DEFAULT_PRESET = "Local simulation"


def _default_preset(name=DEFAULT_PRESET):
    return {
        "mode": "Simulation" if name == DEFAULT_PRESET else "Hardware",
        "target": "localhost",
        "robot": "mur620d",
        "view": "Overview",
    }


def user_session_path():
    config_home = os.environ.get("XDG_CONFIG_HOME")
    if not config_home or not os.path.isabs(config_home):
        config_home = os.path.expanduser("~/.config")
    return Path(config_home) / "match_mur_gui" / "command_center_session.json"


class SessionStore:
    """Small public seam for changing and persisting the active user session."""

    def __init__(self, path=None):
        self.path = Path(path) if path else user_session_path()
        self._data = self._read()
        self._data.setdefault("presets", {})
        self._data.setdefault("active_preset", DEFAULT_PRESET)
        name = self._data["active_preset"]
        self._data["presets"].setdefault(name, _default_preset(name))
        # A restored GUI always starts detached from ROS and external processes.
        self._data["connected"] = False
        self._save()

    def _read(self):
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
            if isinstance(data, dict) and isinstance(data.get("presets", {}), dict):
                return data
        except (OSError, ValueError, TypeError):
            pass
        return {"active_preset": DEFAULT_PRESET, "presets": {DEFAULT_PRESET: _default_preset()}}

    @property
    def session(self):
        preset = self._data["presets"][self._data["active_preset"]]
        return {
            "preset": self._data["active_preset"],
            "target": preset.get("target", "localhost"),
            "robot": preset.get("robot", "mur620d"),
            "view": preset.get("view", "Overview"),
            "mode": preset.get("mode", "Simulation"),
            "connected": False,
        }

    @property
    def presets(self):
        return sorted(self._data["presets"])

    def select_view(self, view):
        if view not in VIEWS:
            raise ValueError(f"Unknown view: {view}")
        self._active()["view"] = view
        self._save()

    def select_preset(self, name, target=None, robot=None, mode=None):
        name = name.strip()
        if not name:
            raise ValueError("Preset name cannot be empty")
        preset = self._data["presets"].setdefault(name, _default_preset(name))
        if target is not None:
            preset["target"] = target
        if robot is not None:
            preset["robot"] = robot
        if mode is not None:
            preset["mode"] = mode
        self._data["active_preset"] = name
        self._save()

    def select_robot(self, robot):
        self._active()["robot"] = robot
        self._save()

    def select_target(self, target):
        self._active()["target"] = target
        self._save()

    def _active(self):
        return self._data["presets"][self._data["active_preset"]]

    def _save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(self.path.suffix + ".tmp")
        temporary.write_text(json.dumps(self._data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        temporary.replace(self.path)


try:
    from PyQt5 import QtCore, QtQuickWidgets, QtWidgets
except ImportError:  # Keep the persistence seam usable on non-GUI build hosts.
    QtCore = QtQuickWidgets = QtWidgets = None


if QtCore is not None:
    class SessionBridge(QtCore.QObject):
        changed = QtCore.pyqtSignal()

        def __init__(self, store):
            super().__init__()
            self.store = store

        @QtCore.pyqtSlot(str)
        def selectView(self, view):
            self.store.select_view(view)
            self.changed.emit()

        @QtCore.pyqtSlot(str)
        def selectPreset(self, name):
            self.store.select_preset(name)
            self.changed.emit()

        @QtCore.pyqtSlot(str)
        def selectRobot(self, robot):
            self.store.select_robot(robot)
            self.changed.emit()

        @QtCore.pyqtSlot(str)
        def selectTarget(self, target):
            self.store.select_target(target)
            self.changed.emit()

        @QtCore.pyqtProperty(str, notify=changed)
        def preset(self): return self.store.session["preset"]

        @QtCore.pyqtProperty(str, notify=changed)
        def target(self): return self.store.session["target"]

        @QtCore.pyqtProperty(str, notify=changed)
        def robot(self): return self.store.session["robot"]

        @QtCore.pyqtProperty(str, notify=changed)
        def view(self): return self.store.session["view"]

        @QtCore.pyqtProperty(str, notify=changed)
        def mode(self): return self.store.session["mode"]

        @QtCore.pyqtProperty("QVariant", notify=changed)
        def presets(self): return self.store.presets


    def create_session_bridge(store=None):
        """Return the QObject context model expected by a Qt/QML host."""
        return SessionBridge(store or SessionStore())
else:
    def create_session_bridge(*args, **kwargs):
        raise RuntimeError("PyQt5 is required to expose the session model to QML")
