# Projektregeln

Für die Qt-Desktopoberfläche gelten die folgenden Designvorgaben. Neue und geänderte Komponenten verwenden die zentralen Werte aus `command_center/app_theme.cpp` und vorhandene QML-Komponenten. Bei dauerhaften Änderungen der Vorgaben wird diese Datei im selben Schritt aktualisiert.

- DM Sans ist die gemeinsame Schrift für Navigation, Text, Zahlen und Bedienelemente. Die lizenzierte Schrift liegt unter `assets/fonts`. Nutze 12 px für Metadaten, 14 px für Bedienelemente und Text, 16 px für Abschnittstitel, 20 px für Bereichsüberschriften und 24 px für Seitentitel. Gewichte sind 400, 500, 600 und 700.
- Abstände folgen einer 4-px-Skala mit 4, 8, 12, 16, 20, 24, 32, 40, 48, 64 und 80 px. Karten erhalten 20–24 px Innenabstand, auf schmalen Ansichten 16 px. Zusammengehörige Elemente stehen 8–12 px auseinander.
- Die zentralen Light- und Dark-Paletten in `app_theme.cpp` definieren Hintergrund, Karten, Formulare, Navigation, Text, Rahmen, Auswahl, Fokus, Badges, Fehler und Tooltips. MATCH-Grün `#B1CA21` markiert primäre Aktionen und aktive Navigation. Textbuttons verwenden neutrale Hoverflächen, mindestens 40 px Höhe und 6 px Rundung. Fokus bleibt sichtbar.
- Erfolg verwendet Kartenfläche, grüne Kante und ein Häkchen in Fokusfarbe. Orange `#DA7644` ist für die Zeitachse vorgesehen. Warn- und Fehlerzustände erhalten verständlichen Text oder Symbole.
- Titel, Theme- und Sprachwahl bleiben in der gemeinsamen Kopfzeile erreichbar. Layouts müssen auch bei langen Beschriftungen und schmalen Fenstern bedienbar bleiben. Felder benötigen dauerhafte Labels und feldbezogene Validierung. Dialoge ordnen Abbrechen links und die primäre Aktion rechts an.
- Stelle Tastaturbedienung, zugängliche Namen und lesbare Kontraste sicher. Pflege übersetzte Hinweise zentral. Tooltips öffnen nach 800 ms Hover und schließen bei Escape oder Interaktion.
- Pflege betroffene Tests bei Änderungen und prüfe Desktop und schmale Fenster visuell mit repräsentativen Zuständen. Verwende isolierte Testdaten. Führe die vollständige Suite auf Wunsch oder bei grundlegenden Änderungen aus und nenne ausgeführte und ausstehende Prüfungen.
