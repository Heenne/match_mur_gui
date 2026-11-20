# General MuR GUI – Bestandsaufnahme und Funktionsspezifikation

Diese Datei beschreibt das Verhalten des bestehenden GUI-Tools als Ausgangspunkt für eine Neuentwicklung. Sie ist aus dem Quellcode von `match_mur_gui` abgeleitet, nicht aus einer separaten Produktanforderung.

## Geltungsbereich

Im Mittelpunkt steht `GeneralMuRGui` aus `match_mur_gui`, gestartet über `ros2 run match_mur_gui general_mur_gui`. Das Fenster wird von `MurBaseGui` bereitgestellt. Es enthält Roboter- und Armsteuerung, MiR-Bedienung, Host-/Hardwarestart und Laufzeitprotokolle.

Das Basisfenster unterstützt Erweiterungsmodule über `MurGuiModule`. Das optionale Paket `match_mocap_gui` ergänzt dasselbe Fenster um Mocap-Status, Karte und Bridge-Steuerung; die separate Mocap-Oberfläche startet maximiert und ist in der README des Pakets beschrieben. Andere Anwendungen wie eine Navigations-GUI sind eigenständige Programme und nicht Teil des hier beschriebenen Basisfensters.

## Ziel und Nutzeraufgaben

Die GUI ist eine Bedienoberfläche für MuR620-Hardware und dafür eingerichtete Operator-Workspaces. Sie ermöglicht:

- SSH-Verbindung und optionales Synchronisieren des lokalen `src/`-Verzeichnisses auf ausgewählte Roboterrechner.
- Host-Prüfungen und Start des Hardware-Launches mit UR-Sicherheits-Preflight.
- Anzeige von Batterie-, Arm- und UR-Reverse-Interface-Status.
- Joggen der MiR-Basis und der UR-Arme, Verstellen der Lifte, Freedrive, Home und TCP-Ausrichtung.
- Setzen und Speichern von MiR-Zielposen sowie Steuern der MiR-Leuchten.
- Starten von RViz, Stoppen verwalteter Prozesse und Sichern/Filtern der GUI-Protokolle.

## Fenster und Informationshierarchie

Das Hauptfenster heißt standardmäßig **General MuR GUI**, startet mit einer Größe von 1180 × 780 px und enthält drei obere Gruppen, Aktionsgruppen und eine Log-Konsole.

### Roboter-Auswahl und Verbindung

| Anzeige/Eingabe | Verhalten und Standard |
|---|---|
| `mur620a`, `mur620b`, `mur620c`, `mur620d` | Auswahl der Roboter für SSH-Aktionen; initial ist nur `mur620d` gewählt. Ist keine Checkbox gewählt, fällt die Auswahl logisch auf `mur620d` zurück. |
| MiR- und MuR-Batterie-Badges | Je Roboter zwei Anzeigen. Grau/`--%` bedeutet keine gültigen Daten; Farben: grün ab 50 %, gelb ab 30 %, orange ab 15 %, rot darunter. Tooltip zeigt Prozentwert bzw. „no data“. |
| `Sync Code` | Standard an. Bei `Connect` wird lokales `WS/src/` per rsync auf `<Remote WS>/src/` kopiert. Build-, Install-, Log-, Cache- und `.git`-Ordner werden ausgeschlossen; rsync nutzt `--delete`. |
| `Remote WS` | Pfad des Colcon-Workspaces auf dem Zielrechner; Standard derzeit `/home/rosmatch/colcon_ws`, durch `REMOTE_WS` überschreibbar. |
| `UR10_r`, `UR10_l` | Bestimmt, welche UR-Arme bei Hostprüfung/Hardwarestart und Freedrive berücksichtigt werden. Beide standardmäßig an. |
| `mir` | Standard aus. Bei Hardwarestart setzt die Auswahl `launch_mir:=true` und startet den MiR-Treiber/Bridge-Pfad. |

Der Button `Connect` ändert seinen Text/Farbzustand abhängig von den ausgewählten Robotern: laufend (`Connect...`, gelb), alle erfolgreich (`Connected`, grün), Warnung (`Check Host`, gelb), Fehler (`Connect Failed`, rot) oder untätig (`Connect`). Ein Tooltip enthält pro Roboter Status und Detailmeldung.

### Launch-Optionen

