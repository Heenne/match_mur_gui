# match_mur_gui

**Required: configure your workspace path in `~/.bashrc` before using these commands.**
Replace the example directory below with the actual name and location of your
workspace. `WS` must point to the workspace root containing `src/`, not to a
repository inside `src/`. There is no automatic workspace discovery.

```bash
# Add these lines to ~/.bashrc; adjust the workspace folder to match your setup.
export WS="$HOME/your_workspace_folder"
source /opt/ros/jazzy/setup.bash
if [ -f "$WS/install/setup.bash" ]; then
    source "$WS/install/setup.bash"
fi
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-62}"
```

Then reload and verify the configured path:

```bash
source ~/.bashrc
printf 'Workspace: %s\n' "$WS"
test -d "$WS/src" && echo "Workspace src directory found" || echo "Fix WS in ~/.bashrc"
```

Check that the printed path matches your actual workspace folder name. Update
`WS` whenever you move or rename the workspace. Remote workspaces can differ;
set `REMOTE_WS` or the GUI's Remote WS field to the path on the remote computer.


## MuR Command Center starten

Das MuR Command Center ist die Desktop-App für Simulation und Hardware. Der
C++-Host und die QML-Ansichten liegen in `command_center/`.

```bash
cd "$WS"
source /opt/ros/jazzy/setup.bash
colcon build --packages-up-to match_mur_gui
source install/setup.bash
ros2 run match_mur_gui mur_command_center
```

Einzelheiten zu Profilen, Prozessen und ROS-Anbindung stehen in der
[Dokumentation zum Command Center](command_center/README.md).

### Mehrere Roboter lokal simulieren

1. Wähle **Simulation** mit Ziel **localhost**, öffne **Prozesse** und wähle eine Gazebo-Welt. Über **Gazebo-Welt auswählen** kannst du eine eigene `.world`- oder `.sdf`-Datei dauerhaft zur Auswahlliste hinzufügen. Die optionale **ROS-Karte** ist eine separate YAML-Datei mit dem zugehörigen Kartenbild und erscheint auf `/map`.
2. Stelle den Fenster-Schalter ein und klicke **Simulationswelt starten**. Sobald Gazebo zum Einsetzen von Robotern bereit ist, werden die Buttons in deren Zeilen freigegeben.
3. Wähle für `mur620a` bis `mur620d` je **MiR-Basis** oder **MuR620** sowie die Startkoordinaten in Metern. Klicke je Zeile **Einsetzen**. Die vier Roboter können in derselben Welt laufen. Bei einer eigenen Welt sollten die Startpositionen auf freien Flächen liegen.
4. Klicke beim gewünschten Roboter **Steuern** und dann **Verbinden**. Gib im Bereich **Roboter** das Joggen frei und halte eine Richtung gedrückt. Beim Roboterwechsel verbindet sich die App nach erneutem Klick auf **Verbinden** mit dem neuen Ziel. Die MuR620-Variante startet zusätzlich die Arm- und Lift-Controller sowie den namespaced MoveIt-Stack.

Die lokale Simulation benötigt die Pakete `mir_gazebo`, `mir_description` und `mur_launch_sim` aus `match_mobile_robotics_jazzy` im gesourcten Workspace. Die Welt und die Roboter laufen in derselben Gazebo-Partition und ROS-Domain. Das Öffnen oder Schließen der Weltansicht ändert den laufenden Aufbau nicht.

### Bestehende PyQt5-Basis-GUI

Die PyQt5-Basisoberfläche ist weiterhin separat verfügbar:

```bash
source /opt/ros/jazzy/setup.bash
source "$WS"/install/setup.bash
ros2 run match_mur_gui general_mur_gui.py
```

## Persönliche Einstellungen

Basis-, Mocap- und Navigations-GUI merken sich Änderungen an der Roboterauswahl,
dem Feld **Remote WS**, den UR-Armseiten, MiR inklusive Kameras, der
MiR-Lokalisierungsquelle sowie den Checkboxen unter **Launch Options** und
**Sync Code** automatisch. Beim nächsten Start werden diese gemeinsamen
Einstellungen wiederhergestellt. Hardware wird weiterhin erst über **Start
Hardware** gestartet.

