# MuR Control GUI – Ziel-Spezifikation für Hardware und Simulation

## 1. Zweck

Die MuR-App ist die gemeinsame Arbeitsoberfläche für Versuche, Entwicklung und Fehlersuche. Sie unterstützt lokale und entfernte Simulationsarbeit sowie den Betrieb realer MuR-Hardware im Versuchsfeld. Vom gewählten Preset aus kann die Bedienperson einen Versuch starten, seinen Zustand beobachten und bei einem Fehler zum betroffenen Prozess und seinem Log springen.

Die Bedienoberfläche bietet gemeinsame Roboterauswahl, Status, manuelle Bewegung, Ziele, Logs und Werkzeuge. Verbindungen, Start-/Stop-Abläufe, Safety-Prüfungen, ROS-Endpunkte, Controller und Zeitquellen werden über das gewählte Betriebsprofil bereitgestellt. Die GUI darf aus der Auswahl „Simulation“ oder „Hardware“ keine Topics oder Controller erraten.

Die Bestandsaufnahme des jetzigen Tools bleibt separat in [`current_gui_spec.md`](current_gui_spec.md) erhalten. Diese Datei ist die Ziel-Spezifikation und ersetzt nicht die historische Beschreibung.
Die gemeinsamen Begriffe stehen im [Glossar](../GLOSSARY.md).

## 2. Ziele und Nicht-Ziele

### Ziele

- Eine GUI für zwei klar unterscheidbare Betriebsarten: **Simulation** und **Hardware**.
- Installierbare Desktop-App für Ubuntu 24.04 als `.deb` mit App-Menüeintrag. Die App läuft in einem eigenen Fenster ohne Browser oder Webserver für die Bedienoberfläche. Installierbare ROS-2-Jazzy-Laufzeitabhängigkeiten sind zulässig. Anwender brauchen für die Installation der App keinen Quellcode-Checkout.
- Simulation lokal oder im Labor-LAN bzw. VPN verbinden, starten, beobachten und kontrolliert beenden können.
- Im Versuchsfeld mit ausgewählten physischen MuRs und Armen sicher arbeiten können.
- Gemeinsame Bedienkonzepte verwenden, während die ROS-Integration profilabhängig bleibt.
- Vor jeder Aktion sichtbaren Zielkontext zeigen: Modus, Verbindung, ROS-Domain, Host, Roboter und aktiver Bewegungscontroller.
- ROS-Graph und Controllerstatus zur Laufzeit prüfen und nicht verfügbare Aktionen sichtbar deaktivieren.
- Benutzerkonfiguration, Posen und Logs in benutzerbeschreibbaren Verzeichnissen speichern.

### Nicht-Ziele

- Die GUI ersetzt keine hardwareseitigen Not-Aus-/Schutzfunktionen und keine Sicherheitsregeln des Roboters.
- Gazebo übernimmt weiterhin die Simulation. Bestehende Hardware- und Simulations-Launchpakete bleiben separat nutzbar.
- Ein Simulationsstatus darf nicht als Nachweis der Einsatzbereitschaft einer Hardware interpretiert werden.

## 3. Betriebs- und Verbindungskontext

Pro App-Instanz ist eine **Sitzung** ausgewählt. Hauptfenster und abgedockte Ansichten teilen dieselbe Sitzung und zeigen dasselbe Zielsystem. Beim Wechsel des Presets oder Zielsystems wechselt der angezeigte Kontext in allen Ansichten gemeinsam.

| Einstellung | Werte/Beispiele | Bedeutung |
|---|---|---|
| Betriebsart | Simulation, Hardware | Bleibt im Hauptfenster sichtbar und kann dort gewechselt werden. |
| Betriebsprofil | Zum Beispiel „Lokale Simulation“ oder „Versuchsfeld“ | Legt Zielsystem, verfügbare Funktionen und Prüfungen für das konkrete Einsatzszenario fest. |
| ROS-Ziel | lokaler Rechner oder konfigurierter Remote-Host | Rechner, auf dem ROS-Graph und Roboter-/Simulationsprozesse laufen. |
| ROS-Domain | konfigurierbare Zahl | Domain des gewählten Systems; wird vor Verbindung sichtbar angezeigt. |
| Workspace/Setup | automatisch erkannt oder explizit konfiguriert | ROS-Umgebung und Launchpakete des Zielsystems. |
| Roboter | aus Profil/Graph verfügbare MuRs | Konkreter Roboter, dessen Status/Aktionen die GUI bedient. Mehrfachauswahl ist nur für geeignete Status-/Startaktionen erlaubt. |
| Arme | links/rechts | Vorhandene Arme; Verfügbarkeit stammt aus Profil und ROS-Graph. |

Die GUI muss zwischen **Verbinden** (an vorhandenen Graph anhängen) und **Starten** (konfigurierten Prozess starten) unterscheiden. Ein laufender, nicht von der GUI gestarteter Prozess darf nicht als GUI-eigener Prozess beendet werden.

### Sitzungsstatus

Die Kopfzeile zeigt dauerhaft:

- Betriebsart, aktives Betriebsprofil, Zielhost und ROS-Domain.
- Verbindungszustand: getrennt, Verbindung wird geprüft, verbunden, eingeschränkt oder fehlgeschlagen.
- Laufzustand: nicht gestartet, startet, läuft, stoppt, beendet oder fehlgeschlagen; Startquelle „GUI“ oder „extern“.
- Ausgewählten Roboter und die Verfügbarkeit von Basis, linkem/rechtem Arm, Lift und optionalen Komponenten.
- Simulationszeit aktiv/fortschreitend bei Simulationsprofilen; Systemzeit bei Hardware.

Bei Änderung von Profil, Host, Domain oder Workspace sind bestehende Verbindungen zuerst kontrolliert zu lösen. Aktive Bewegungen werden vor dem Wechsel gestoppt; der Wechsel selbst darf keine Roboterbewegung auslösen.

## 4. Informationsarchitektur

### Hauptfenster und Ansichten

Die **Sitzungsleiste** zeigt Betriebsart, Preset, Zielroboter und Verbindung. Host und ROS-Domain sind im Sitzungsdetail sichtbar. Die **Statusleiste** zeigt abhängig von der Betriebsart den Roboter- oder Simulationsstatus und die letzte Warnung. Die folgenden Namen und die Anordnung der Ansichten werden im Layout-Prototyp erprobt.

