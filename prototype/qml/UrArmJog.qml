import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

FocusScope {
    id: panel
    property var controller
    focus: true
    onActiveFocusChanged: if (!activeFocus && controller) controller.stop("focus lost")

    Rectangle {
        anchors.fill: parent
        color: "#1b2933"
        radius: 12
        border.color: "#2b3c47"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: "UR arm control"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: sideChoice
                model: ["UR10_r", "UR10_l"]
                currentIndex: controller && controller.selectedArm === "UR10_l" ? 1 : 0
                onActivated: if (controller) controller.selectArm(currentIndex === 1 ? "l" : "r")
                Accessible.name: "Selected UR arm"
            }
        }

        Label {
            Layout.fillWidth: true
            text: controller ? controller.readiness : "No UR arm controller"
            color: controller && controller.ready ? "#45d6a8" : "#f0b44d"
            font.family: "monospace"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Translation ≤ 0.05 m/s"; color: "#a8bac5" }
                Slider {
                    id: linear
                    Layout.fillWidth: true
                    from: 0.001; to: 0.05; value: controller ? controller.linearSpeed : 0.01
                    onMoved: if (controller) controller.setLinearSpeed(value)
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Rotation ≤ 0.20 rad/s"; color: "#a8bac5" }
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
                text: controller && controller.armed ? "Disarm" : "Arm selected UR arm"
                enabled: controller && (controller.ready || controller.armed)
                onClicked: if (controller) controller.setArmed(!controller.armed)
                Accessible.name: "Explicitly arm selected UR arm jog"
            }
            Label { Layout.fillWidth: true; text: controller ? controller.status : ""; color: "#a8bac5"; wrapMode: Text.WordWrap }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 8
            color: "#532c31"
            border.color: "#f07178"
            Button {
                anchors.fill: parent
                text: controller ? "STOP · " + controller.selectedArm : "STOP ARM JOG"
                enabled: controller && (controller.holding || controller.armed)
                onClicked: if (controller) controller.stop("operator stop")
                background: Rectangle { color: "transparent" }
                contentItem: Text {
                    text: parent.text
                    color: "#fff0f0"
                    font.bold: true
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                Accessible.name: "Stop selected UR arm jog immediately"
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
                    {label: "Rx−", axis: "rx-"}, {label: "Rx+", axis: "rx+"},
                    {label: "Ry−", axis: "ry-"}, {label: "Ry+", axis: "ry+"},
                    {label: "Rz−", axis: "rz-"}, {label: "Rz+", axis: "rz+"}
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
                    Accessible.name: "Hold to jog " + modelData.label
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Hold a direction to move. Releasing, losing focus, changing arms, or disconnecting stops the command. Only this arm is addressed."
            color: "#a8bac5"
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
