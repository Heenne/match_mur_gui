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
Bundles include mode, target host, ROS domain, robot, visualization choice, and optional local
workspace/RViz config references. Import maps these references to local paths;
blank mappings explicitly leave the references unassigned. The portable
allowlist omits the personal last view, layout, log filters, and credentials.

Connecting to a non-local target resolves its host asynchronously, sets the
profile's ROS domain before `rclcpp::init()`, and adds the resolved IPv4
addresses as Fast DDS direct peers. The Diagnosis view reports the configured
route separately from ROS graph results; DNS resolution does not prove UDP
reachability. VPN/LAN routing and firewall access for DDS traffic must already
be available on the client and remote host.

Build from a sourced ROS 2 Jazzy workspace:

```bash
colcon build --packages-select match_mur_gui --cmake-args -DBUILD_TESTING=ON
source install/setup.bash
ros2 run match_mur_gui mur_rviz_docking_prototype
```

RViz loads the active preset's machine-local config path. Pass
`--config /path/to/view.rviz` to override it for this launch. Connecting checks
that the selected profile provides a readable RViz config before initializing
ROS, then sets `use_sim_time` from the profile's Simulation or Hardware mode
before starting RViz. Changing preset, mode, target, robot, visualization, ROS
domain, or effective config disconnects the old RViz session; connect again to
attach to the newly selected context. The dock titles and diagnostics show the
same active profile. The optional `--context` label is retained for the host's
initial title. The GUI supports keys 1–7 to select Overview, Visualization,
Robot, Processes, Diagnosis, Logs, and Settings.

The host remains a prototype; ROS graph connection feedback is limited to the
RViz node starting. No simulation or hardware launch process is wired to the
shell.
