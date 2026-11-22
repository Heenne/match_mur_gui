"""Spawn one independently namespaced MiR or MuR620 in the selected Gazebo world."""

from pathlib import Path
from tempfile import TemporaryDirectory

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import xacro
import yaml


def mir_nodes(robot, world, pose, directory):
    share = Path(get_package_share_directory("mir_description"))
    source = yaml.safe_load((share / "config" / "mur_controllers.yaml").read_text())
    controllers = ("joint_state_broadcaster", "mobile_base_controller")
    manager = source["controller_manager"]["ros__parameters"]
    mobile = source["mobile_base_controller"]["ros__parameters"]
    mobile.update(
        odom_frame_id=f"{robot}/odom", base_frame_id=f"{robot}/base_footprint",
        tf_frame_prefix_enable=False, tf_frame_prefix="",
    )
    config = Path(directory.name) / "controllers.yaml"
    config.write_text(yaml.safe_dump({
        f"/{robot}/controller_manager": {"ros__parameters": {
            "update_rate": manager["update_rate"],
            **{name: manager[name] for name in controllers},
        }},
        f"/{robot}/gz_ros_control": source["gz_ros_control"],
        f"/{robot}/mobile_base_controller": source["mobile_base_controller"],
    }))
    description = xacro.process_file(
        str(share / "urdf" / "mir_600" / "mir_600.gazebo.urdf"),
        mappings={"robot_namespace": robot, "tf_prefix": robot, "simulation_controllers": str(config)},
    ).toxml()
    spawn = Node(
        package="ros_gz_sim", executable="create", namespace=robot,
        arguments=[
            "-world", world, "-name", robot, "-allow_renaming", "false", "-string", description,
            "-x", pose["x"], "-y", pose["y"], "-z", pose["z"], "-Y", pose["Y"],
        ],
        output="screen",
    )
    spawner = Node(
        package="controller_manager", executable="spawner", namespace=robot,
        arguments=[*controllers, "-c", f"/{robot}/controller_manager", "-p", str(config),
                   "--activate-as-group", "--controller-manager-timeout", "90",
                   "--service-call-timeout", "45", "--switch-timeout", "30"],
        output="screen",
    )

    def after_spawn(event, context):
        if context.is_shutdown:
            return []
        if event.returncode != 0:
            raise RuntimeError(f"Gazebo could not spawn {robot}")
        return [spawner]

    def after_spawner(event, context):
        if event.returncode != 0 and not context.is_shutdown:
            raise RuntimeError(f"Could not activate the MiR controllers for {robot}")
        return []

    return [
        Node(
            package="robot_state_publisher", executable="robot_state_publisher", namespace=robot,
            parameters=[{"robot_description": description, "frame_prefix": f"{robot}/", "use_sim_time": True}],
            output="screen",
        ),
        Node(
            package="ros_gz_bridge", executable="parameter_bridge", namespace=robot,
            arguments=[f"/{robot}/{scan}@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan" for scan in ("f_scan", "b_scan")],
            output="screen",
        ),
        RegisterEventHandler(OnProcessExit(target_action=spawn, on_exit=after_spawn)),
        RegisterEventHandler(OnProcessExit(target_action=spawner, on_exit=after_spawner)),
        spawn,
    ]


def generate_launch_description():
    directory = TemporaryDirectory(prefix="mur-command-center-robot-")

    def spawn_robot(context):
        robot, model, world, world_file = (
            LaunchConfiguration(name).perform(context)
            for name in ("robot", "model", "world", "world_file")
        )
        if robot not in ("mur620a", "mur620b", "mur620c", "mur620d"):
            raise ValueError("Choose a robot from mur620a through mur620d")
        pose = {key: LaunchConfiguration(key).perform(context) for key in ("x", "y", "z", "Y")}
        if model == "mir":
            return mir_nodes(robot, world, pose, directory)
        if model != "mur620":
            raise ValueError("Choose either mir or mur620")
        # The upstream launch derives its internal Gazebo world name from a .world file.
        world_reference = Path(directory.name) / "selected.world"
        world_reference.symlink_to(world_file)
        full_launch = Path(get_package_share_directory("mur_launch_sim")) / "launch" / "mur620.launch.py"
        return [IncludeLaunchDescription(
            PythonLaunchDescriptionSource(str(full_launch)),
            launch_arguments={
                **pose, "robot_name": robot, "world": str(world_reference.with_suffix("")),
                "include_gz": "false", "launch_rviz": "false", "launch_moveit": "true",
                "publish_global_moveit_descriptions": "false", "publish_tf_alias": "false",
            }.items(),
        )]

    return LaunchDescription([
        DeclareLaunchArgument("robot"), DeclareLaunchArgument("model"),
        DeclareLaunchArgument("world"), DeclareLaunchArgument("world_file"),
        DeclareLaunchArgument("x"), DeclareLaunchArgument("y"),
        DeclareLaunchArgument("z", default_value="0.07"),
        DeclareLaunchArgument("Y", default_value="0.0"),
        OpaqueFunction(function=spawn_robot),
    ])
