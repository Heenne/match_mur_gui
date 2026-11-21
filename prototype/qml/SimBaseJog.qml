import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Item {
    id: panel
    implicitWidth: 520
    implicitHeight: 390
    focus: true
    activeFocusOnTab: true
    property color panelColor: "#1b2933"
    property color foreground: "#f2f6f8"
    property color muted: "#a8bac5"
    property int heldKey: -1

    function directionForKey(key) {
        if (key === Qt.Key_Up) return "forward"
        if (key === Qt.Key_Down) return "backward"
        if (key === Qt.Key_Left) return "left"
        if (key === Qt.Key_Right) return "right"
        return ""
    }

    onActiveFocusChanged: {
        if (!activeFocus && heldKey !== -1) stopJog("keyboard focus lost")
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Escape) {
            panel.heldKey = -1
            panel.stopJog("keyboard stop")
            event.accepted = true
            return
        }
        var direction = panel.directionForKey(event.key)
        if (direction !== "") {
            if (!event.isAutoRepeat) {
                panel.heldKey = event.key
                simBaseJog.press(direction)
            }
            event.accepted = true
        }
    }
    Keys.onReleased: {
        if (event.key === panel.heldKey) {
            panel.heldKey = -1
            simBaseJog.release()
            event.accepted = true
        }
    }

    function stopJog(reason) {
        panel.heldKey = -1
        if (simBaseJog) simBaseJog.stop(reason)
    }

    Component.onCompleted: {
        forceActiveFocus()
        simBaseJog.setContext(session.robot, session.mode,
            session.preset + "|" + session.target, session.connectionState === "Connected", session.simCommandTopic)
    }
    Connections {
        target: session
        function onChanged() {
            simBaseJog.setContext(session.robot, session.mode,
                session.preset + "|" + session.target, session.connectionState === "Connected", session.simCommandTopic)
        }
    }
    Connections {
        target: Window.window
        function onActiveChanged() {
            if (!Window.window.active) panel.stopJog("window lost focus")
        }
    }
    Connections {
        target: Qt.application
        function onStateChanged(state) {
            if (state !== Qt.ApplicationActive) panel.stopJog("application lost focus")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: panel.panelColor
        radius: 12
        border.color: "#2b3c47"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12
            Label { text: "Simulation base jog"; color: panel.foreground; font.pixelSize: 20; font.bold: true }
            Label {
                Layout.fillWidth: true
                text: "Configured endpoint: " + (session.simCommandTopic || "not configured") + " · TwistStamped · max 0.15 m/s, 0.4 rad/s"
                color: panel.muted
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Switch {
                    id: enableSwitch
                    text: checked ? "Jog enabled" : "Explicitly enable jog"
                    checked: simBaseJog.enabled
                    enabled: session.mode === "Simulation" && session.connectionState === "Connected"
                    onToggled: simBaseJog.setEnabled(checked)
                }
                Item { Layout.fillWidth: true }
                Button { text: "STOP"; enabled: simBaseJog.holding; onClicked: panel.stopJog("Stop button") }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Linear · " + simBaseJog.linearSpeed.toFixed(2) + " m/s"; color: panel.muted }
                Slider {
                    Layout.fillWidth: true
                    from: 0.01
                    to: 0.15
                    value: simBaseJog.linearSpeed
                    onMoved: simBaseJog.setLinearSpeed(value)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Turn · " + simBaseJog.angularSpeed.toFixed(2) + " rad/s"; color: panel.muted }
                Slider {
                    Layout.fillWidth: true
                    from: 0.05
                    to: 0.4
                    value: simBaseJog.angularSpeed
                    onMoved: simBaseJog.setAngularSpeed(value)
                }
            }
            Label {
                text: "Keyboard: arrow keys hold direction · Space or Escape stops"
                color: panel.muted
            }
            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                columns: 3
                rowSpacing: 8
                columnSpacing: 8
                Item { width: 108; height: 56 }
                HoldButton { text: "Forward"; direction: "forward" }
                Item { width: 108; height: 56 }
                HoldButton { text: "Turn left"; direction: "left" }
                HoldButton { text: "Backward"; direction: "backward" }
                HoldButton { text: "Turn right"; direction: "right" }
            }
            Label {
                Layout.fillWidth: true
                text: simBaseJog.status
                color: simBaseJog.holding ? "#45d6a8" : panel.muted
                Accessible.name: simBaseJog.status
            }
        }
    }

    component HoldButton: Rectangle {
        required property string text
        required property string direction
        width: 108
        height: 56
        radius: 8
        color: holdArea.pressed ? "#397f80" : "#243640"
        border.color: "#48616d"
        Label { anchors.centerIn: parent; text: parent.text; color: panel.foreground; font.bold: true }
        MouseArea {
            id: holdArea
            anchors.fill: parent
            enabled: simBaseJog.enabled
            acceptedButtons: Qt.LeftButton
            onPressed: simBaseJog.press(parent.direction)
            onReleased: simBaseJog.release()
            onCanceled: simBaseJog.stop("pointer canceled")
        }
    }
}