Die benutzerspezifische Datei liegt außerhalb der Repositories unter
`~/.config/match_mur_gui/settings.ini` (bei gesetztem absolutem
`XDG_CONFIG_HOME` unter `$XDG_CONFIG_HOME/match_mur_gui/settings.ini`). Sie
wird daher nicht mit Git versioniert oder gepusht. Zum Zurücksetzen alle GUIs
schließen und diese Datei löschen. Ohne gespeicherte Werte gelten die bisherigen
Voreinstellungen; für **Remote WS** zunächst `REMOTE_WS` beziehungsweise `WS`.
Eine gespeicherte Remote-WS-Auswahl hat bei späteren Starts Vorrang.

## Lokalisierung beim Hardwarestart

Mit aktivierter **mir**-Checkbox unter **MiR-Lokalisierung** die gewünschte
Quelle auswählen und anschließend **Start Hardware** drücken. Die Auswahl gilt
für diesen Start und alle dabei ausgewählten MuRs. Ein bereits laufender
Hardwaretreiber wird dadurch nicht umgeschaltet.

| Auswahl | Verhalten |
| --- | --- |
| **MiR-Controller** (Voreinstellung) | Ausschließlich die native `robot_pose` der MiR, auch wenn Mocap bereits läuft. |
| **Mocap / Qualisys** | Ausschließlich die kalibrierte Qualisys-Pose im Frame `map`. Die Empfangs-Bridge wird gestartet oder wiederverwendet. |
| **AMCL · gespeicherte Navigationskarte** | Lokalen Kartenserver und AMCL starten oder vorhandene Sitzung verwenden. Laser und Encoder-Odometrie liefern die laufende Pose. |

Die Mocap-GUI wählt beim Öffnen **Mocap** vor; die Auswahl lässt sich vor dem
Hardwarestart ändern. Für Mocap und AMCL bleibt der lokale Empfangshelfer als
GUI-Prozess aktiv. Er aktiviert bei Bedarf die geprüfte Map-Ausgabe einer
vorhandenen Bridge, statt einen zweiten QTM-Datenstrom zu starten. Eine ältere
Bridge ohne diesen Dienst und ohne Map-Ausgabe einmal nach dem Update neu starten.
Fremde Kartenserver und AMCL-Sitzungen werden verwendet und beim Schließen
dieser GUI nicht beendet.

Der AMCL-Modus nutzt zunächst `mur620c_mocap.yaml` im Navigationspaket (oder die
bereits im selben Lokalisierungs-Namespace geladene Karte). Die Startpose kommt
wie beim bisherigen Vergleich einmalig aus Mocap; danach arbeitet AMCL ohne
laufende Mocap-Korrektur. Für den globalen Kartenbezug wird die bestehende,
geprüfte `map -> mocap`-Kalibrierung benötigt. **Open RViz** öffnet die
Navigationsansicht, wenn AMCL gewählt ist. Die Vergleichs-GUI kann unabhängig
davon eine Standmessung aktivieren.

Die Auswahl legt die Quelle der **ROS-Roboterpose** fest. Der Hardwareselektor
veröffentlicht als einzige Instanz `map -> <robot>/base_footprint`; AMCLs
interner TF-Baum bleibt privat. Ohne frische Daten der gewählten Quelle bleibt
die Lokalisierung wartend; es gibt keinen stillen Rückfall auf eine andere
Quelle. Die interne Fahrsteuerung der MiR wird damit nicht ersetzt.

Beim ersten Hardwarestart nach dem Update **Sync Code** und **Build before
launch** aktiviert lassen, damit `mir_localization_source` und der aktualisierte
Selektor auch auf dem MuR verfügbar sind. CLI-Starts unterstützen
`mir_localization_source:=mir|mocap|amcl`; das bisherige `auto` bleibt als
CLI-Voreinstellung für bestehende Skripte erhalten.

## Batteriedetails

Die MiR- und MuR-Batteriesymbole links oben zeigen den Ladezustand. Hover zeigt
die Details als Tooltip; ein Klick öffnet sie als Liste. Die MuR-BMS liefert
den expliziten Lade-/Entladezustand, Spannung, Strom und Restkapazität über
`/<robot>/bms_status/battery_state`. Die MiR-Batterie nutzt den REST-Status für
Prozent und Restlaufzeit sowie `/PB/bms_status` für Strom, Spannung, Kapazität
und Ladeflags. Strom und daraus berechnete Leistung sind positiv beim Laden,
negativ beim Entladen. Eine MuR-Restlaufzeit bzw. Zeit bis voll ist eine
Schätzung **bei unveränderter momentaner Stromstärke**; MiR liefert zusätzlich
seine eigene Restlaufzeit-Schätzung. Nicht vorhandene oder mehr als zehn
Sekunden alte Werte erscheinen nicht als aktuelle Messung.

