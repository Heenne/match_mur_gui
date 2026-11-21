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
initial title. The GUI supports keys 1–8 to select Overview, Visualization,
Robot, Processes, Diagnosis, Logs, Settings, and Mocap.

The host remains a prototype; connecting confirms that its RViz node started,
but does not prove reachability of every remote ROS endpoint. Full simulation
and hardware launch flows are not yet wired to the shell.

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

In Hardware mode the Robot view also exposes a selected UR10_l or UR10_r jog
panel. Before arming, the app checks that `external_control.urp` is reported
running by the selected arm's dashboard, that the robot is in remote control,
robot/safety modes and the Cartesian
controller are current, at least six selected-arm joint positions are fresh,
and a `TwistStamped` receiver exists at the integrated Cartesian controller's
command topic. The reverse-interface indicator is explicitly marked inferred:
the ROS driver exposes no direct reverse TCP connection status, so it is
derived from the running external-control program and fresh RUNNING robot and
safety status. Unknown or stale checks block arming and jogging.

Jog requires explicit arming for one selected arm. The six translation/rotation
directions are hold-to-run, capped at 0.05 m/s and 0.20 rad/s, and send
`TwistStamped` commands with a 250 ms DDS lifespan. Release, loss of focus,
readiness, session context, or connection sends a zero command where the
verified endpoint remains available and otherwise lets the short-lived command
expire. The red **STOP** area is limited to the selected arm. This view does
not switch controllers or start the UR driver; the integrated Cartesian
controller must already be active. The app does not replace the robot's
safety-rated stop or teach pendant.

The Processes view starts the local MuR620 simulation and manages the Gazebo
world view as a separate `gz sim -g` client. Starting or stopping the client
does not stop the simulation server, `/clock`, ROS bridge, or their logs. The
client becomes available when the Gazebo Transport service `/gazebo/worlds` is
discovered in the local partition. This works independently of the app's ROS
connection and RViz configuration. When auto-open is enabled, starting the
simulation waits up to 60 seconds for this service. Client launch output and
failure state appear in Processes.

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

## Hardware start prototype

In Hardware mode, the Robot view provides a profile-persistent component and
launch-options panel. **Run preflight** explicitly checks the selected host and
the dashboard safety/remote-control state of each selected UR arm. A missing
or unknown response blocks launch just like a reported failure; the GUI never
clears a safety stop. Re-run preflight after changing the target, robot,
connection, selected arms, or remote workspace.

**Start selected hardware** is enabled only after every required check is
confirmed. The app runs the existing hardware launch script with the selected
arm, MiR, camera, controller, FT, collision, and MoveIt options. It sets
`CLEAN_START=false` so the script will not kill pre-existing robot processes,
and disables its implicit build. The remote workspace defaults to
`$HOME/colcon_ws` on the target account; it can be changed per preset. The
remote run is tracked as an app-owned process, with output in Processes and a
scoped Stop action.

Source synchronization and workspace build are separate actions. Sync requires
the preset's local workspace mapping, confirms the source and destination in a
dialog, excludes build/install/log/`.git`, and preserves remote-only files. It
does not use `rsync --delete`. Build runs only when **Build workspace** is
pressed. Both actions report completion or failure in the panel and Logs.
Hardware launch and these operations use SSH non-interactively; configure SSH
keys or an agent yourself. Tests use injected adapters and a fake SSH executable;
they do not connect to or start physical hardware.

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

### Lift controls

The Robot view selects the left or right hardware lift and displays its current height, age, step and configured limits. Motion requires a fresh Ewellix state and a compatible command subscriber; Simulation does not fall back to hardware topics. The adapter uses the MuR hardware launch default conversion of 3225 ticks per metre and the 0.005–0.260 m range. Installations using different lift calibration or limits require a matching adapter/profile before use. A sent target is not a hardware acknowledgement. Holding a direction repeats steps; releasing stops further targets. The Stop button or loss of application focus while holding requests a hold at the latest fresh height, but cannot guarantee a physical stop. Validate the adapter with the intended hardware separately.