| Option | Standard | Wirkung beim Hardwarestart |
|---|---:|---|
| `Build before launch` | an | Setzt `BUILD_BEFORE_LAUNCH=true`; ansonsten false. |
| `Integrated controller` | an | Setzt Integrated-Cartesian-Admittance-Modus; aus wählt den Fallback `forward_velocity_controller`. Bei an werden Velocity-Safety- und J-PARSE-Launches abgeschaltet. |
| `Use FT sensor` | an | Setzt `INTEGRATED_CARTESIAN_USE_FT`. |
| `Require wrench` | aus | Setzt `INTEGRATED_CARTESIAN_REQUIRE_WRENCH`. |
| `Collision avoidance` | an | Übergibt `integrated_controller_enable_collision_avoidance`. |
| `Collision markers` | aus | Übergibt `integrated_controller_publish_collision_markers`. |
| `Zero admittance` | aus | Bei an werden Admittanz- und Wrench-Twist-Gain-Vektoren auf sechs Nullen gesetzt. |
| `Launch MoveIt` | an | Übergibt `launch_moveit`; beeinflusst auch `MOVEIT_WITH_INTEGRATED_CARTESIAN`. |
| `MoveIt speed` | 20 % | Slider 1–100 %, wird als Skalierung für Home-Bewegungen verwendet. |

Der Hardwarestart setzt außerdem BMS-Start mit `can0`/250000, automatisches Umschalten der MoveIt-Controller und deaktiviert MoveIt-RViz im Hardware-Launch. Der explizite `Open RViz`-Button startet RViz separat.

### Arm-Status

Für jeden Roboter und jede Seite (`UR10_r`, `UR10_l`) zeigt die Statusgruppe zunächst `unknown | UR reverse missing`. Der linke Teil ist der Arm-/Preflight-Status, der rechte Teil kennzeichnet, ob die UR-Reverse-Schnittstelle bereit ist. Meldungen aus dem Hardwarelog setzen Reverse-Status auf bereit/zurückgefallen; der `Enable URs / Ready`-Ablauf prüft und wartet zusätzlich auf stabilen Status.

### Aktionsgruppen im Hauptfenster

| Gruppe | Aktion | Verhalten |
|---|---|---|
| General | `Connect` | SSH, Remote-Workspace anlegen, optional rsync und Host-Setup-Check. |
| General | `Check Host` | Führt das Diagnose-Preflight-Skript am Zielrechner aus und zeigt Ausgabe im Log. |
| General | `Start Hardware` | Module benachrichtigen, Host-Preflight, UR-Netz-/Dashboard-Sicherheitsprüfung, danach Hardware-Launch pro ausgewähltem Roboter. |
| General | `Open RViz` | Startet lokales RViz mit der MuR620d-MoveIt-Konfiguration; MoveIt soll bereits laufen. Setzt `use_sim_time=false`. |
| General | `Save GUI Log` | Schreibt eine Momentaufnahme des aktuell gefilterten sichtbaren Terminaltexts. |
| General | `Stop Managed Processes` | Stoppt lokale verwaltete Prozesse und per SSH bekannte Home-/Alignment-Prozesse der ausgewählten Roboter. |
| MiR | `MiR Jog` | Öffnet Basis-Jogdialog. |
| MiR | `MiR Lights` | Öffnet Lichteffekt-/Farbdialog. |
| MiR | `MiR Goals` | Öffnet Zielkoordinaten- und gespeicherte-Posen-Dialog. |
| UR | `Enable URs / Ready` | Führt `ensure_ur_ready.py` für ausgewählte Arme aus, wartet auf Reverse-Interface; maximal ein automatischer Retry mit erzwungenem Neustart. |
| UR | `Home L`, `Home R` | Beendet vorherige Armbewegungen, deaktiviert Freedrive bei Bedarf und startet MoveIt-Bewegung zur benannten Pose `Home_custom`. |
| UR | `Jog L`, `Jog R` | Öffnet UR-/Lift-Jogdialog, falls Reverse-Interface bereit. Andernfalls wird zunächst UR-Ready geprüft. |
| UR | `Freedrive` | Schaltet Freedrive für ausgewählte Roboter/Arme um; wechselt Controller und hält den Freedrive-Enable-Befehl mit 10 Hz aktiv. |
| UR | `Align` | Öffnet TCP-Ausrichtungsdialog. |
| UR | `STOP ARM MOTION` | Rote Not-Stopp-Aktion der GUI: stoppt Jogdialogs, Module und Motion-Prozesse, cancelt Arm-Actions und veröffentlicht wiederholt Null-Twists für beide Arme aller ausgewählten Roboter. Dies ist kein hardwareseitiger Not-Aus. |

