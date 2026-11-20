# Native command center host

`rviz_docking_main.cpp` is the native Qt Widgets host. It embeds
`qml/Main.qml` in a `QQuickWidget`, exposes the `SessionController` as the
`session` QML context object, and keeps RViz in its own dock. The session shell
is visible before any ROS initialization. Choosing **Connect** initializes the
ROS node and creates the RViz dock.

Session state is stored by `QSettings` in the user's application config
directory (`MATCH/MuRCommandCenter`), outside the source tree. Preset, target,
robot, mode, and last view are stored per preset. A new preset opens Overview.
Connection state is in memory only and always starts Disconnected. The shell
does not start simulator or robot processes.

Profiles can be edited and exported as versioned `.murpreset.json` bundles.
Bundles include mode, target, robot, visualization choice, and optional local
workspace/RViz config references. Import maps these references to local paths;
blank mappings explicitly leave the references unassigned. The portable
allowlist omits the personal last view, layout, log filters, and credentials.

Build from a sourced ROS 2 Jazzy workspace:

```bash
colcon build --packages-select match_mur_gui --cmake-args -DBUILD_TESTING=ON
source install/setup.bash
ros2 run match_mur_gui mur_rviz_docking_prototype
```

Pass an existing RViz config with `--config /path/to/view.rviz`. The optional
`--context` label is shown in the RViz dock title. The GUI supports keys 1–7 to
select Overview, Visualization, Robot, Processes, Diagnosis, Logs, and
Settings.

The host remains a prototype; ROS graph connection feedback is limited to the
RViz node starting. No simulation or hardware launch process is wired to the
shell.
