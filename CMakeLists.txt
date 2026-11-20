cmake_minimum_required(VERSION 3.8)
project(match_mur_gui)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(match_mur_gui)

# Runnable native RViz docking prototype (Prototype B in gui_rewrite_spec.md).
set(CMAKE_AUTOMOC ON)
find_package(Qt5 REQUIRED COMPONENTS Widgets QuickWidgets Network)
find_package(rclcpp REQUIRED)
find_package(rosgraph_msgs REQUIRED)
find_package(controller_manager_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rviz_common REQUIRED)
find_package(ament_index_cpp REQUIRED)

add_executable(mur_rviz_docking_prototype
  prototype/rviz_docking_main.cpp
  prototype/ros_session_monitor.cpp
  prototype/process_manager.cpp
)
target_compile_features(mur_rviz_docking_prototype PRIVATE cxx_std_17)
target_link_libraries(mur_rviz_docking_prototype Qt5::Widgets Qt5::QuickWidgets Qt5::Network)
ament_target_dependencies(mur_rviz_docking_prototype ament_index_cpp controller_manager_msgs rclcpp rosgraph_msgs rviz_common std_msgs)
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
  ament_add_pytest_test(test_deb_package test/test_deb_package.py)
endif()

install(DIRECTORY prototype
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
