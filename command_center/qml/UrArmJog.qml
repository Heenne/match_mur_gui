import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

FocusScope {
    implicitHeight: controls.implicitHeight + 36
    id: panel
    property var controller
    focus: true
    onActiveFocusChanged: if (!activeFocus && controller) controller.stop("focus lost")

    Rectangle {
        anchors.fill: parent
        color: theme.surface
        radius: 12
        border.color: theme.line
    }

    ColumnLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("UR arm control"); color: theme.ink; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: sideChoice
                model: ["UR10_r", "UR10_l"]
                currentIndex: controller && controller.selectedArm === "UR10_l" ? 1 : 0
                onActivated: if (controller) controller.selectArm(currentIndex === 1 ? "l" : "r")
                Accessible.name: qsTr("Selected UR arm")
            }
        }

        Label {
            Layout.fillWidth: true
            text: controller ? controller.readiness : qsTr("No UR arm controller")
            color: controller && controller.ready ? theme.focus : theme.warning
            font.family: "monospace"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Translation ≤ 0.05 m/s"); color: theme.muted }
                Slider {
                    id: linear
                    Layout.fillWidth: true
                    from: 0.001; to: 0.05; value: controller ? controller.linearSpeed : 0.01
                    onMoved: if (controller) controller.setLinearSpeed(value)
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Rotation ≤ 0.20 rad/s"); color: theme.muted }
                Slider {
                    id: angular
                    Layout.fillWidth: true
                    from: 0.01; to: 0.20; value: controller ? controller.angularSpeed : 0.05
                    onMoved: if (controller) controller.setAngularSpeed(value)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: controller && controller.armed ? qsTr("Disarm") : qsTr("Arm selected UR arm")
                enabled: controller && (controller.ready || controller.armed)
                onClicked: if (controller) controller.setArmed(!controller.armed)
                Accessible.name: qsTr("Explicitly arm selected UR arm jog")
            }
            Label { Layout.fillWidth: true; text: controller ? controller.status : ""; color: theme.muted; wrapMode: Text.WordWrap }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 8
            color: theme.dangerSurface
            border.color: theme.danger
            Button {
                anchors.fill: parent
                text: controller ? "STOP · " + controller.selectedArm : qsTr("STOP ARM JOG")
                enabled: controller && (controller.holding || controller.armed)
                onClicked: if (controller) controller.stop("operator stop")
                background: Rectangle { color: "transparent" }
                contentItem: Text {
                    text: parent.text
                    color: theme.ink
                    font.bold: true
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                Accessible.name: qsTr("Stop selected UR arm jog immediately")
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 6
            rowSpacing: 8
            columnSpacing: 8
            Repeater {
                model: [
                    {label: "X−", axis: "tx-"}, {label: "X+", axis: "tx+"},
                    {label: "Y−", axis: "ty-"}, {label: "Y+", axis: "ty+"},
                    {label: "Z−", axis: "tz-"}, {label: "Z+", axis: "tz+"},
                    {label: qsTr("Rx−"), axis: "rx-"}, {label: qsTr("Rx+"), axis: "rx+"},
                    {label: qsTr("Ry−"), axis: "ry-"}, {label: qsTr("Ry+"), axis: "ry+"},
                    {label: qsTr("Rz−"), axis: "rz-"}, {label: qsTr("Rz+"), axis: "rz+"}
                ]
                Button {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    text: modelData.label
                    enabled: controller && controller.armed && controller.ready
                    onPressed: if (controller) controller.press(modelData.axis)
                    onReleased: if (controller) controller.release()
                    onCanceled: if (controller) controller.stop("button cancelled")
                    Accessible.name: qsTr("Hold to jog %1").arg(modelData.label)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Hold a direction to move. Releasing, losing focus, changing arms, or disconnecting stops the command. Only this arm is addressed.")
            color: theme.muted
            wrapMode: Text.WordWrap
        }
        Timer {
            interval: 500
            repeat: true
            running: controller !== null
            onTriggered: controller.refresh()
        }
    }
}
