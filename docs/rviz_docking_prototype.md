# RViz docking prototype

The `mur_rviz_docking_prototype` executable hosts RViz's native
`rviz_common::VisualizationFrame` inside a Qt 5 Widgets `QDockWidget`. RViz
retains its complete display tree, tool bar, view controls, status panel, and
configuration support. The neighboring Logs dock is deliberately simple and
shows that another MuR view can coexist. Float the Visualisierung dock from its
title bar and dock it again to exercise the host relationship.

Build from a sourced ROS 2 Jazzy workspace:

```bash
colcon build --packages-select match_mur_gui --cmake-args -DBUILD_TESTING=ON
source install/setup.bash
ros2 run match_mur_gui mur_rviz_docking_prototype
```

Pass an existing RViz config with `--config /path/to/view.rviz`. The optional
`--context "Hardware · Versuchsfeld · mur620a"` label is shown in the window
title, both dock titles, and status bar. This is a display label only; it does
not change ROS domains, hosts, profiles, or an existing ROS connection. The
process inherits the shell's ROS environment, including `ROS_DOMAIN_ID`.

## Automated non-display checks

`test_rviz_docking_contract.py` checks that help works headlessly, a nonexistent
RViz config is rejected before creating the host window, and unsupported
arguments fail clearly. These CLI checks do not establish that Qt can render or
that input focus survives docking on a desktop.

## Ubuntu 24.04 desktop checks still needed

Use a desktop session with ROS 2 Jazzy and a running graph, then check:

1. Load a known `.rviz` configuration and confirm the display tree, toolbar,
   status messages, and camera/view controls are all present.
2. Load a configuration that references a deliberately unavailable TF frame.
   Confirm RViz shows the missing transform in its status area and on the
   affected display.
3. Use a tool that needs mouse and keyboard input, float the entire
   Visualisierung dock, use the tool, then dock it again and repeat. Confirm
   render output continues, the selected tool and displays remain, and focus
   returns to the intended widget after each transition.
4. Run two disposable ROS contexts with different test frames or topics, switch
   the shell's context and relaunch with a different `--context` label. Confirm
   the host does not claim to switch a live RViz context: this prototype creates
   one ROS node for its lifetime, so a context change requires process restart.
5. Keep the Logs dock visible while RViz is floating and redocked. Close and
   reopen the RViz dock from Qt's dock menu and verify that the same frame and
   display state survive.

Record Qt/ROS versions, desktop session (Wayland or X11), GPU/driver, config,
and any focus or render failure. A successful compile and headless contract
checks are insufficient evidence for the docking decision.

## Architecture decision

This prototype follows the spec's recommended Qt 5 Widgets host and native
`VisualizationFrame` embed. The spec's decision rule is to retain native
embedding if full RViz remains stable while detached and redocked. If desktop
checks expose recurring rendering or focus failures, use separately launched
RViz as the fallback; do not embed a foreign process window or stream pixels.
The current implementation provides a concrete prototype, not a final verdict:
the required desktop checks have not run in this build environment.
