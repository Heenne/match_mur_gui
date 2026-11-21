import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    implicitHeight: controls.implicitHeight + 40
    id: root
    required property var controller
    required property var sessionController
    color: theme.surface
    radius: 12
    border.color: theme.line

    ColumnLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label { text: "Qualisys / Mocap"; color: theme.ink; font.pixelSize: 22; font.bold: true }
        Label {
            Layout.fillWidth: true
            text: "Optional pose monitor for " + root.sessionController.robot + ". Existing ROS bridges are detected and left under their current owner's control."
            color: theme.muted
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 18
            rowSpacing: 8
            Label { text: "Raw pose"; color: theme.muted }
            Label { Layout.fillWidth: true; text: root.controller.poseStatus; color: root.controller.poseStatus.indexOf("Stale") >= 0 ? theme.warning : theme.accent; Accessible.name: root.controller.poseStatus }
            Label { text: "Position / orientation"; color: theme.muted }
            Label { Layout.fillWidth: true; text: root.controller.pose + " · frame " + (root.controller.poseFrame || "unknown"); color: theme.ink; wrapMode: Text.WordWrap }
            Label { text: "Rate and age"; color: theme.muted }
            Label { text: (root.controller.frequencyHz > 0 ? root.controller.frequencyHz.toFixed(1) + " Hz" : "— Hz") + " · " + (root.controller.dataAgeSeconds >= 0 ? root.controller.dataAgeSeconds.toFixed(2) + " s old" : "no data"); color: theme.ink }
            Label { text: "Map / TF"; color: theme.muted }
            Label { Layout.fillWidth: true; text: root.controller.mapStatus; color: root.controller.mapStatus.indexOf("active") >= 0 ? theme.accent : theme.warning; wrapMode: Text.WordWrap; Accessible.name: root.controller.mapStatus }
            Label { text: "Localization hold"; color: theme.muted }
            Label { Layout.fillWidth: true; text: root.controller.freezeStatus; color: root.controller.frozen ? theme.warning : theme.ink; wrapMode: Text.WordWrap }
            Label { text: "Bridge ownership"; color: theme.muted }
            Label { Layout.fillWidth: true; text: root.controller.bridgeState; color: root.controller.bridgeState.indexOf("external") >= 0 ? theme.accent : (root.controller.bridgeState.indexOf("Failed") >= 0 ? theme.danger : theme.ink); wrapMode: Text.WordWrap }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: theme.line }
        Label { text: "Bridge connection"; color: theme.ink; font.pixelSize: 17; font.bold: true }
        GridLayout {
            Layout.fillWidth: true
            columns: width < 540 ? 2 : 4
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
        Flow {
            Layout.fillWidth: true
            spacing: 6
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
            color: theme.muted
            wrapMode: Text.WordWrap
        }
        Item { Layout.fillHeight: true }
    }
}