## Unterdialoge

### UR- und Lift-Jog

Titel: `Jog UR10_{side} + right/left lift`. Der Dialog enthält Roboter-Auswahl, Modus und Ready-Status, Geschwindigkeitswerte, Cartesian-Jog-Tastenfeld sowie Lift-Regler.

- Translation ist der Startmodus. `M` schaltet zwischen Translation und Rotation um.
- Linear: 0.001–0.2 m/s, Standard 0.01 m/s. Angular: 0.01–1.0 rad/s, Standard 0.1 rad/s.
- Gedrückthalten von X/Y/Z- bzw. Rotations-Tasten veröffentlicht alle 50 ms einen `TwistStamped`; Loslassen stoppt die Achse. Tastatur: Pfeile für X/Y, PgUp/PgDn für Z, Space oder `.` zum Stoppen.
- Die Armsteuerung ist deaktiviert, solange das UR-Reverse-Interface nicht bereit ist. `STOP` stoppt den Jog.
- Lift-Anzeige zeigt Höhe aus Joint States oder `-- m`. Step ist 0.001–0.05 m (Standard 0.01), Min/Max standardmäßig 0.005/0.260 m. `Lift +`/`Lift -` sendet sofort und wiederholt alle 250 ms; Zielwerte werden auf Min/Max begrenzt. Ohne Joint-State-Höhe oder an einer Grenze wird nicht weiter verfahren.
- Schließen stoppt Arm- und Lift-Jog.

### TCP-Ausrichtung (`Align TCP to Plane`)

- Roboter-Auswahl aus aktuell ausgewählten Robotern, Arm-Auswahl links/rechts; Standardarm richtet sich nach Auswahl, sonst rechts.
- Winkelgeschwindigkeit 0.03–0.30 rad/s, Standard 0.10.
- Drei grafische Vorschauen für XY-, XZ- und YZ-Ebene. Je Richtung wird eine Ausrichtung angeboten; Ausrichtung erhält die TCP-Position und richtet die Werkzeug-Z-Achse gemäß der ausgewählten Ebenenorientierung aus.
- Die sechs Aktionen sind: XY (+Z-Seite, TCP-Z → −Z; −Z-Seite, TCP-Z → +Z), XZ (+Y-Seite, TCP-Z → −Y; −Y-Seite, TCP-Z → +Y), YZ (+X-Seite, TCP-Z → −X; −X-Seite, TCP-Z → +X). Bezugsframe wird als `<robot>/<UR10_side>/base_link` angezeigt.
- Status: Ready, laufend, abgeschlossen, gestoppt oder Fehler. `Stop` löst den gemeinsamen Arm-Stop aus; `Close` schließt den Dialog.
- Ausführung erfolgt remote über `align_arm_to_plane.py`, mit berechnetem Timeout. Vorher werden Jog, Actions und konkurrierende Bewegungsprozesse beendet.

### MiR Jog

- MiR-Namespace-Auswahl (editierbar) und `Refresh` zur ROS-Graph-Erkennung.
- Linear 0.01–0.8 m/s (Standard 0.10); Winkel 0.05–1.5 rad/s (Standard 0.30).
- Vorwärts, Rückwärts, links/rechts drehen und `STOP`. Gedrückthalten bzw. Pfeiltasten steuern; Space stoppt. 50-ms-Publish-Timer; Stop veröffentlicht drei Null-Kommandos. Schließen stoppt.
- Topic-Wahl: `/<namespace>/cmd_vel` als `Twist`, wenn dort Abonnenten entdeckt werden; andernfalls `/<namespace>/cmd_vel_stamped` als `TwistStamped` mit Frame `/<namespace>/base_link`.

### MiR Lights

- Namespace-Auswahl mit Refresh.
- `Rainbow Start`, `Rainbow Stop` und `MATCH Color` rufen Trigger-Services auf.
- `Choose Color` öffnet den Farbwähler; Vorschau startet mit RGB (0, 180, 255). `Apply Solid` sendet RGB-Werte an den Solid-Color-Service.

### MiR Goals

