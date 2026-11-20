# Gazebo world view prototype

The General MuR GUI now has an **Open Gazebo World View** toggle. It launches
`gz sim -g`, Gazebo Sim's GUI-only mode, and tracks that one GUI process as
`gazebo_client`. Toggling it off or using **Stop Managed Processes** terminates
only that GUI-owned process. Closing the MuR GUI follows the same managed
process cleanup path. It does not send a Gazebo server shutdown request or use
process-name cleanup for Gazebo.

Gazebo is therefore expected to discover and attach to a server already
advertising on the same Gazebo Transport partition. This prototype does not
start a server, select a world, or configure `GZ_PARTITION`; both processes must use the same partition. The default partition works when
neither launch overrides it. If no compatible server is
reachable, the Gazebo client reports its own connection/loading failure. The
GUI currently exposes that output in its existing process log.

## Local verification

This machine has `/opt/ros/jazzy` and Gazebo Sim 8.15.0 (`gz sim --versions`),
the Harmonic generation. The CLI lists `-g` as “Run only the GUI” and `-s` as
“Run only the server”. The command construction test verifies the client is
started with `-g` and does not include a world argument or server mode.

There was no active Gazebo simulation. An isolated server startup was attempted
with a unique `GZ_PARTITION` and a temporary empty world, but Gazebo exited
before advertising services with `error in getifaddrs: Unknown error -1` in
this sandbox. Although display environment variables are present, this blocked
server startup means attachment and GUI rendering could not be exercised here.
The command test does not establish whether the GUI can render every world or
sensor configuration while attached. Validate those cases on the operator
workstation with the real simulation profile and its intended `GZ_PARTITION`.
