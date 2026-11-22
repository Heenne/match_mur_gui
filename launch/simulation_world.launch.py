"""Start a Gazebo server without robots and bridge its simulation clock."""

import os
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, EmitEvent, ExecuteProcess, OpaqueFunction
from launch.actions import RegisterEventHandler, SetEnvironmentVariable
from launch.event_handlers import OnProcessExit
from launch.events import Shutdown
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def start_world(context):
    world = Path(LaunchConfiguration("world_file").perform(context))
    occupancy_map = LaunchConfiguration("map_file").perform(context)
    gazebo_share = Path(get_package_share_directory("mir_gazebo"))
    resources = [
        str(world.parent), str(gazebo_share / "worlds"), str(gazebo_share / "worlds" / "include"),
        str(gazebo_share / "models"), os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
    ]
    server = ExecuteProcess(cmd=["gz", "sim", "-s", "-r", str(world)], output="screen")
    actions = [
        SetEnvironmentVariable("GZ_SIM_RESOURCE_PATH", os.pathsep.join(filter(None, resources))),
        RegisterEventHandler(OnProcessExit(
            target_action=server, on_exit=[EmitEvent(event=Shutdown(reason="Gazebo server exited"))],
        )),
        server,
        Node(
            package="ros_gz_bridge", executable="parameter_bridge", name="clock_bridge",
            arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"], output="screen",
        ),
    ]
    if occupancy_map:
        actions.extend([
            Node(
                package="nav2_map_server", executable="map_server", name="map_server",
                parameters=[{"yaml_filename": occupancy_map, "use_sim_time": True}], output="screen",
            ),
            Node(
                package="nav2_lifecycle_manager", executable="lifecycle_manager",
                name="lifecycle_manager_map",
                parameters=[{"autostart": True, "node_names": ["map_server"]}],
                output="screen",
            ),
        ])
    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("world_file"),
        DeclareLaunchArgument("map_file", default_value=""),
        OpaqueFunction(function=start_world),
    ])
