# Native command center host

`rviz_docking_main.cpp` is the native Qt Widgets host. It embeds
`qml/Main.qml` in a `QQuickWidget`, exposes the `SessionController` as the
`session` QML context object, and keeps RViz in its own dock. The session shell
is visible before any ROS initialization. Choosing **Connect** initializes the
ROS node and creates the RViz dock.

Session state is stored by `QSettings` in the user's application config
directory (`MATCH/MuRCommandCenter`), outside the source tree. Preset, target,
robot, mode, Gazebo auto-open preference, and last view are stored per preset. A new preset opens Overview.
Connection state is in memory only and always starts Disconnected. The shell
does not start simulator or robot processes.

Profiles can be edited and exported as versioned `.murpreset.json` bundles.
Bundles include mode, target host, ROS domain, robot, visualization choice, Gazebo auto-open preference, and optional local
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
initial title. The GUI supports keys 1–8 to select Overview, Visualization,
Robot, Processes, Diagnosis, Logs, Settings, and Mocap.

The host remains a prototype; connecting confirms that its RViz node started,
but does not prove reachability of every remote ROS endpoint. Full simulation
and hardware launch flows are not yet wired to the shell.

The Robot view includes profile-routed goals and saved poses. Simulation uses
Nav2 `NavigateToPose` at `/<robot>/navigate_to_pose` and reads fresh
`PoseStamped` ground truth from `/<robot>/ground_truth/pose`. Hardware uses the
MiR `PoseStamped` goal topic and selected robot's cancel topic only when the
expected message type and receiver are discovered. A profile never falls back
to the other mode's adapter. Poses are stored under the user's application
configuration directory; **Import MiR poses** accepts the legacy JSON array
and `{ "poses": [...] }` formats and merges imported entries by name.

The Processes view starts the local MuR620 simulation and manages the Gazebo
world view as a separate `gz sim -g` client. Starting or stopping the client
does not stop the simulation server, `/clock`, ROS bridge, or their logs. The
client can be opened later when `/clock` is discovered in the selected ROS
domain. When auto-open is enabled, starting simulation also requests a ROS
connection and waits for `/clock`; the client is not started if the server is
not confirmed within 60 seconds. Client launch output and failure state appear
in Processes. The app does not discover Gazebo servers through Gazebo Transport.

The optional **Mocap** view reads raw `/qualisys/<robot>/pose`, map-frame
`/qualisys_map/<robot>/pose`, `/tf_static` (`map -> mocap`), and the per-robot
`localization_frozen` status. It displays pose/frame, receipt rate, data age,
map/TF availability, and freeze state. Map output and localization freeze use
the existing Qualisys `SetBool` services. The view can start the existing
`match_mocap_ros2 qualisys_ssh_bridge` with editable SSH/QTM hosts and port;
these settings persist in the user's application settings. A detected
`qualisys_ssh_bridge` node or existing robot pose publisher is treated as an
external bridge: the app reuses its data and never stops it. Only a bridge
started as the named `Qualisys Mocap bridge` process by this app exposes the
stop action. Bridge output and exit state are recorded with the other launch
logs. Map/TF output is enabled for an app-started bridge, with the existing
bridge's calibration validation deciding whether it is available.