- Namespace-Auswahl und Refresh; Frame editierbar (Standard `map`); Pose-Name editierbar.
- X/Y in Metern von −1000 bis 1000, Standard 0, Schritt 0.01, drei Dezimalstellen. Yaw −180° bis 180°, Standard 0°, Schritt 1°, eine Dezimalstelle.
- `Drive Coordinates` veröffentlicht das eingegebene Ziel.
- `Save Coordinates` legt die Eingabe als benannte Pose ab bzw. aktualisiert einen gleichnamigen Eintrag.
- `Save Current Robot Pose` liest `robot_pose` und speichert die aktuelle Position; bei fehlender Pose wird protokolliert.
- Liste gespeicherter Posen: Auswahl lädt Werte in die Felder; `Drive Selected`, `Delete Selected`, `Reload` bedienen die Auswahl.
- JSON-Datei ist standardmäßig `WS/src/match_mur_gui/config/mir_poses.json`, überschreibbar über `MIR_GUI_POSES_FILE`. Akzeptiert ein JSON-Array oder ein Objekt mit `poses`-Array.

### UR-Sicherheitsdialoge

Wenn das Dashboard-Preflight einen Safety-/Protective-Stop erkennt, hält der Hardwarestart an und zeigt pro UR Host-Erreichbarkeit, Blockierung, Banner und Dashboard-Abfragen. Optionen: `Start abbrechen` oder `Fehler quittieren und starten`. Quittieren führt Clear-Kommandos aus und startet nur bei Exitcode 0. Wenn der Preflight selbst nicht lesbar/erfolgreich ist, erscheint ein Warnungsdialog mit Details und den Optionen `Trotzdem starten` oder `Start abbrechen`; Standard ist Abbrechen.

## ROS-Schnittstellen

Die GUI betreibt einen ROS-Worker-Thread mit eigenem `rclpy`-Node `mur_gui`. Das Fenster setzt standardmäßig ROS Domain 62 für Shell-Aufrufe; der ROS-Worker erbt die Prozessumgebung.

| Zweck | Schnittstelle | Typ/Verhalten |
|---|---|---|
| Gemeinsame Gelenkzustände | `/joint_states` | `sensor_msgs/JointState`; Positionswerte werden für Lift-Anzeige gelesen. |
| MuR-Batterie | `/<robot>/bms_status/SOC` | `std_msgs/Float32`, Prozentwert. |
| MiR-Batterie | `/<robot>/battery_state` | `sensor_msgs/BatteryState`, `percentage` als 0–1 skaliert auf Prozent. |
| MiR Basissteuerung | `/<ns>/cmd_vel`, sonst `/<ns>/cmd_vel_stamped` | `Twist` oder `TwistStamped`, Auswahl per Abonnenten-Erkennung. |
| MiR Pose | `/<ns>/robot_pose` | `geometry_msgs/Pose`; aktuelle Pose für „Save Current Robot Pose“. |
| MiR Ziel | `/<ns>/move_base_simple/goal` | `geometry_msgs/PoseStamped`. |
| MiR Leuchten | `/<ns>/RGB_control/{rainbow_start,rainbow_stop,match_color,solid_color}` | Drei `std_srvs/Trigger`-Services und `mir_srvs/ColorRGB` für Solid Color. |
| UR Cartesian Jog | `/<robot>/<UR10_side>/integrated_cartesian_admittance_controller/equilibrium_twist_cmd` | `geometry_msgs/TwistStamped`, Frame `<UR10_side>/base_link`. |
| Armbewegung stoppen | Entdeckte `.../_action/cancel_goal`-Services unter Roboter-Namespace | `action_msgs/CancelGoal`; zusätzlich Null-Twists. |
| Freedrive | `/<robot>/<UR10_side>/freedrive_mode_controller/enable_freedrive_mode` | `std_msgs/Bool`; Status über Controller `transition_event`; Controller-Manager-Dienste unter `/<robot>/<UR10_side>/controller_manager`. |
| Lift | `/<robot>/ewellix_lift_{l|r}/command` | `ewellix_interfaces/Command`, sendet Meterziel. |

MiR-Namespaces werden aus ROS-Graph-Namen mit bekannten Suffixen für cmd_vel, Pose, Scans und Lichtservices abgeleitet. Falls keine Erkennung verfügbar ist, greifen Namespace-/Robot-Auswahl und `MIR_GUI_NAMESPACE`.

## Prozesse, Konfiguration und Dateisystem

