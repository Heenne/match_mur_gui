import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: root
    required property var controller
    required property var sessionController
    color: "#1b2933"
    radius: 12
    border.color: "#2b3c47"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label { text: "Qualisys / Mocap"; color: "#f2f6f8"; font.pixelSize: 22; font.bold: true }
        Label {
            Layout.fillWidth: true
            text: "Optional pose monitor for " + root.sessionController.robot + ". Existing ROS bridges are detected and left under their current owner's control."
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 18
            rowSpacing: 8
            Label { text: "Raw pose"; color: "#a8bac5" }
            Label { Layout.fillWidth: true; text: root.controller.poseStatus; color: root.controller.poseStatus.indexOf("Stale") >= 0 ? "#f0b44d" : "#45d6a8"; Accessible.name: root.controller.poseStatus }
            Label { text: "Position / orientation"; color: "#a8bac5" }
            Label { Layout.fillWidth: true; text: root.controller.pose + " · frame " + (root.controller.poseFrame || "unknown"); color: "#f2f6f8"; wrapMode: Text.WordWrap }
            Label { text: "Rate and age"; color: "#a8bac5" }
            Label { text: (root.controller.frequencyHz > 0 ? root.controller.frequencyHz.toFixed(1) + " Hz" : "— Hz") + " · " + (root.controller.dataAgeSeconds >= 0 ? root.controller.dataAgeSeconds.toFixed(2) + " s old" : "no data"); color: "#f2f6f8" }
            Label { text: "Map / TF"; color: "#a8bac5" }
            Label { Layout.fillWidth: true; text: root.controller.mapStatus; color: root.controller.mapStatus.indexOf("active") >= 0 ? "#45d6a8" : "#f0b44d"; wrapMode: Text.WordWrap; Accessible.name: root.controller.mapStatus }
            Label { text: "Localization hold"; color: "#a8bac5" }
            Label { Layout.fillWidth: true; text: root.controller.freezeStatus; color: root.controller.frozen ? "#f0b44d" : "#f2f6f8"; wrapMode: Text.WordWrap }
            Label { text: "Bridge ownership"; color: "#a8bac5" }
            Label { Layout.fillWidth: true; text: root.controller.bridgeState; color: root.controller.bridgeState.indexOf("external") >= 0 ? "#45d6a8" : (root.controller.bridgeState.indexOf("Failed") >= 0 ? "#f07178" : "#f2f6f8"); wrapMode: Text.WordWrap }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#354752" }
        Label { text: "Bridge connection"; color: "#f2f6f8"; font.pixelSize: 17; font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: sshHost; Layout.fillWidth: true; text: root.controller.sshHost; placeholderText: "SSH host"; Accessible.name: "Qualisys SSH host" }
            TextField { id: qtmHost; Layout.fillWidth: true; text: root.controller.qtmHost; placeholderText: "QTM host"; Accessible.name: "QTM host" }
            TextField {
                id: qtmPort
                Layout.preferredWidth: 100
                text: String(root.controller.qtmPort)
                placeholderText: "QTM port"
                validator: IntValidator { bottom: 1; top: 65535 }
                Accessible.name: "QTM port"
            }
            Button { text: "Save"; onClicked: root.controller.setBridgeSettings(sshHost.text, qtmHost.text, Number(qtmPort.text)) }
        }
        RowLayout {
            Button {
                text: "Start bridge"
                enabled: root.sessionController.connectionState === "Connected" && root.controller.bridgeState === "Stopped"
                onClicked: root.controller.startBridge()
            }
            Button {
                text: "Stop app-owned bridge"
                enabled: root.controller.bridgeState.indexOf("app-owned") >= 0 && root.controller.bridgeState.indexOf("Failed") < 0
                onClicked: root.controller.stopBridge()
            }
            Button { text: "Enable map / TF output"; enabled: root.sessionController.connectionState === "Connected"; onClicked: root.controller.setMapOutput(true) }
            Button { text: "Disable map / TF output"; enabled: root.sessionController.connectionState === "Connected"; onClicked: root.controller.setMapOutput(false) }
            Button { text: root.controller.frozen ? "Resume localization" : "Freeze localization"; enabled: root.sessionController.connectionState === "Connected"; onClicked: root.controller.setFrozen(!root.controller.frozen) }
        }
        Label {
            Layout.fillWidth: true
            text: "Map output and localization freeze use the Qualisys bridge's /qualisys/enable_map_output and per-robot freeze_localization services. A failed calibration or stale pose is reported by the bridge."
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }
        Item { Layout.fillHeight: true }
    }
}
