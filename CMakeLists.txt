cmake_minimum_required(VERSION 3.8)
project(match_mur_gui)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(match_mur_gui)

# Native MuR Command Center with QML views and an RViz dock.
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
find_package(ur_dashboard_msgs REQUIRED)

add_executable(mur_command_center
  command_center/main.cpp
  command_center/app_theme.cpp
  command_center/ui_preferences.cpp
  command_center/ui_preferences.hpp
  command_center/ros_session_monitor.cpp
  command_center/hardware_status.cpp
  command_center/hardware_status.hpp
  command_center/lift_controller.cpp
  command_center/lift_controller.hpp
  command_center/hardware_start.cpp
  command_center/hardware_start.hpp
  command_center/process_manager.cpp
  command_center/log_store.cpp
  command_center/log_panel.cpp
  command_center/simulation_base_jog.cpp
  command_center/simulation_base_jog.hpp
  command_center/mir_hardware_control.cpp
  command_center/mir_hardware_control.hpp
  command_center/goals_poses.cpp
  command_center/goals_poses.hpp
  command_center/mocap_controller.cpp
  command_center/mocap_controller.hpp
  command_center/ur_arm_jog.cpp
  command_center/ur_arm_jog.hpp
  command_center/ur_motion_actions.cpp
  command_center/ur_motion_actions.hpp
  command_center/ur_shutdown.hpp
  command_center/ur_shutdown.cpp
  command_center/ur_shutdown_test.cpp
  command_center/ur_freedrive.cpp
  command_center/ur_freedrive.hpp
)
target_compile_features(mur_command_center PRIVATE cxx_std_17)
target_link_libraries(mur_command_center Qt5::Widgets Qt5::QuickWidgets Qt5::Network)
ament_target_dependencies(mur_command_center action_msgs actionlib_msgs ament_index_cpp controller_manager_msgs ewellix_interfaces geometry_msgs mir_srvs nav2_msgs rcl_interfaces rclcpp rosgraph_msgs rviz_common sensor_msgs std_msgs std_srvs tf2_msgs ur_dashboard_msgs)
install(TARGETS mur_command_center DESTINATION lib/${PROJECT_NAME})

install(PROGRAMS
  scripts/align_arm_to_plane.py
  scripts/set_cartesian_controller.py
  scripts/ensure_ur_ready.py
  scripts/shutdown_urs.py
  scripts/general_mur_gui.py
  scripts/move_arm_to_named_pose.py
  scripts/mur_rviz_docking_prototype
  scripts/ur_dashboard_safety_check.py
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  add_test(NAME sim_base_jog_behavior COMMAND mur_command_center --contract-test-sim-base-jog)
  set_tests_properties(sim_base_jog_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME mir_hardware_control_behavior COMMAND mur_command_center --contract-test-mir-hardware-control)
  set_tests_properties(mir_hardware_control_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME goals_poses_behavior COMMAND mur_command_center --contract-test-goals-poses)
  set_tests_properties(goals_poses_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME mocap_monitor_behavior COMMAND mur_command_center --contract-test-mocap)
  set_tests_properties(mocap_monitor_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME hardware_status_behavior COMMAND mur_command_center --contract-test-hardware-status)
  set_tests_properties(hardware_status_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME lift_controller_behavior COMMAND mur_command_center --contract-test-lift-controller)
  set_tests_properties(lift_controller_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME ur_arm_jog_behavior COMMAND mur_command_center --contract-test-ur-arm-jog)
  set_tests_properties(ur_arm_jog_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME ur_shutdown_behavior COMMAND mur_command_center --contract-test-ur-shutdown)
  set_tests_properties(ur_shutdown_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME ur_freedrive_behavior COMMAND mur_command_center --contract-test-ur-freedrive)
  set_tests_properties(ur_freedrive_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME ur_motion_actions_behavior COMMAND mur_command_center --contract-test-ur-motion-actions)
  set_tests_properties(ur_motion_actions_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME hardware_start_behavior COMMAND mur_command_center --contract-test-hardware-start)
  set_tests_properties(hardware_start_behavior PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME localization_graph COMMAND mur_command_center --contract-test-localization-graph)
  set_tests_properties(localization_graph PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME localization_source_selection COMMAND mur_command_center --contract-test-localization-source)
  set_tests_properties(localization_source_selection PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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
  ament_add_pytest_test(test_command_center_qml test/test_command_center_qml.py)
  ament_add_pytest_test(test_gazebo_client test/test_gazebo_client.py)
  ament_add_pytest_test(test_session_shell test/test_session_shell.py)
  ament_add_pytest_test(test_remote_discovery_config test/test_remote_discovery_config.py)
  ament_add_pytest_test(test_process_manager test/test_process_manager.py)
  ament_add_pytest_test(test_log_panel test/test_log_panel.py)
  ament_add_pytest_test(test_hardware_start test/test_hardware_start.py)
  ament_add_pytest_test(test_mir_hardware_control_ui test/test_mir_hardware_control_ui.py)
  ament_add_pytest_test(test_deb_package test/test_deb_package.py)
endif()

install(DIRECTORY assets/ DESTINATION share/${PROJECT_NAME}/assets)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME} FILES_MATCHING PATTERN "*.launch.py")

install(DIRECTORY command_center/qml
  DESTINATION share/${PROJECT_NAME}/command_center
)

find_program(QT_LRELEASE_EXECUTABLE NAMES lrelease-qt5 lrelease REQUIRED)
set(translation_source "${CMAKE_CURRENT_SOURCE_DIR}/command_center/translations/command_center_de.ts")
set(translation_catalog "${CMAKE_CURRENT_BINARY_DIR}/command_center_de.qm")
add_custom_command(OUTPUT "${translation_catalog}"
  COMMAND "${QT_LRELEASE_EXECUTABLE}" "${translation_source}" -qm "${translation_catalog}"
  DEPENDS "${translation_source}"
  VERBATIM)
add_custom_target(command_center_translations ALL DEPENDS "${translation_catalog}")
add_dependencies(mur_command_center command_center_translations)
install(FILES "${translation_catalog}" DESTINATION share/${PROJECT_NAME}/command_center/translations)

ament_package()
