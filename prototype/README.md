# QML layout prototype

Run `./prototype/run_layout_prototype.sh` on a Qt 5.15 desktop with Qt Quick Controls 2 installed. This sample-data-only window does not connect to ROS.

The header keeps mode, preset, robot and connection state visible in every layout. The warning points to the affected Bringup log. Select either log tab to inspect its independent sample output. The top buttons compare the three arrangements:

| Variant | Layout | Best fit |
| --- | --- | --- |
| Trial field | Robot state, visualization and logs in one row | Quick status checks and operator focus |
| Development | Large visualization, status alongside, logs below | Iterating on visualization while retaining process output |
| Split work | Visualization and logs share the available width | Following a visualization and a specific process log together |

**Selected direction: Development.** It gives the ROS visualization the most room while keeping both logs accessible in the same window; the Trial field variant remains one click away when status and operation matter more. The warning is pinned above the workspace in all variants, and its wording names the Bringup log directly. The mode, preset and robot controls update the shared session context without changing the selected layout.

The status labels are sample values: Connected and Running describe the session, while Base online and Left arm ready describe robot components. Warning is amber and links to a concrete cause. No action in this prototype controls a robot.
