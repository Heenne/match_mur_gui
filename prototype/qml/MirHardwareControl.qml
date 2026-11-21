import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtQuick.Dialogs 1.3 as NativeDialogs

Item {
    id: panel
    implicitWidth: 720
    implicitHeight: 470
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
    function stopJog(reason) {
        panel.heldKey = -1
        mirHardwareControl.stop(reason)
    }

    onActiveFocusChanged: {
        if (!activeFocus && heldKey !== -1) stopJog("keyboard focus lost")
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Escape) {
            panel.stopJog("keyboard stop")
            event.accepted = true
            return
        }
        var direction = panel.directionForKey(event.key)
        if (direction !== "") {
            if (!event.isAutoRepeat) {
                panel.heldKey = event.key
                mirHardwareControl.press(direction)
            }
            event.accepted = true
        }
    }
    Keys.onReleased: {
        if (event.key === panel.heldKey) {
            panel.heldKey = -1
            mirHardwareControl.release()
            event.accepted = true
        }
    }

    Component.onCompleted: {
        forceActiveFocus()
        mirHardwareControl.setContext(session.robot, session.mode, session.target,
            session.connectionState === "Connected")
    }
    Connections {
        target: session
        function onChanged() {
            mirHardwareControl.setContext(session.robot, session.mode, session.target,
                session.connectionState === "Connected")
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
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Label { text: "MiR base and lights"; color: panel.foreground; font.pixelSize: 20; font.bold: true }
                Item { Layout.fillWidth: true }
                Label { text: "/" + session.robot + " · " + session.target; color: panel.muted }
            }
            Label {
                Layout.fillWidth: true
                text: "Jog accepts only a connected Hardware session with a verified /cmd_vel (Twist) or /cmd_vel_stamped (TwistStamped) subscriber. Maximum 0.15 m/s and 0.4 rad/s."
                color: panel.muted
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Switch {
                    id: armSwitch
                    text: checked ? "MiR jog armed" : "Explicitly arm MiR jog"
                    checked: mirHardwareControl.armed
                    enabled: session.mode === "Hardware" && session.connectionState === "Connected"
                    onToggled: mirHardwareControl.setArmed(checked)
                    Accessible.name: text
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "STOP"
                    enabled: mirHardwareControl.holding
                    onClicked: panel.stopJog("Stop button")
                    Accessible.name: "Stop MiR jog"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Linear · " + mirHardwareControl.linearSpeed.toFixed(2) + " m/s"; color: panel.muted }
                Slider {
                    Layout.fillWidth: true
                    from: 0.01; to: 0.15; value: mirHardwareControl.linearSpeed
                    enabled: armSwitch.checked
                    onMoved: mirHardwareControl.setLinearSpeed(value)
                }
                Label { text: "Turn · " + mirHardwareControl.angularSpeed.toFixed(2) + " rad/s"; color: panel.muted }
                Slider {
                    Layout.fillWidth: true
                    from: 0.05; to: 0.4; value: mirHardwareControl.angularSpeed
                    enabled: armSwitch.checked
                    onMoved: mirHardwareControl.setAngularSpeed(value)
                }
            }
            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                columns: 3
                rowSpacing: 6
                columnSpacing: 8
                Item { width: 104; height: 42 }
                HoldButton { text: "Forward"; direction: "forward" }
                Item { width: 104; height: 42 }
                HoldButton { text: "Turn left"; direction: "left" }
                HoldButton { text: "Backward"; direction: "backward" }
                HoldButton { text: "Turn right"; direction: "right" }
            }
            Label {
                Layout.fillWidth: true
                text: mirHardwareControl.status
                color: mirHardwareControl.holding ? "#45d6a8" : panel.muted
                Accessible.name: mirHardwareControl.status
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2b3c47" }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Lights"; color: panel.foreground; font.bold: true }
                Button { text: "Rainbow"; enabled: session.mode === "Hardware" && session.connectionState === "Connected"; onClicked: mirHardwareControl.rainbowStart() }
                Button { text: "Stop effect"; enabled: session.mode === "Hardware" && session.connectionState === "Connected"; onClicked: mirHardwareControl.rainbowStop() }
                Button { text: "MATCH color"; enabled: session.mode === "Hardware" && session.connectionState === "Connected"; onClicked: mirHardwareControl.matchColor() }
                Button { text: "Solid color…"; enabled: session.mode === "Hardware" && session.connectionState === "Connected"; onClicked: colorDialog.open() }
            }
            Label {
                Layout.fillWidth: true
                text: mirHardwareControl.lightStatus
                color: mirHardwareControl.lightStatus.indexOf("Applied") === 0 ? "#45d6a8" : panel.muted
                wrapMode: Text.WordWrap
                Accessible.name: mirHardwareControl.lightStatus
            }
        }
    }

    component HoldButton: Rectangle {
        required property string text
        required property string direction
        width: 104
        height: 42
        radius: 8
        color: holdArea.pressed ? "#397f80" : "#243640"
        border.color: "#48616d"
        Label { anchors.centerIn: parent; text: parent.text; color: panel.foreground; font.bold: true }
        MouseArea {
            id: holdArea
            anchors.fill: parent
            enabled: mirHardwareControl.armed
            acceptedButtons: Qt.LeftButton
            onPressed: mirHardwareControl.press(parent.direction)
            onReleased: mirHardwareControl.release()
            onCanceled: mirHardwareControl.stop("pointer canceled")
        }
    }

    NativeDialogs.ColorDialog {
        id: colorDialog
        title: "MiR solid color"
        onAccepted: mirHardwareControl.setSolidColor(
            Math.round(selectedColor.r * 255),
            Math.round(selectedColor.g * 255),
            Math.round(selectedColor.b * 255))
    }
}
