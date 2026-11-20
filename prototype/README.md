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
colcon build --packages-up-to match_mur_gui --cmake-args -DBUILD_TESTING=ON
source install/setup.bash
ros2 run match_mur_gui mur_rviz_docking_prototype
```

The workspace must also contain the `mir_srvs` interface package from
`match-ROS/match_mobile_robotics_jazzy` at
`mir_robot/mir_srvs`. The Debian build checks out this repository and builds
that package as part of the dependency closure. The `.deb` bundles the
generated `mir_srvs` runtime libraries and ament resources; the MiR light
endpoints remain optional and are checked only when used.

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

The Diagnosis view reports ROS graph endpoints and controller-manager state.
In Hardware mode it also reads the selected robot's base/arm controller
managers, lift joints, and separate MuR and MiR batteries. Battery percentages
and lift positions include their data age; values older than three seconds are
marked stale. MiR's reported remaining runtime is labeled as an estimate and
has its own age. No dashboard safety state is inferred from ROS graph presence.
Controller, battery, and lift reads are subscriptions or read-only
`list_controllers` requests; this status path does not start hardware or issue
motion commands.

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

## Remote simulation process prototype

From the Processes view, select a Simulation preset whose target is a remote
host and choose **Start remote simulation**. The target must be a host alias or
hostname accepted by SSH, the remote account must have non-interactive SSH key
access, and `bash -lc` on that host must find the sourced ROS 2 Jazzy workspace
and `mur_launch_sim` package. The selected ROS domain and robot name are passed
to the remote launch. The app records remote stdout/stderr under a host- and
run-specific log source.

Each launch receives a random run ID and starts in a dedicated `setsid`
process group. Stop connects over SSH again and checks the recorded PID's
environment for that exact run ID and its process-group ID before sending
SIGINT to the group. A foreign process is never selected by name or stopped.
If SSH or the ownership check fails, the process row reports **Stop failed**
and the log says the remote run may still be active. This prototype does not
store SSH credentials; configure SSH keys or an agent on the operator account.

## MiR hardware controls

The Robot view exposes MiR base jog and lights only in a connected Hardware
session. Jog is disarmed on startup and after any profile, target, mode, robot,
or connection change. The operator must arm it explicitly. Direction controls
are hold-to-run, stop when released or when application/window focus is lost,
and are capped at 0.15 m/s and 0.4 rad/s. The adapter prefers a discovered
`/<robot>/cmd_vel` `geometry_msgs/msg/Twist` subscriber, then checks
`/<robot>/cmd_vel_stamped` `geometry_msgs/msg/TwistStamped`; no verified
subscriber means no motion command. Commands repeat at 10 Hz with a 250 ms DDS
lifespan. The hardware controller does not infer that a robot is safe to move
from graph discovery.

Light effects call only the selected robot's
`/<robot>/RGB_control/{rainbow_start,rainbow_stop,match_color}` services when
they are discovered as `std_srvs/srv/Trigger`. Solid colors use
`/<robot>/RGB_control/solid_color` with `mir_srvs/srv/ColorRGB`; response or
service errors are shown in the panel. Missing or incompatible services are
reported without trying another namespace. RGB values are clamped to 0–255.
All jog and lighting behavior tests use a fake adapter; no physical MiR was
connected during development.