Nach dem Aktualisieren von `match_mur_gui`, `mir_launch_hardware` und
`mur_launch_hardware` die Pakete bauen und die GUI sowie die jeweiligen
Hardwareprozesse neu starten.

## Ubuntu-Anwendungssuche und Desktop-Icon

### Inbetriebnahme auf dem Bedienrechner

Nach dem Build bei der Ersteinrichtung jedes Ubuntu-Bedienrechners die drei
Desktop-Verknüpfungen für Basis-GUI, Mocap und Navigation installieren.
`WS` muss wie oben beschrieben auf den Workspace zeigen; die Pakete
`match_mur_gui`, `match_mocap_gui` und `match_mur_navigation` müssen gebaut sein.

```bash
for app in base mocap navigation; do
  python3 "$WS"/src/match_mur_gui/scripts/install_gui_desktop.py \
    --workspace "$WS" --app "$app" || break
done
```

Danach stehen **MuR Basis-GUI**, **MuR Mocap** und **MuR Navigation** auf dem
Desktop und in der Anwendungssuche bereit. Nach dem Verschieben des Workspaces
den Installer mit dem aktualisierten `WS` erneut ausführen.

### Weitere Installationsoptionen

Der Installer legt für die Basis-GUI und auf Wunsch für Mocap, Cooperative Handling, OAK und Navigation eigene Starteinträge und Icons im Stil der MuR-Diagnose an. Er richtet die Verknüpfungen für den aktuellen Benutzer ohne `sudo` ein. Die benötigten ROS-Pakete müssen zuvor im Workspace gebaut sein.

```bash
cd "$WS"/src/match_mur_gui
python3 scripts/install_gui_desktop.py --app all
```

Danach in der Ubuntu-Anwendungssuche nach „MuR Basis-GUI“, „MuR Mocap“, „MuR Cooperative Handling“, „MuR OAK Kamera“ oder „MuR Navigation“ suchen oder die gleichnamigen Desktop-Icons doppelklicken. Der Installer prüft die vorhandenen ROS-Executables, bevor er Dateien anlegt. Jede Verknüpfung lädt beim Start `/opt/ros/jazzy/setup.bash` und das Workspace-Setup; ein Terminal ist nicht nötig. Ohne vorgegebene `ROS_DOMAIN_ID` verwendet sie Domain 62. Sie startet nur die gewählte GUI und keine Roboterhardware.

### Ubuntu 24.04 `.deb` ohne Workspace

Pushes auf `main` erzeugen einen Ubuntu 24.04 / ROS 2 Jazzy `.deb` als
GitHub Actions-Artefakt. Das Artefakt **match-mur-gui-ubuntu-24.04-ros-jazzy**
herunterladen und installieren:

```bash
sudo apt install ./match-mur-gui_*.deb
```

APT installiert die im Paket deklarierten ROS-, Qt- und QML-Laufzeitabhängigkeiten.
Auf dem Rechner müssen die passenden ROS-2-Jazzy-Paketquellen eingerichtet
sein. Danach erscheint **MuR Command Center** in der Ubuntu-Anwendungssuche.
Der Starteintrag lädt `/opt/ros/$ROS_DISTRO/setup.bash` (standardmäßig Jazzy)
und startet die installierte App ohne Quell-Workspace. ROS-Pakete für die
Simulation und projektspezifische RViz-Konfigurationen kommen aus dem
jeweiligen Robotik-Workspace. Die bisherige **MuR Basis-GUI** hat weiterhin
ihren eigenen Menüeintrag. Ohne gesetztes `WS` verwendet deren Starter
`$XDG_DATA_HOME/match_mur_gui` (sonst `~/.local/share/match_mur_gui`).

Alle GUIs mit der gemeinsamen MuR-Basis konfigurieren vor dem ROS-Start automatisch
direkte DDS-Peers für die per SSH erreichbaren Hostnamen `mur620a` bis `mur620d`. Dadurch
funktioniert die Verbindung auch ohne zuverlässige Multicast-Erkennung. Die
Konfiguration berücksichtigt die aktuelle `ROS_DOMAIN_ID` (Standard 62) und liegt
unter `logs/gui/dds/`; sie wird bei Bedarf neu erzeugt. RViz und Kartierung erben
dieselbe Einstellung. Ein separates Bash-Skript oder eine Datei unter `/tmp` ist
nicht erforderlich. Auch `ros2 run` verwendet diese automatische Konfiguration.
Im GUI-Log zeigt `DDS: direct peers ...` die verwendeten Adressen und die Domain.