| Ansicht | Inhalt |
|---|---|
| **Übersicht** | Zustand des gewählten Roboters und der Komponenten, Hinweise, nächste sinnvolle Aktion. |
| **Visualisierung** | RViz oder eine eigene ROS-Datenansicht, zusätzlich die Gazebo-Weltansicht, wenn eingebettet. |
| **Roboter** | Bedienung von Basis, Armen und Lift, Ziele und Posen sowie profilabhängige Zusatzfunktionen wie MiR-Leuchten. |
| **Prozesse** | Startablauf des Presets mit Status und Sprung zum jeweiligen Log. Einzelne lokale und entfernte Launches bleiben für gezielte Fehlersuche bedienbar. Im Simulationsmodus zusätzlich die Einstellung „Gazebo-Weltansicht beim Start öffnen“. |
| **Diagnose** | ROS-Graph, Controller, TF, Datenalter und Gründe für fehlende Funktionen. |
| **Logs** | Zusammengeführte Live-Ansicht, Tabs pro Launch oder ROS-Quelle, Filter und Export. |
| **Einstellungen** | Presets, Profil- und Verbindungsdaten sowie persönliche Einstellungen. |

Visualisierung und Logs lassen sich als eigene Fenster öffnen und wieder andocken. Mehrere Ansichten können gleichzeitig sichtbar sein, etwa RViz auf dem Hauptmonitor und Logs daneben. Abgedockte Ansichten bleiben an dieselbe Sitzung gebunden und zeigen deren Kontext. Abdocken oder Schließen einer Ansicht ändert weder Verbindung noch Prozesszustand. Eine eingebettete Gazebo-Weltansicht soll sich auf dieselbe Weise abdocken lassen.

Die Oberfläche muss bei üblichen Operator-Laptopauflösungen nutzbar sein und bei kleinen Fenstern scrollen. Kritische Aktionen dürfen nicht außerhalb des sichtbaren Bereichs versteckt werden.

### Übersicht

Zeigt pro ausgewähltem Roboter Karten mit:

- Online-/Datenzustand und Alter der letzten Nachricht.
- Basispose und Basiscontroller, falls verfügbar.
- MuR-/MiR-Batterie, sofern die entsprechenden Quellen im Profil existieren.
- Für jeden Arm: Controllerstatus, UR-Ready-/Reverse-Interface-Status bei Hardware, Joint-/TCP-Daten soweit verfügbar und Freedrive-Zustand.
- Lift-Höhe und Grenzstatus, soweit verfügbar.
- Simulationslaufzeit/Sim-Zeit bzw. Hardware-Safety-/Betriebsstatus.

Fehlende Werte werden als „nicht verfügbar“ mit Grund dargestellt, nicht als 0 oder als gesund interpretiert. Jeder Messwert zeigt Einheit und Datenalter bei veralteten Daten.

### Visualisierung

- Die App soll ROS-Daten und die zugehörigen Diagnosezustände in beiden Betriebsarten darstellen können. Dazu gehören Fehler, die erklären, warum eine erwartete Darstellung fehlt.
- Vollständiges RViz ist eine Option innerhalb der Ansicht **Visualisierung**. „Vollständig“ bezieht sich auf die RViz-Oberfläche mit Displays, Display-Einstellungen, Statusmeldungen, Werkzeugen, Kamerasichten und `.rviz`-Konfigurationen. Sitzung, Launches, Logs und Roboterbedienung liegen in den eigenen Bereichen der App. Die Machbarkeit der Einbettung, des Abdockens und des Wechsels des ROS-Kontexts wird früh prototypisch geprüft.
- Eine eigene ROS-Datenansicht soll als wählbare Alternative zu RViz möglich sein. Ihre erste Ausbaustufe und ihre Priorität gegenüber der RViz-Integration sind offen. Verbindungs- und Sitzungszustand bleiben beim Wechsel der Ansicht erhalten.
- Die Gazebo-Weltansicht ist unabhängig von der gewählten ROS-Datenansicht. Wenn die Einbettung im Prototyp keinen stabilen Weg ergibt, öffnet die App Gazebo in seinem eigenen Fenster.

## 5. Gemeinsame Funktionsbereiche

### Startabläufe

Jedes Preset kann einen benannten **Startablauf** anbieten, etwa „Simulation starten“. Eine Hauptaktion stößt die zugehörigen Launches gemeinsam an. Unter **Prozesse** sind deren Zustände und Logs sichtbar. Einzelne Launches können dort weiterhin gezielt gestartet werden. Für den Standardablauf sind keine Klicks pro Launch erforderlich.

Auch die vom Startablauf verwendeten Startskripte müssen die Prozesszuordnung respektieren. Die vorhandenen Simulations- und Hardware-Wrapper können standardmäßig Prozesse anhand von Namensmustern beenden. Für das Starten aus der App braucht es einen Aufrufpfad, der fremde Prozesse außerhalb des eigenen Laufs erhält.

### 5.1 Basissteuerung

- Jog-Steuerung mit vorwärts/rückwärts, links/rechts drehen und sofortiger Null-Geschwindigkeit.
- Einstellbare lineare und Winkelgeschwindigkeit mit Einheiten, sicheren Profilgrenzen und sichtbarem aktuellen Wert.
- Tastatursteuerung nur bei Fokus auf den Jog-Bereich; Tastenbelegung sichtbar. Loslassen, Fokusverlust, Fensterwechsel, Verbindungsverlust und Dialogschluss senden ein Stop-Kommando.
- Konfigurierbare Totmann-/Hold-to-run-Bedienung. Ein gehaltenes Kommando wird zyklisch mit begrenzter Gültigkeitsdauer publiziert; kein unbegrenzt gültiger letzter Befehl.
- Anzeige des tatsächlich verwendeten Topicnamens, Typs und Abonnenten-/Verbindungsstatus in Diagnoseansicht.
- Erkannte Roboter und MiR-Namespaces können aktualisiert werden. Eine ausdrücklich konfigurierte Zuordnung bleibt in der Sitzung sichtbar.
- Optional „Pose speichern“, falls das Profil eine Posequelle unterstützt.
- **MiR-Leuchten** als optionale Hardwarefunktion. Regenbogeneffekt starten und stoppen, MATCH-Farbe setzen oder eine eigene RGB-Farbe mit Vorschau auswählen und anwenden. Die App zeigt die Rückmeldung des jeweiligen Dienstes.

### 5.2 Armsteuerung