- Lokale Shell-Kommandos laufen über `bash -lc`; SSH verwendet `BatchMode=yes`. Remote-ROS-Kommandos sourcen `/opt/ros/jazzy/setup.bash` sowie `<Remote WS>/install/setup.bash`, setzen standardmäßig Domain 62 und deaktivieren den ROS-CLI-Daemon.
- `WS` ist der lokale Workspace, `REMOTE_WS` der Remote-Workspace. Im Code haben beide derzeit den Fallback `/home/rosmatch/colcon_ws`.
- Der Hardwarestart nutzt remote `src/match_mobile_robotics_jazzy/start_mur620_hardware_logged.sh`, Host-Setup-/Diagnoseskripte und `src/match_mur_gui/scripts/ur_dashboard_safety_check.py`.
- GUI-Logs werden unter `WS/src/match_mur_gui/logs/gui/` angelegt; dort entsteht auch `latest.log`. Hardwarelog-Auswertung referenziert `WS/src/match_mobile_robotics_jazzy/logs/hardware/latest.log`.
- Startzeitpunkt, Prozessein-/ausgabe und GUI-Meldungen werden in die Logdatei geschrieben. Terminalansicht ist schreibgeschützt, ohne Zeilenumbruch und folgt neuen Meldungen automatisch.
- Logfilter `Errors`, `Warnings`, `Infos` starten alle aktiviert. Filter steuern die Anzeige und die Snapshot-Datei; die laufende Logdatei bekommt ungefilterte Meldungen. Einstufung beruht auf Textmustern für Fehler- und Warnbegriffe.
- Beim Schließen werden verwaltete Prozesse beendet, Modul-Shutdown-Hooks ausgeführt und der ROS-Worker heruntergefahren.

### Aktuelle Portabilitätsrisiken

Die GUI erwartet ein bestimmtes Workspace-/Hardware-Setup. Insbesondere sind `/home/rosmatch/colcon_ws` als Fallback für `WS` und `REMOTE_WS` im Quellcode fest eingetragen. Der GUI-Logpfad liegt zudem im Source-Tree. Wenn der Benutzer `WS` nicht korrekt setzt oder das Source-Verzeichnis nicht beschreibbar ist, kann bereits das Erzeugen der GUI-Logdatei beim Start mit `PermissionError` fehlschlagen. Die Neufassung sollte Workspace-Erkennung, Benutzerdaten-/Logpfade und Remote-Workspace-Konfiguration als getrennte, sichtbare Konfiguration behandeln.

Auch die UR-/Hardwareaktionen sind spezifisch: feste Roboternamen, UR10 links/rechts, ein erwarteter Reverse-IP-Wert `192.168.12.69` für `mur620d`, festgelegte Launchpakete und benannte MoveIt-Pose `Home_custom`. `Open RViz` lädt eine MuR620d-Hardwarekonfiguration auch dann, wenn ein anderer Roboter ausgewählt ist.

## Simulationstauglichkeit und bekannte Integrationsgrenzen

Die Oberfläche kann einzelne ROS-Kommandos grundsätzlich auch bei laufender Simulation ausführen, ist im aktuellen Stand aber auf Hardwarebedienung ausgelegt:

- `Start Hardware`, SSH, rsync, UR-Dashboard-Sicherheit, `ensure_ur_ready.py`, Home und Alignment bedienen einen Remote-Hardware-Workspace und sind kein Simulationsstart.
- Die MiR-Jog-Dialoge erwarten `/<ns>/cmd_vel` oder `/<ns>/cmd_vel_stamped`; die Simulation verwendet bei `diff_drive_controller` typischerweise `/<robot>/mobile_base_controller/cmd_vel` mit `TwistStamped`. Dieser Topic-Pfad wird von der GUI nicht gesucht.
- Der GUI-UR-Jog erwartet den Hardware-Controller `integrated_cartesian_admittance_controller` unter `/<robot>/UR10_{l|r}/...`. Die Simulationscontroller und Controller-Manager-Namespaces sind anders angeordnet; Freedrive, Home und Arm-Jog sind daher nicht ohne Anpassung kompatibel.
- MiR-Ziel/Pose-/Licht-Topics setzen die MiR-Bridge-Konventionen voraus und sind nicht automatisch mit simulierten Nav2-/Gazebo-Schnittstellen gleichzusetzen.
- `Open RViz` verwendet Hardwarekonfiguration und `use_sim_time=false`.
- Globales `/joint_states` kann mit einem Simulator kompatibel sein, wenn dieser das Topic global publiziert; das allein macht Lift- oder Armsteuerung noch nicht kompatibel.

