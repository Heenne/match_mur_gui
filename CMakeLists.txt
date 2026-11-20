cmake_minimum_required(VERSION 3.8)
project(match_mur_gui)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(match_mur_gui)

# Runnable native RViz docking prototype (Prototype B in gui_rewrite_spec.md).
set(CMAKE_AUTOMOC ON)
find_package(Qt5 REQUIRED COMPONENTS Widgets QuickWidgets Network)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(mir_srvs REQUIRED)
find_package(rosgraph_msgs REQUIRED)
find_package(controller_manager_msgs REQUIRED)
find_package(ewellix_interfaces REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(tf2_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(actionlib_msgs REQUIRED)
find_package(action_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(rviz_common REQUIRED)
find_package(ament_index_cpp REQUIRED)

add_executable(mur_rviz_docking_prototype
  prototype/rviz_docking_main.cpp
  prototype/ros_session_monitor.cpp
  prototype/hardware_status.cpp
  prototype/hardware_status.hpp
  prototype/lift_controller.cpp
  prototype/lift_controller.hpp
  prototype/process_manager.cpp
  prototype/log_store.cpp
  prototype/log_panel.cpp
  prototype/simulation_base_jog.cpp
  prototype/simulation_base_jog.hpp
  prototype/mir_hardware_control.cpp
  prototype/mir_hardware_control.hpp
  prototype/goals_poses.cpp
  prototype/goals_poses.hpp
  prototype/mocap_controller.cpp
  prototype/mocap_controller.hpp
)
target_compile_features(mur_rviz_docking_prototype PRIVATE cxx_std_17)
target_link_libraries(mur_rviz_docking_prototype Qt5::Widgets Qt5::QuickWidgets Qt5::Network)
ament_target_dependencies(mur_rviz_docking_prototype action_msgs actionlib_msgs ament_index_cpp controller_manager_msgs ewellix_interfaces geometry_msgs mir_srvs nav2_msgs rcl_interfaces rclcpp rosgraph_msgs rviz_common sensor_msgs std_msgs std_srvs tf2_msgs)
install(TARGETS mur_rviz_docking_prototype DESTINATION lib/${PROJECT_NAME})

install(PROGRAMS
  scripts/align_arm_to_plane.py
  scripts/set_cartesian_controller.py
  scripts/ensure_ur_ready.py
  scripts/shutdown_urs.py
  scripts/general_mur_gui.py
  scripts/move_arm_to_named_pose.py
  scripts/ur_dashboard_safety_check.py
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  add_test(NAME sim_base_jog_behavior COMMAND mur_rviz_docking_prototype --contract-test-sim-base-jog)
  set_tests_properties(sim_base_jog_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME mir_hardware_control_behavior COMMAND mur_rviz_docking_prototype --contract-test-mir-hardware-control)
  set_tests_properties(mir_hardware_control_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME goals_poses_behavior COMMAND mur_rviz_docking_prototype --contract-test-goals-poses)
  set_tests_properties(goals_poses_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME mocap_monitor_behavior COMMAND mur_rviz_docking_prototype --contract-test-mocap)
  set_tests_properties(mocap_monitor_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME hardware_status_behavior COMMAND mur_rviz_docking_prototype --contract-test-hardware-status)
  set_tests_properties(hardware_status_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME lift_controller_behavior COMMAND mur_rviz_docking_prototype --contract-test-lift-controller)
  set_tests_properties(lift_controller_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_cartesian_startup test/test_cartesian_startup.py)
  ament_add_pytest_test(test_cartesian_jog test/test_cartesian_jog.py)
  ament_add_pytest_test(test_home_moveit_client test/test_home_moveit_client.py)
  ament_add_pytest_test(test_alignment test/test_alignment.py)
  ament_add_pytest_test(test_desktop_installer test/test_desktop_installer.py)
  ament_add_pytest_test(test_app_icon test/test_app_icon.py)
  ament_add_pytest_test(test_gui_layout test/test_gui_layout.py)
  ament_add_pytest_test(test_ur_shutdown test/test_ur_shutdown.py)
  ament_add_pytest_test(test_ros_discovery test/test_ros_discovery.py)
  ament_add_pytest_test(test_mir_jog test/test_mir_jog.py)
  ament_add_pytest_test(test_localization_selection test/test_localization_selection.py)
  ament_add_pytest_test(test_rviz_docking_contract test/test_rviz_docking_contract.py)
  ament_add_pytest_test(test_qml_layout_prototype test/test_qml_layout_prototype.py)
  ament_add_pytest_test(test_gazebo_client test/test_gazebo_client.py)
  ament_add_pytest_test(test_session_shell test/test_session_shell.py)
  ament_add_pytest_test(test_remote_discovery_config test/test_remote_discovery_config.py)
  ament_add_pytest_test(test_process_manager test/test_process_manager.py)
  ament_add_pytest_test(test_log_panel test/test_log_panel.py)
  ament_add_pytest_test(test_mir_hardware_control_ui test/test_mir_hardware_control_ui.py)
  ament_add_pytest_test(test_deb_package test/test_deb_package.py)
endif()

install(DIRECTORY prototype
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