- Auswahl Roboter/Arm; Translation und Rotation mit Geschwindigkeitsbegrenzung.
- X/Y/Z- und Roll/Pitch/Yaw-Jog, Maus-/Tasten-Hold-to-run sowie sichtbares Stop.
- Status von Arm-Controller, Joint States, TCP-/Referenzframe und Bewegungsbereitschaft.
- **UR bereitmachen** ist eine eigene Hardwareaktion für ausgewählte Arme. Sie zeigt den Status von External Control und Reverse-Interface sowie das Ergebnis je Arm.
- **Home** pro Seite fährt eine konfigurierte benannte MoveIt-Pose mit einstellbarer Geschwindigkeit an und zeigt Fortschritt, Abbruch und Ergebnis.
- **Freedrive** pro unterstütztem Arm zeigt den bestätigten Zustand und lässt sich wieder verlassen.
- **TCP zur Ebene ausrichten** bietet XY-, XZ- und YZ-Ebene mit Vorschau, Wahl der Ausrichtungsrichtung, Referenzframe und Winkelgeschwindigkeit. Die Ausrichtung soll die TCP-Position erhalten.
- **Lift** zeigt die aktuelle Höhe und die Grenzen je Seite. Schrittweite und Grenzen kommen aus der Profilkonfiguration oder werden innerhalb der zulässigen Werte eingestellt. Die App bietet Heben und Senken mit Rückmeldung. Bei fehlender aktueller Höhe oder erreichter Grenze bleibt die jeweilige Bewegung gesperrt.
- Diese Funktionen erscheinen entsprechend den Fähigkeiten des gewählten Profils und der Verfügbarkeit der benötigten Schnittstellen. Fehlt eine Voraussetzung, erscheint der Grund in der Ansicht.
- Armbewegung nur bei bestätigtem Zustand des ausgewählten Profils starten; bei fehlendem Controller, Joint States, Zeitquelle oder Status keine Bewegungsaktion ausführen.
- Beenden einer Bewegungsaktion umfasst GUI-Zustand, laufende GUI-Aktion, ROS-Cancel, falls eine Action verwendet wird, und sichere Stop-Kommandos des Profils.

### 5.3 Ziele und Posen

- Ziele über Position und Yaw in einem im Profil unterstützten Frame setzen.
- Wertebereich, Einheiten und Ziel-Frame dauerhaft neben Eingaben anzeigen.
- Ziel aus den Eingabefeldern oder aus einer gespeicherten Pose senden. Posen lassen sich unter einem Namen speichern, aktualisieren, laden und löschen. Die Liste kann neu geladen werden.
- **Navigation abbrechen** ist eine eigene Aktion, wenn der Profiladapter einen Abbruch unterstützt. Die App zeigt das Ergebnis für den gewählten Roboter.
- Gespeicherte Posen in benutzerbeschreibbarer Konfigurationsdatei; ungültige Datei darf den Start nicht verhindern und muss mit verständlichem Fehler gemeldet werden.
- Bestehende MiR-Posendateien sollen importierbar sein. Die aktuelle GUI akzeptiert ein JSON-Array oder ein Objekt mit einem `poses`-Array.
- Bei „aktuelle Pose übernehmen“ Quelle/Topic, Frame und Zeitstempel prüfen; kein Ziel speichern, wenn Pose fehlt oder veraltet ist.
- Hardwareprofil kann MiR-eigene Ziele/Posen verwenden; Simulationsprofil kann Nav2-, Ground-Truth- oder MiR-Pose anbieten. Diese Quellen werden je Profil explizit konfiguriert und in der UI benannt.

### 5.4 Erweiterungen

Module wie Qualisys/Mocap, Kamera, Navigation oder Diagnose registrieren Panels, Statusanzeigen und Lifecycle-Hooks über eine stabile Erweiterungsschnittstelle. Jedes Modul deklariert benötigte ROS-Endpunkte und Profile. Die Basis-GUI startet nur Module, die installiert und für das aktive Profil freigegeben sind.

Das bestehende Mocap-Modul soll als optionaler Bereich integrierbar sein. Es umfasst Pose, Frequenz, Datenalter und Map-Status pro Roboter, eine Einstellung für die Ausgabe von Map-Posen und Roboter-TF sowie Start/Stop der von der App verwalteten Bridge. Eine vorhandene, geeignete Bridge kann weiterverwendet werden. Freeze/Resume bleibt von den verfügbaren Services und dem aktiven Profil abhängig.

Die separat startbaren Anwendungen für Navigation, OAK-Kamera und Cooperative Handling sind Integrationskandidaten. Welche ihrer eigenen Funktionen in die MuR-App übernommen werden, ist separat zu entscheiden.

## 6. Profilverhalten

### 6.1 Simulation

Das Simulationsprofil muss:

- Einen vorhandenen Simulationsgraphen lokal oder remote erkennen und anhängen können.
- Optional einen konfigurierten Launch starten und nur den von der GUI gestarteten Launchprozess kontrolliert beenden können.
- Die Einstellung **Gazebo-Weltansicht beim Start öffnen** bestimmt, ob die App zusammen mit der Simulation auch den grafischen Client öffnet. Die App merkt sich die Einstellung.
- Ziel für die erste Version ist **Gazebo-Weltansicht öffnen/schließen** bei laufender Simulation, sofern der Prototyp die Trennung bestätigt. Die App startet den Gazebo-Server unabhängig vom grafischen Client. Der Client kann sich nachträglich mit dem laufenden Server verbinden. Beim Schließen des Clients laufen Simulator, ROS-Bridge und ROS-Visualisierung weiter. Wenn der Zielserver für den Client nicht erreichbar ist, zeigt die App das in der Weltansicht an. [Gazebo Harmonic: Server und GUI getrennt starten](https://gazebosim.org/docs/harmonic/getstarted/#step-2-run)
- Die Option `--headless-rendering` für Sensoren auf einem Server ohne Displayserver ist eine separate Einstellung beim Serverstart. [Gazebo Sim: Headless Rendering](https://gazebosim.org/api/sim/8/headless_rendering.html)
- `use_sim_time` aus dem Profil übernehmen und anzeigen. Sim-Zeit-/Clock-Ausfall blockiert zeitabhängige Aktionen und wird als Fehlerstatus angezeigt.
- ROS-Graph nach verfügbaren Robotern, Topics, Actions, Services und Controller-Managern durchsuchen; Profilregeln verwenden, um Endpunkte den Funktionen zuzuordnen.
- Simulationseigene Basis-, Arm- und Lift-Controller verwenden; Hardware-UR-Reverse-, Dashboard- und Safety-Dialoge nicht ausführen.
- Für die ROS-Datenvisualisierung die Simulationskonfiguration, den gewählten ROS-Graphen und Sim-Zeit verwenden.
- Simulation stoppen können, wenn GUI-gestartet; extern gestartete Simulation als extern markieren und nicht beenden.

Für den aktuellen Simulationsstand des Workspace sind insbesondere diese Kandidaten dokumentiert; alle Namen bleiben konfigurierbar und müssen beim Verbinden verifiziert werden:

| Funktion | Aktueller Simulationskandidat | Status/Regel |
|---|---|---|
| Basis-Geschwindigkeit | `/<robot>/mobile_base_controller/cmd_vel` (`geometry_msgs/TwistStamped`) | Durch `diff_drive_controller`/Launchkonfiguration; GUI publiziert nur, wenn passender Subscriber und Typ erkannt wurden. |
| Basis-Odometrie | `/<robot>/mobile_base_controller/odom` | Profil kann als Odometriequelle nutzen. |
| Ground-Truth-Pose | `/<robot>/ground_truth/pose`, `/<robot>/ground_truth/odom` | Simulationsdiagnostik/-pose; als Ground Truth kennzeichnen, nicht mit Sensorlokalisierung verwechseln. |
| Controller-Manager | `/<robot>/controller_manager` | Roboterweiter Manager, anders als der Hardware-Manager pro UR-Arm. |
| Gelenkzustände | globales `/joint_states` oder profilkonfigurierter Topic | Namenspräfixe berücksichtigen; mehrere Roboter dürfen keine mehrdeutigen Joint-Namen erzeugen. |
| Kartesischer Armjog (falls J-PARSE gestartet) | `/<robot>/jparse_velocity_controller_l/twist_cmd`, `..._r/twist_cmd` | Simulationsspezifischer Adapter; Nachrichtentyp ist zur Laufzeit zu prüfen. |
| Zeit | `/clock` | Sim-Zeitquelle; erforderlich, wenn `use_sim_time=true`. |

Nach dem ersten Simulationsablauf folgt die Basissteuerung. UR-Arm-Jog, Lift, Home und Ziele in Simulation kommen hinzu, wenn deren konkrete Controller-/Action-Verträge im Profil festgelegt und geprüft wurden. Ein Hardwarecontroller darf nicht stillschweigend als Fallback verwendet werden.

### 6.2 Hardware / Versuchsfeld

Das Hardwareprofil muss:

- Lokalen oder konfigurierten Remote-Host verwenden; Host-Schlüssel, Benutzer und Workspace sind konfigurierbar. Zugangsdaten werden nicht in der GUI-Konfiguration gespeichert.
- ROS-Erkennung im Labor-LAN und VPN unterstützen. Verwendete Domain und Erkennungsart sind in der Diagnose sichtbar. Profile sollen direkte DDS-Peers oder eine vorhandene Discovery-Konfiguration verwenden können, wenn Multicast für das Ziel nicht ausreicht.
- Verbindung und Host-Preflight getrennt vom Hardwarestart anbieten.
- Optional Quellcode synchronisieren/builden können. Vor destruktivem Sync wird Quelle/Ziel mit Pfad angezeigt und bestätigt; Excludes und Sync-Richtung sind festgelegt. Ein Build ist nur bei explizitem Build-Schritt auszuführen.
- Vor Hardwarestart Host- und Netzprüfungen für ausgewählte Arme ausführen; UR-Dashboard-Safety-/Protective-Stop-Status prüfen.
- Ein erkanntes Safety-Stop nicht automatisch quittieren. Detailmeldung, betroffene UR und manuelle Entscheidung zeigen. Clear und Fortsetzung verlangen explizite Bestätigung; unklarer Preflight blockiert Start standardmäßig.
- UR-Ready/External-Control-Bereitschaft mit Timeout und klarer Diagnose erfassen.
- Roboter/Arme/MiR/BMS/FT/MoveIt gemäß Profil starten und jeden Prozess mit Status, Log und Abbruchmöglichkeit verfolgen.
- Die vorhandenen Startoptionen als erweiterte Profilparameter berücksichtigen. Dazu gehören Armauswahl, MiR-Treiber und optionale MiR-RGB-/Tiefenkameras, Controllerwahl, Kraftsensor und erforderliche Wrench-Daten, Kollisionsvermeidung und Marker, deaktivierte Admittanz, MoveIt und seine Geschwindigkeit sowie BMS-Konfiguration. Der bestätigte Standard-Startablauf nutzt die gespeicherten Werte.
- Wenn der MiR-Start mehrere Lokalisierungsquellen unterstützt, lässt sich vor dem Start zwischen MiR-Controller, Qualisys/Mocap und AMCL mit gespeicherter Karte wählen. Die App zeigt die gewählte Quelle und die Datenverfügbarkeit. Die Auswahl gilt für den nächsten Start. Vorhandene geeignete Lokalisierungsprozesse können angebunden werden.
- Kontrolliertes **UR-Herunterfahren** wird als eigene optionale Hardwareaktion berücksichtigt. Zielroboter und Armseiten sind vor Bestätigung sichtbar. Fortschritt und Ergebnis erscheinen pro Arm. Der normale Prozess-Stop bleibt eine separate Aktion.
- Hardwarebewegungsfunktionen bei nicht bereitem Roboter oder fehlender UR-Schnittstelle deaktivieren.
- Hardware-RViz-Konfiguration, `use_sim_time=false`, Controller- und Frame-Namen aus dem Hardwareprofil laden.

Hardwareprofile dürfen spezifische Werte enthalten, darunter Reverse-IP, BMS-CAN-Gerät, Launchargumente, zulässige Controller und `Home_custom`. Diese Werte gehören in Profil-/Systemkonfiguration, nicht in UI-Grundlogik.

### 6.3 Funktionsmatrix

| Funktion | Simulation | Hardware |
|---|---|---|
| ROS-Graph verbinden | lokal/remote | lokal/remote |
| Prozess starten/stoppen | konfigurierter Sim-Launch; fremde Prozesse bleiben unangetastet | Hardware-Launch über Hostprofil; Status und Preflight zwingend |
| Basis-Jog | Sim-Controller/Topic aus Profil | MiR-/Basis-Treiber aus Profil |
| Ziele setzen und Navigation abbrechen | sofern ein Navigationsadapter verfügbar ist | sofern der MiR-Adapter die Aktion unterstützt |
| MiR-Leuchten und Kameras | sofern eine entsprechende Simulationsfunktion vorliegt | optional je nach Treiber und Diensten |
| Arm-Jog | Sim-Controlleradapter; optional | UR-Ready/Controller-Adapter; optional nach Safety-Prüfung |
| Freedrive | verborgen, außer Simulation stellt explizite Entsprechung bereit | Controllerwechsel und UR-Status aus Profil |
| Home / Alignment | Sim-Action/MoveIt, sofern eingerichtet | Hardware-MoveIt-/Skriptpfad nach UR-Ready |
| Lift | Sim-Liftcontroller | Ewellix-Liftcontroller |
| Dashboard-Safety | nicht anwendbar | erforderlich bei UR-Hardware |
| UR bereitmachen / UR herunterfahren | nicht anwendbar | pro Arm, sofern vom Profil unterstützt |
| RViz | Sim-Konfiguration, Sim-Zeit | Hardwarekonfiguration, Systemzeit |
| Gazebo-Weltansicht | ohne Fenster, eingebettet nach erfolgreichem Prototyp oder separates Gazebo-Fenster | nicht anwendbar |
| Ground Truth | möglich, klar als Simulation markiert | nicht als Ersatzdaten verwenden |
| Mocap | optional | optional |

## 7. Statusanzeigen und Meldungen

- In der Sitzungsleiste und in Prozesszeilen erscheinen kurze Zustände in farbigen, abgerundeten Rechtecken mit mittig ausgerichtetem Text, etwa **Startet**, **Läuft** oder **Fehler**. Der Text benennt den Zustand auch ohne Farberkennung.
- Verbindungs-, Prozess-, Sensor-, Controller- und Safety-Zustände sind voneinander getrennt. Ein „grüner“ Gesamtzustand darf keinen unbekannten Teilzustand verdecken.
- Datenstatus kennt mindestens: aktuell, veraltet, nicht empfangen, inkompatibler Typ, Endpoint fehlt und deaktiviert.
- MiR- und MuR-Batterien zeigen Prozent, Quelle und Alter getrennt an. Schwellwerte sind konfigurierbar. Soweit geliefert, stehen Ladestatus, Spannung, Strom, Leistung, Restkapazität und Laufzeit als Details zur Verfügung. Schätzwerte sind entsprechend gekennzeichnet.
- Armstatus unterscheidet: nicht konfiguriert, Controller fehlt, inaktiv, aktiv, UR nicht bereit, Freedrive, Fehler und unbekannt.
- Alle Aktionen liefern eine sichtbare Rückmeldung mit Ziel, Ergebnis und Logreferenz. Lang laufende Aktionen haben Fortschritt/Timeout/Abbruch.
- Warnungen dürfen nicht nur im Log erscheinen, wenn sie einen Start oder eine Bewegung verhindern.

## 8. Stoppen und Sicherheit

- Die Jog-Ansichten zeigen einen gut sichtbaren Stop. Der zentrale Arm-Stop ist vom Hauptfenster aus erreichbar, sofern das Profil für die aktuelle Auswahl eine Arm-Stop-Schnittstelle bereitstellt.
- Zentraler Arm-Stop stoppt beide Arme aller im Bestätigungs-/Statusbereich aufgeführten ausgewählten Roboter. Scope ist vor Aktion sichtbar. Basis-Stop ist eine separate Aktion.
- Stop-Aktionen senden profilkonforme Null-/Cancel-Kommandos mehrfach oder mit ausreichender Gültigkeitsdauer und prüfen die Verbindung, soweit möglich.
- Bei Prozess-/ROS-Verbindungsverlust werden aktive GUI-Kommandos beendet; nach Wiederverbindung erfolgt keine automatische Wiederaufnahme.
- Unter **Prozesse** kann die Bedienperson einen eigenen Launch, einen ganzen Startablauf oder die von der App verwalteten Prozesse der Sitzung stoppen. Extern gestartete Prozesse bleiben davon ausgenommen.
- Schließen der GUI stoppt von ihr gestartete Bewegungs-/Launchprozesse kontrolliert. Externe ROS-Graphen bleiben erhalten. Beim Verlassen mit laufender Hardwarebewegung erscheint ein klarer Hinweis und die GUI versucht Stop vor dem Schließen.
- UI-Stop ist kein Sicherheits- oder Not-Aus. Beschriftung und Dokumentation müssen das eindeutig machen.
- Bewegungswerte werden profilseitig begrenzt; UI-Werte allein sind keine Sicherheitsgrenze. Robotercontroller müssen selbst Watchdog-/Grenzfunktionen besitzen.

## 9. Konfiguration und Persistenz

- Kein Pfad darf `/home/<bestimmter_nutzer>/...` voraussetzen.
- Lokaler Workspace kann aus installierter Umgebung/ROS-Paketpfaden ermittelt oder explizit ausgewählt werden. Remote-Workspace wird pro Hostprofil konfiguriert.
- Einstellungen, Logs, gespeicherte Ziele und temporäre Prozessdaten liegen unter benutzerbeschreibbaren XDG-Verzeichnissen oder einem gewählten Datenordner, nicht im installierten Source-Tree.
- Die App stellt die zuletzt gewählte Betriebsart, das zuletzt verwendete Preset, den Zielroboter und die gespeicherten Einstellungen wieder her. Nach dem Start bleibt die Sitzung getrennt, bis der Nutzer „Verbinden“ wählt. Starten und Bedienfreigabe bleiben eigene Aktionen.
- Presets können benannt, schnell ausgewählt, exportiert und importiert werden. Sie bündeln Betriebsart, Betriebsprofil, Verbindungsziel, Roboter und Visualisierung. Beim Import lassen sich rechnerabhängige Pfade und Zugänge vor Ort zuordnen. Zugangsdaten werden nicht exportiert.
- Persönliche Einstellungen wie Fensterlayout und zuletzt verwendete Logfilter werden separat gespeichert und beim Neustart wiederhergestellt. Die App merkt sich pro Preset die zuletzt verwendete Ansicht und stellt sie beim erneuten Öffnen wieder her. Ein erstmals verwendetes Preset öffnet die **Übersicht**.
- Profilfelder sind validiert; Geheimnisse (Passwörter, private Schlüsselmaterialien) werden nicht im Klartext gespeichert.
- Konfigurationen sind versioniert und migrierbar. Bei Fehlern bleibt ein minimaler Start mit Profilwahl/Diagnose möglich.
- Start- und Sitzungskonfiguration lassen sich exportieren, um Fehlerberichte reproduzierbar zu machen; sensible Werte werden ausgelassen.

## 10. Protokollierung und Diagnose

- Strukturierte Logs mit Zeit, Level, Modus, Host, Roboter, Komponente und Nachricht.
- Logs zeigen `stdout` und `stderr` der über die App gestarteten Prozesse sowie verfügbare Live-Meldungen aus dem verbundenen ROS-Graphen über `/rosout`. Fremde Prozesse ohne erreichbare Logquelle erscheinen mit entsprechendem Hinweis.
- Die Gesamtansicht führt Meldungen zeitlich zusammen. Jeder gestartete Launch hat zusätzlich einen eigenen Logtab. Bei einem Neustart bleibt nachvollziehbar, zu welchem Lauf eine Meldung gehört. Zwei Logs sollen zum Vergleichen parallel sichtbar sein können.
- Die Logansicht lässt sich nach Schweregrad und Quelle filtern und im Text durchsuchen. Tabs haben eigene Filter. Die aufgezeichneten Meldungen bleiben für die Diagnose erhalten.
- Logs können als vollständige Aufzeichnung oder als Ausschnitt der ausgewählten, gefilterten Ansicht exportiert werden. Der Logpfad ist in der Sitzung sichtbar.
- Diagnoseansicht listet erkannte ROS-Endpunkte (Name, Typ, Publisher/Subscriber, Status), verwendete Mapping-Regel, Controllerzustand und Zeitquelle.
- Prozesse zeigen Startkommando ohne Geheimnisse, Laufzeit, Exitcode und verknüpfte Ausgaben.
- Log-/Statusdaten, die aus Simulation stammen, enthalten Profilmarkierung, damit sie nicht mit Hardwareprotokollen verwechselt werden.

## 11. Konfigurationsmodell

Ein Profil beschreibt mindestens:

```yaml
id: mur620_sim
mode: simulation
target:
  host: local
  ros_domain_id: 62
  workspace: auto
time:
  use_sim_time: true
robots:
  - name: mur620a
    base:
      cmd_topic: /mur620a/mobile_base_controller/cmd_vel
      cmd_type: geometry_msgs/msg/TwistStamped
      odom_topic: /mur620a/mobile_base_controller/odom
    arm:
      left:
        twist_topic: /mur620a/jparse_velocity_controller_l/twist_cmd
      right:
        twist_topic: /mur620a/jparse_velocity_controller_r/twist_cmd
    controller_manager: /mur620a/controller_manager
    joint_states_topic: /joint_states
processes:
  simulator_launch: null # oder konfigurierter Launch-Prozess
visualization:
  rviz_config: null
```

Das Beispiel skizziert Konfigurationsdaten, ist kein final festgelegtes Dateiformat. Profilvalidierung muss ROS-Nachrichtentypen, benötigte Stop-Schnittstellen und unterstützte Funktionen prüfen. Ein Topic-Mapping wird erst aktiv, wenn Name und Nachrichtentyp übereinstimmen.

## 12. Qualitäts- und Bedienanforderungen

- ROS-Verbindungen und Prozessstarts blockieren nie den UI-Thread.
- Verlust eines Topics oder Hosts wird zeitnah gemeldet; Datenalter ist sichtbar.
- Kein automatischer Start von Hardwarebewegung nach Anwendungsstart, Verbindungsaufbau oder Profilwechsel.
- Kein ausgewählter Roboter: keine Bewegung; UI fordert explizite Auswahl.
- Mehrere ausgewählte Roboter: Bewegungen werden standardmäßig deaktiviert, bis Nutzer gezielt einen Steuerroboter wählt. Gruppenaktionen sind auf Start/Status/Stop begrenzt und zeigen alle Zielgeräte.
- Jede bewegende Aktion ist über Maus und Tastatur erreichbar und hat dokumentierte Einheiten.
- Dialoge sind per Tastatur bedienbar und melden Status mit Text und Farbe. Das Hauptfenster unterstützt maximierte Ansicht und Vollbild.
- Lokale UI bleibt funktionsfähig, wenn optionale ROS-Pakete/Module fehlen; betroffene Funktionen werden als nicht installiert markiert.

## 13. Abnahmekriterien

1. Eine Sitzung kann mit einem bereits laufenden lokalen oder Remote-Simulationsgraphen verbunden werden und erkennt Domain, Clock, Roboter und verfügbare Controller.
2. Die GUI kann einen konfigurierten Simulationsstart ausführen, seinen Status/Logs zeigen und nur diesen GUI-eigenen Prozess stoppen.
3. Simulations-Basisjog verwendet den im Profil erkannten TwistStamped-Controller und sendet bei Loslassen, Fokusverlust, Stop und Trennung Null-Kommandos.
4. Ein fehlender oder typinkompatibler Simulationsendpunkt wird als nicht verfügbar angezeigt; es wird kein Hardwaretopic-Fallback verwendet.
5. Hardwareverbindung kann geprüft werden, ohne Hardware zu starten. Hardwarestart läuft erst nach Host-/UR-Preflight und zeigt Safety-Blocker mit Details.
6. Ein Dashboard-Safety-Stop verhindert Start, bis Nutzer die Meldung explizit quittiert; bei fehlgeschlagenem Preflight ist Standardentscheidung Abbruch.
7. Jog-, Stop-, Home-, Freedrive-, Lift- und Alignment-Aktionen erscheinen nur, wenn das Profil und die Laufzeitprüfung sie unterstützen.
8. Ein von außen gestarteter ROS-Prozess bleibt beim Stoppen oder Schließen der GUI unangetastet.
9. Profile, Pose-Datei und Logs funktionieren für beliebige lokale Nutzernamen ohne fest kodierte Home-Pfade oder Schreibzugriff auf Source-Repositories.
10. Logexport erlaubt die Rekonstruktion von Profil, Zielhost, ausgewähltem Roboter, erkanntem ROS-Graph, Prozessresultat und Fehlerursache, ohne Zugangsdaten einzuschließen.
11. Simulation und Hardware sind in Fensterkopf, Status, Logs und RViz-Auswahl eindeutig markiert.
12. Nach einem Neustart sind Betriebsart, Preset und Roboter wieder ausgewählt und die zuletzt verwendete Ansicht ist geöffnet. Die App baut erst nach „Verbinden“ eine Verbindung auf.
13. Ein exportiertes Preset lässt sich auf einem zweiten Rechner importieren. Rechnerabhängige Pfade und Zugänge können dort zugeordnet werden.
14. Gazebo kann ohne Gazebo-Fenster starten. ROS-Visualisierung und Simulatorlogs bleiben nutzbar. Bestätigt der Prototyp das getrennte Ausführen, lässt sich das Gazebo-Fenster öffnen, schließen und erneut öffnen, während Weltzustand und Simulationszeit fortlaufen.
15. Die ROS-Visualisierung und zwei parallele Launch-Logs können auf einem oder mehreren Monitoren betrachtet werden. Abdocken und erneutes Andocken erhalten die laufende Sitzung und den Ansichtsinhalt.
16. Die App lässt sich auf Ubuntu 24.04 aus einem `.deb` installieren und aus dem App-Menü öffnen.
17. Ein definierter Startablauf startet die zugehörigen Launches mit einer Aktion. Jeder Launch zeigt Zustand und eigenes Log und kann für gezielte Fehlersuche einzeln gestartet werden.
18. Nach einem Wechsel des Zielsystems zeigen Hauptfenster und abgedockte Ansichten denselben neuen Sitzungskontext.
19. Ein externer Launch bleibt auch beim Start eines eigenen Startablaufs erhalten.
20. Bei einem Profil mit Navigationsabbruch oder MiR-Leuchten erscheint die Aktion mit Rückmeldung und Zielroboter. Bei einem Profil ohne diese Schnittstelle erklärt die App die fehlende Verfügbarkeit.
21. Eine vorhandene MiR-Posendatei kann übernommen werden. Lift-Höhen und Batteriedetails zeigen fehlende oder veraltete Daten als solche.

### Erster lauffähiger Stand

Der erste durchgängige Ablauf beginnt mit dem lokalen Simulationspreset. Die App stößt die zugehörigen Prozesse über **Simulation starten** an, zeigt RViz und Launch-Logs und kann die gestartete Simulation wieder stoppen. Das Hardwareprofil bietet zunächst Verbindung, Status und Diagnose. Danach folgen die Bedienbereiche schrittweise, beginnend mit der Basissteuerung in der Simulation.

## 14. Vorschläge für frühe Prototypen

| Prototyp | Varianten und Prüffrage |
|---|---|
| **A · Layout mit Beispieldaten** | **Versuchsfeld** stellt Roboterstatus und Bedienung in den Vordergrund. **Entwicklung** gibt der Visualisierung den meisten Platz und hält mehrere Launch-Logs griffbereit. **Geteilte Ansicht** zeigt zwei gleichwertige Bereiche, etwa RViz und Logs. In allen Varianten schaltet man Betriebsart und Preset um, findet eine konkrete Warnung und löst eine Ansicht aus dem Hauptfenster. |
| **B · Vollständiges RViz abdocken** | Kleine Desktop-App auf Ubuntu 24.04 mit ROS 2 Jazzy und dem RViz-`VisualizationFrame`. Prüfen, ob die ganze RViz-Oberfläche in der App funktioniert, ob sich eine `.rviz`-Datei laden lässt, ob ein absichtlich fehlender TF sichtbar gemeldet wird und ob beim Abdocken sowie beim Wechsel zwischen zwei Testkontexten die erwarteten Daten und Werkzeuge erhalten bzw. korrekt neu geladen werden. |
| **C · Gazebo ohne Fenster** | Mit der vorhandenen MuR-Simulation prüfen, ob sich der Gazebo-Client während der Sitzung öffnen und schließen lässt, während der Server und `/clock` weiterlaufen. Eine eingebettete Weltansicht separat mit parallelem RViz ausprobieren. Für Sensoren auf einem Rechner ohne Displayserver ist EGL-basiertes Headless Rendering ein eigener Prüfpunkt. |

### Technikvergleich für Prototyp B

**Gewählter Aufbau:** C++ und Qt 5 Widgets für das Hauptfenster, die Docks und natives RViz. Die MuR-Ansichten entstehen in QML. Bestehende Python-ROS-Nodes lassen sich über ihre ROS-Schnittstellen weiterverwenden. [Architekturentscheidung](adr/0001-qt-desktop-mit-rviz-und-qml.md)

Die wichtigste Anforderung für diesen Vergleich ist vollständiges RViz als abdockbare Ansicht in einer eigenständigen MuR-App. Bewertet werden außerdem die Gestaltungsmöglichkeiten, der Integrationsaufwand und die zusätzlichen Abhängigkeiten des Ubuntu-Pakets.

| Aufbau | RViz und Fenster | Gestaltung und Aufwand | Bewertung |
|---|---|---|---|
| **C++ und Qt 5 Widgets** | Die App bindet den vollständigen `VisualizationFrame` nativ ein. Ein eigenes `QMainWindow` verwaltet die Ansichten und ihre Docks. | Eigene Komponenten, Layouts und Gestaltung über Qt. Qt 5 und RViz Jazzy liegen bereits im Workspace vor. | **Empfohlene Basis.** Der direkteste Weg zu einer MuR-App mit vollständigem RViz. [RViz VisualizationFrame](https://github.com/ros2/rviz/blob/jazzy/rviz_common/include/rviz_common/visualization_frame.hpp) |
| **Qt 5 Widgets mit QML** | Behält das native RViz und die Docks. Die QML-Ansichten laufen daneben in einem `QQuickWidget`. | Deklaratives UI mit frei gestaltbaren Komponenten, ohne zusätzliche Web-Engine. Das Zusammenspiel beim Rendern und Abdocken ist im Prototyp zu prüfen. | **Gewählt** für die eigenen Ansichten wie Übersicht, Presets und Prozessstatus. [Qt 5 QQuickWidget](https://github.com/qt/qtdeclarative/blob/5.15/src/quickwidgets/qquickwidget.cpp) |
| **Qt 5 Widgets mit einer TypeScript-Oberfläche** | RViz bleibt im nativen Dock. Andere Ansichten laufen in einem `QWebEngineView` daneben. `QWebChannel` verbindet sie mit dem App-Kern. | HTML und CSS bieten hohe Gestaltungsfreiheit und vorhandene Web-Komponenten. Das Paket braucht zusätzlich Qt WebEngine und der App-Kern eine Schnittstelle zur Oberfläche. Die Dateien werden lokal geladen. | **Sinnvolle Alternative**, wenn TypeScript für das Team einen konkreten Vorteil bringt. [Qt 5 QWebEngineView](https://github.com/qt/qtwebengine/blob/5.15/src/webenginewidgets/api/qwebengineview.h), [Qt 5 QWebChannel-Transport](https://github.com/qt/qtwebengine/blob/5.15/src/webenginewidgets/api/qwebenginepage.cpp) |
| **PyQt5 mit nativer RViz-Anbindung** | Die vorhandene App-Technik lässt sich weiterverwenden. Für den vollständigen C++-`VisualizationFrame` wäre eine eigene passende Python-Anbindung nötig. | Nutzen durch Wiederverwendung bestehender UI-Logik. Bindings, Objektlebensdauer und Paketierung verursachen Zusatzarbeit. | **Nur bei klarer Priorität für Python im Hauptfenster.** Bestehende ROS-Funktionen können weiterhin als Python-Nodes verwendet werden. |
| **MuR-Erweiterungen als RViz-Panels** | RViz ist das Hauptfenster. Die MuR-Funktionen laufen als Panels in RViz. | Der schnellste Weg zu einem RViz-nahen Bedienwerkzeug. Navigation und Sitzung sind an die RViz-App gebunden. | **Für ein kleines RViz-Werkzeug geeignet.** Für den eigenständigen MuR-Arbeitsbereich passt die umgekehrte Einbettung besser. [RViz Jazzy: Panel-Plugins](https://github.com/ros2/rviz/blob/jazzy/docs/plugin_development.md) |
| **Tauri, Electron oder andere Desktop-Technik mit separatem RViz** | RViz bleibt als eigenes Fenster voll nutzbar. Es ist kein Dock der MuR-App. | Freie Wahl der App-Technik. Die App kann Start, Profil und `.rviz`-Datei des externen Prozesses koordinieren. | **Rückfalloption**, wenn separates RViz für den Alltag ausreicht. [Tauri-Architektur](https://v2.tauri.app/concept/architecture/), [Electron BrowserWindow](https://www.electronjs.org/docs/latest/api/browser-window) |
| **Qt 6 oder NodeGui als Host** | Das hier installierte RViz Jazzy verwendet die Qt-5-Widget-API. Für native Einbettung in Qt 6 wäre eine dafür gebaute und kompatible RViz-Variante nötig. | Moderner Qt-Stack bzw. native Widgets über JavaScript, dafür zusätzlicher Aufwand für eigene Builds oder getrennte Prozesse. | **Für Jazzy zurückstellen.** [RViz Jazzy: Build-Abhängigkeiten](https://github.com/ros2/rviz/blob/jazzy/rviz_common/CMakeLists.txt), [NodeGui](https://github.com/nodegui/nodegui) |
| **Andere ROS-Visualisierung** | Eine eigene Ansicht oder ein bestehender Viewer wie Lichtblick kann ROS-Daten mit eigenen Panels darstellen. RViz-Plugins und `.rviz`-Layouts sind eigene Anforderungen. | Viel Freiheit für gezielte Arbeitsabläufe. Ein vorhandener Viewer spart gegenüber einem Neubau Aufwand. Die MuR-Funktionen brauchen weiterhin eine Anbindung. | **Spätere wählbare Ansicht**, wenn weniger Displays für einen konkreten Ablauf besser funktionieren. [Lichtblick: ROS 2](https://lichtblick-suite.github.io/docs/docs/connecting-to-data/frameworks/ros2/) |

Plattformabhängiges Einhängen eines fremden Fensters und Streaming einer gerenderten RViz-Oberfläche bleiben außerhalb der bevorzugten Lösungen. Sie bringen zusätzliche Anforderungen an Grafik, Eingabefokus und Fensterverwaltung mit. Der Plattformweg müsste auf Ubuntu mit dem vorgesehenen Displayserver geprüft werden. [Qt: Fremde Fenster](https://doc.qt.io/qt-6.8/qwindow.html#fromWinId)

Der technische Prototyp verwendet Qt 5 Widgets und vollständiges RViz. Erfolgreich ist er, wenn eine `.rviz`-Konfiguration mit Displaybaum, sichtbarem TF-Fehler und Werkzeugen nach dem Ab- und Wiederandocken bedienbar bleibt. Daneben läuft eine Logansicht, die sich separat abdocken lässt. Der Layout-Prototyp erprobt mit QML und Beispieldaten die Navigation und Sichtbarkeit der Abläufe. Sobald beide Grundlagen stehen, wird eine QML-Ansicht neben RViz im selben Fenster erprobt.

## 15. Paketierung und Releases

Das Installationsziel ist ein `.deb` für Ubuntu 24.04 mit ROS 2 Jazzy. Vorgeschlagen ist ein GitHub-Actions-Build für Änderungen auf `main`, der das Paket als Build-Artefakt bereitstellt. Ein ausgewählter Versions-Tag wie `v0.1.0` soll ein GitHub Release mit dem passenden Paket auslösen. Ob die Version per Tag oder durch einen eigenen Release-Workflow vergeben wird, ist noch offen.

## 16. Repository-Bezug und verifizierte Ausgangslage

Der Funktionsumfang wurde mit der [Bestandsaufnahme](current_gui_spec.md), der ausgecheckten Basis-GUI (`af564e5`), dem lokal vorhandenen neueren `main` (`d04ad28`) und dem optionalen Mocap-Modul im Workspace abgeglichen. `main` ergänzt unter anderem die Lokalisierungswahl, den Navigationsabbruch, MiR-Kameras, Batteriedetails, den gemeinsamen Einstellungsspeicher und das kontrollierte UR-Herunterfahren. Der Vergleich erfasst den Quellcode. Die Verfügbarkeit von Funktionen an der realen Hardware und in den jeweiligen Launches ist separat zu prüfen.

| Vorhandener Funktionsbereich | Abdeckung im Rewrite |
|---|---|
| Roboter, Arme, MiR und Host auswählen. SSH-Verbindung, Host-Prüfung, Code-Sync, Build und Hardwarestart. | [Sitzung](#3-betriebs--und-verbindungskontext), [Hardwareprofil](#62-hardware--versuchsfeld) und [Startabläufe](#startabläufe) |
| MiR-Jog, Leuchten, Ziel senden, aktuelle oder eingegebene Pose speichern, gespeicherte Posen verwalten und Navigation abbrechen. | [Basissteuerung](#51-basissteuerung) und [Ziele und Posen](#53-ziele-und-posen) |
| UR-Bereitschaft, Arm-Jog, Lift, Home, Freedrive, TCP-Ausrichtung, Arm-Stop und kontrolliertes Herunterfahren. | [Armsteuerung](#52-armsteuerung), [Hardwareprofil](#62-hardware--versuchsfeld) und [Stoppen und Sicherheit](#8-stoppen-und-sicherheit) |
| Batterien, Verbindung, Arm-/Controllerzustand, Reverse-Interface und Laufzustand. | [Übersicht](#übersicht) und [Statusanzeigen](#7-statusanzeigen-und-meldungen) |
| RViz starten, laufende Prozesse und ihre Logs sehen, gefilterten Ausschnitt speichern und verwaltete Prozesse stoppen. | [Visualisierung](#visualisierung), [Logs](#10-protokollierung-und-diagnose) und [Prozess-Stop](#8-stoppen-und-sicherheit) |
| Persönliche Einstellungen, Vollbild und Ubuntu-Anwendungseintrag. | [Persistenz](#9-konfiguration-und-persistenz), [Bedienanforderungen](#12-qualitäts--und-bedienanforderungen) und [Paketierung](#15-paketierung-und-releases) |
| Qualisys/Mocap-Ansicht und Bridge sowie weitere separat startbare MuR-Anwendungen. | [Erweiterungen](#54-erweiterungen). Mocap ist beschrieben. Der Umfang der übrigen Anwendungen bleibt offen. |

Für Simulationsmapping wurden außerdem folgende Workspace-Dateien herangezogen:

- `match_mobile_robotics_jazzy/mur_robot/mur_launch_sim/launch/mur620.launch.py`
- `match_mobile_robotics_jazzy/mur_robot/mur_launch_sim/launch/mur_base.launch.py`
- `match_mobile_robotics_jazzy/mir_robot/mir_description/config/mur_controllers.yaml`
- `match_mobile_robotics_jazzy/mur_robot/mur_launch_sim/src/gz_ground_truth_publisher.cpp`

Die dort aufgeführten Simulationsnamen sind aktuelle Kandidaten aus dem Repository. Die neue GUI muss sie beim Verbinden validieren, da Launchargumente, Namensräume und installierte Controller je nach Simulationsprofil variieren können.

## 17. Offene Produktentscheidungen

- **Startablauf:** Wann gilt ein Schritt als bereit, was geschieht bei einem Fehler und wo pflegt das Team die Startschritte?
- **Presetwechsel:** Was geschieht mit eigenen Prozessen, die beim Wechsel des Zielsystems noch laufen?
- **Erweiterungen:** Welche Funktionen aus Navigation, OAK und Cooperative Handling sollen Teil des Command Centers werden? Welchen Einsatz soll eine eigene ROS-Datenansicht zuerst verbessern?
- **Machbarkeit:** Vollständiges RViz beim Abdocken und Gazebo-Weltansicht am laufenden Simulator werden in den beschriebenen Prototypen geprüft.
