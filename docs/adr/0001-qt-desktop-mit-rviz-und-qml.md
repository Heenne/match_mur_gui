---
status: accepted
---

# Qt-Desktop-App mit RViz und QML

Die MuR-App verwendet C++ und Qt 5 Widgets für das Hauptfenster, die Docks und den vollständigen RViz-`VisualizationFrame`. Damit kann die App die native RViz-Oberfläche direkt verwenden, ohne eine Python-Anbindung für das Widget zu pflegen. Die eigenen Ansichten entstehen in QML, und bestehende Python-ROS-Nodes bleiben über ihre ROS-Schnittstellen nutzbar.

Der technische Prototyp prüft auf Ubuntu 24.04 mit ROS 2 Jazzy, ob das vollständige RViz einschließlich Displaybaum, Werkzeugen und `.rviz`-Konfiguration beim Abdocken und erneuten Andocken benutzbar bleibt. Ein separat gestartetes RViz ist die Rückfalloption, falls der Prototyp bei der Einbettung wiederkehrende Grafik- oder Fokusprobleme zeigt. [RViz Jazzy: VisualizationFrame](https://github.com/ros2/rviz/blob/jazzy/rviz_common/include/rviz_common/visualization_frame.hpp)