Vorhandene gültige Fast-DDS-Profile, ein expliziter Discovery-Server, eine andere
Middleware oder eine ausdrücklich auf localhost beschränkte Erkennung bleiben
erhalten. Weitere direkte Peers können über `ROS_STATIC_PEERS` mit Semikolon
getrennt ergänzt werden. Nach einem Netzwerk- oder Adresswechsel die GUI neu
starten, damit die Adressen vor dem Start der ROS-Kontexte erneut aufgelöst werden.

Mit `--app base` wird nur die Basis-GUI installiert; weitere Werte sind `mocap`, `cooperative`, `oak` und `navigation`. `--workspace PFAD` wählt einen anderen Colcon-Workspace, `--no-desktop-shortcut` erzeugt nur Einträge für die Anwendungssuche. Der Aufruf ist wiederholbar. Bestehende fremde Dateien mit denselben Namen werden nicht überschrieben. Auf GNOME werden Desktop-Verknüpfungen als vertrauenswürdig markiert; falls der Dateimanager dies nicht unterstützt, im Kontextmenü „Starten erlauben“ wählen.

## Home über MoveIt

Home nutzt den laufenden `move_group` des ausgewählten Roboters für Planung und Ausführung. Die benannte Zielpose wird aus dessen SRDF gelesen. Dadurch entfällt eine zusätzliche MoveItPy-Instanz einschließlich ihres bisherigen Absturzes beim Beenden. Die Geschwindigkeitseinstellung skaliert weiterhin die geplante Trajektorie.

Vor Planung und Ausführung muss das UR-External-Control-Programm laufen. Der Hardware-Trajektorien-Proxy überwacht zusätzlich den Programmstatus und begrenzt die Ausführungsdauer. Bei Abbruch, Verbindungsverlust oder Zeitüberschreitung fordert er die Stornierung an und deaktiviert Trajektorien- und Geschwindigkeitsregler. Eine fehlende Bestätigung wird als Fehler protokolliert. Nur nach erfolgreicher Ausführung wird ein zuvor aktiver kartesischer Regler wieder aktiviert. Home selbst hat eine Gesamtlaufzeitgrenze von 70 Sekunden mit anschließender Stornierung; die äußere GUI-Prozessgrenze beträgt 90 Sekunden.

Für Änderungen an diesem Ablauf müssen `match_mur_gui`, `mur_control`, `mur_launch_hardware` und `mur_moveit_config` auf den verwendeten Rechnern aktualisiert und gebaut werden. Bereits laufende Proxy- und MoveIt-Prozesse müssen anschließend neu gestartet werden.

## Ausgewählte URs kontrolliert herunterfahren

Im Tab **UR** den Knopf **Ausgewählte URs herunterfahren** verwenden. Ziel sind ausschließlich die ausdrücklich markierten MuRs und die markierten Armseiten `UR10_l` / `UR10_r`. Ohne MuR- oder Armauswahl wird nichts abgeschaltet. Der Bestätigungsdialog zeigt die konkrete Zielliste; Abbrechen ist voreingestellt. Werkstücke vorher ablegen oder sichern: Der Helfer fährt keine Parkpose an und schaltet keine Greifer gezielt um.

UR beschreibt zuerst das Ausschalten des Arms und anschließend das Herunterfahren der Control Box. Der Helfer setzt dies über den Dashboard Server auf Port 29999 um:

1. PolyScope-Version und bei e-Series den Remote-Control-Modus prüfen.
2. `stop` senden und auf `programState = STOPPED` warten.
3. `power off` senden und auf `robotmode = POWER_OFF` warten.
4. `shutdown` senden und die Antwort `Shutting down` auswerten.

Ein bereits ausgeschalteter Arm wird nach Prüfung von `STOPPED` direkt heruntergefahren. Fehler oder Zeitüberschreitungen verhindern den nächsten Schritt für diesen Arm. Die anderen ausgewählten URs werden unabhängig bearbeitet; Ergebnis und Dashboard-Antworten erscheinen im GUI-Log und im Armstatus. **„Herunterfahren vom UR bestätigt“ bedeutet, dass der Dashboard Server den Befehl angenommen hat**; der endgültige stromlose Zustand wird damit nicht gemessen. Bei Verbindungsverlust ohne Antwort wird kein Erfolg angenommen und `shutdown` nicht automatisch wiederholt.