Für die Neufassung sollte ein explizites Betriebsprofil (Hardware/Simulation) die Topic-Namen, Controller-Manager, Sim-Zeit, Bewegungsaktionen und Launch-/Stop-Strategie bereitstellen. Simulationsfunktionen sollten gegen tatsächlich gestartete ROS-Graph-Endpunkte geprüft werden.

## Erweiterung: MuR Mocap GUI

Das Paket `match_mocap_gui` erstellt `MurBaseGui(modules=[MocapGuiModule()])`. Es ergänzt:

- Aktionsgruppe `Mocap`: `Start Mocap`, `Stop Mocap`.
- Statuszeile für den GUI-gesteuerten Driver.
- Checkbox `Map-Posen und Roboter-TF aus Qualisys publishen`, standardmäßig an.
- Tabelle pro ausgewähltem Roboter: Pose-Status, Raw-Hz, geglättetes x/y/yaw und Map-Status. Monitoring alle 0.2 s; Pose gilt nach 0.5 s als veraltet.
- Startet lokal `match_mocap_ros2.qualisys_ssh_bridge` mit 100-Hz-Eingang; Stop betrifft nur den GUI-gesteuerten Bridge-Prozess. Hardwarestart startet den Bridge bei aktivierter Map-Ausgabe automatisch, sofern noch kein Map-Stream gesehen wird.
- Das Paket-README dokumentiert `/qualisys/<robot>/pose`, `/pose_smoothed`, optionale `/qualisys_map/...`-Posen und TF-Verhalten.

## Quellcode-Inventar

| Bereich | Datei/Klasse |
|---|---|
| Hauptfenster und ROS-Anbindung | `match_mur_gui/match_mur_gui/base_gui.py`: `MurBaseGui`, `RosWorker`, `MurGuiContext`, Dialoge |
| Plane-/Alignment-Geometrie | `match_mur_gui/match_mur_gui/alignment.py` |
| GUI-Entrypoint | `match_mur_gui/scripts/general_mur_gui.py` |
| Remote UR-Ready | `match_mur_gui/scripts/ensure_ur_ready.py` |
| Benannte Armbewegung | `match_mur_gui/scripts/move_arm_to_named_pose.py` |
| Plane-Ausrichtung | `match_mur_gui/scripts/align_arm_to_plane.py` |
| UR-Dashboard-Preflight | `match_mur_gui/scripts/ur_dashboard_safety_check.py` |
| Optionales Mocap-Modul | `match_mobile_robotics_jazzy/mocap/match_mocap_gui/match_mocap_gui/mocap_gui_module.py` |

## Leitlinien für die Neuentwicklung

Die neue Implementierung sollte mindestens diese Verhaltensverträge bewusst übernehmen oder gezielt ersetzen:

1. Roboter-/Arm-Auswahl und klare Statusanzeige mit getrennten Verbindungs-, Controller- und Safety-Zuständen.
2. Explizite Profile für Hardware und Simulation mit konfigurierbaren Topic-, Service-, Controller- und Frame-Namen.
3. Eingabebereiche für Geschwindigkeit, Grenzen, Frame, Zielpose und MiR-Namespace mit dokumentierten Einheiten und Bereichen.
4. Sicherheitsabläufe: Start-Preflight, verständliche Details, bewusste Quittierung, Jog-Stop beim Schließen und ein zentraler Arm-Stop mit dokumentiertem Wirkbereich.
5. Persistente MiR-Posen und GUI-Protokolle an benutzerbeschreibbaren, konfigurierbaren Speicherorten.
6. Trennung zwischen UI, ROS-Adapter, Remote-Orchestrierung und anwendungsspezifischen Hardware-/Simulationsprofilen.
7. Erweiterungspunkte für optionale Bedienmodule wie Mocap, ohne sie fest in das Basisfenster einzubauen.

## Quellenstand

Beschreibung abgeleitet aus dem lokalen Workspace-Quellcode, insbesondere `match_mur_gui/match_mur_gui/base_gui.py`, den Skripten im `scripts/`-Verzeichnis sowie der README und dem Modulcode von `match_mocap_gui`. Laufzeitverhalten und Geräteverfügbarkeit sind durch diese Codelektüre nicht automatisch bestätigt; Topics, Services und Controller müssen in der Zielumgebung tatsächlich vorhanden sein.