Die GUI beendet ausgewählte Home-/Align-Abläufe, fordert deren Stornierung an, setzt Jog-Befehle auf null und sperrt Freedrive-Keepalives sowie neue Bewegungsanforderungen für die Zielarme. Verzögerte Enable-Retries werden verworfen. Die Sperre bleibt auch bei Fehlern bestehen, bis ausdrücklich **Enable URs / Ready** oder **Start Hardware** verwendet wird. Laufende Enable-/Start-Preflights und die automatische UR-Aktivierung beim Hardware-Start müssen vor dem Abschalten abgeschlossen sein. Bei fehlgeschlagenem Hardware-Start zuerst **Stop Managed Processes** verwenden. Während des Abschaltens bleibt die GUI offen und „Stop Managed Processes“ lässt den Abschalthelfer weiterlaufen. Andere Bediengeräte und externe Automatisierungen dürfen währenddessen keine Startbefehle senden.

Voraussetzungen: SSH-Zugang zum ausgewählten MuR-Rechner; dessen Hostnamen `UR10_l` und `UR10_r` müssen auf die eigenen URs zeigen. Der Helfer läuft über `python3` direkt aus `Remote WS/src/match_mur_gui/scripts/shutdown_urs.py`, auch ohne laufenden ROS-Treiber. Unterstützt werden CB3 / PolyScope 3 und e-Series / PolyScope 5 ab 5.6 (für die Remote-Abfrage). PolyScope X und unbekannte Versionen werden abgewiesen. `match_mur_gui` auf GUI- und MuR-Rechnern aktualisieren, bauen und die GUI neu starten:

```bash
colcon build --packages-select match_mur_gui --symlink-install
```

MiR, Hubachsen und MuR-Rechner werden nicht heruntergefahren. Das erneute Einschalten der UR-Control-Box erfolgt lokal. Vor Trennen der Netzversorgung das vollständige Herunterfahren abwarten; nach dem Trennen nennt UR 30 Sekunden zum Entladen gespeicherter Energie.

Herstellerquellen:

- [UR: Power Down The Robot](https://www.universal-robots.com/manuals/EN/HTML/SW5_24/Content/prod-usr-man/complianceUR5e/H_g5_sections/mechanical_interface_g5/powerdown.htm)
- [UR: Dashboard-Befehle für e-Series](https://www.universal-robots.com/manuals/EN/HTML/SW5_23/Content/prod-dashboard/Dashboard_table.htm)
- [UR: Dashboard-Befehle für CB-Series](https://www.universal-robots.com/articles/ur/dashboard-server-cb-series-port-29999/)

### Command Center: Endpunkte im Profil

Im Command Center werden bewegende Basis- und Navigationsendpunkte unter **Settings → Edit profile** für jedes Preset ausdrücklich konfiguriert. Leere Felder deaktivieren die entsprechende Funktion. Alle Namen müssen absolute ROS-Namen unter dem ausgewählten Roboter sein, z. B. `/mur620d/cmd_vel` mit `geometry_msgs/msg/Twist` für die MiR-Basis oder `/mur620d/mobile_base_controller/cmd_vel` (TwistStamped) für die Simulation. Die GUI prüft den konfigurierten Namen, Nachrichtentyp und passenden Subscriber vor dem Senden. Bei Verlust des MiR-Endpunkts wird die Jog-Freigabe aufgehoben. Nach einem Profilwechsel muss erneut freigegeben werden.

Für Navigation bezeichnet das Goal-Feld im Simulationsmodus die Nav2-Action `NavigateToPose` und das Pose-Feld eine `PoseStamped`-Quelle. Im Hardwaremodus bezeichnet Goal einen `PoseStamped`-Empfänger, Cancel einen `actionlib_msgs/msg/GoalID`-Empfänger und Pose eine `geometry_msgs/msg/Pose`-Quelle. Diese Zuordnung gehört zum jeweiligen Modus; nach einem Moduswechsel müssen die Navigationsendpunkte erneut bestätigt werden. Endpunkte werden im Preset gespeichert und in portable Presets exportiert. Bestehende Presets ohne diese Felder starten mit deaktivierten Funktionen und müssen eingerichtet werden.
