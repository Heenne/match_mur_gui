import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: root
    required property var processManager
    color: "#1b2933"
    radius: 12
    border.color: "#2b3c47"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label { text: "Processes"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true
            text: session.target === "localhost" || session.target === "127.0.0.1"
                  ? "Start the local MuR620 simulation in one action. Gazebo GUI is disabled; each app-started process has its own status and captured output."
                  : "Start the MuR620 simulation over SSH on " + session.target + ". The remote ROS 2 Jazzy workspace must be available in a login shell; SSH keys or an agent must allow non-interactive access."
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Button {
                text: session.target === "localhost" || session.target === "127.0.0.1" ? "Start simulation" : "Start remote simulation"
                enabled: session.mode === "Simulation"
                onClicked: session.target === "localhost" || session.target === "127.0.0.1"
                           ? root.processManager.startSimulation(session.robot)
                           : root.processManager.startRemoteSimulation(session.target, session.rosDomain, session.robot)
            }
            Button { text: "Open Gazebo world view"; enabled: session.target === "localhost" || session.target === "127.0.0.1"; onClicked: root.processManager.startGazeboView() }
            Button { text: "Stop app-owned processes"; onClicked: root.processManager.stopAll() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#354752" }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Column {
                width: parent.width
                spacing: 10
                Repeater {
                    model: root.processManager.processes
                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        implicitHeight: processColumn.implicitHeight + 20
                        color: "#14212a"
                        radius: 8
                        ColumnLayout {
                            id: processColumn
                            anchors.fill: parent
                            anchors.margins: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.name; color: "#f2f6f8"; font.bold: true }
                                Label { text: modelData.host.length ? "@ " + modelData.host : "local"; color: "#a8bac5" }
                                Label { text: modelData.state; color: modelData.state === "Failed" || modelData.state === "Stop failed" || modelData.state === "Stop unconfirmed" ? "#f07178" : modelData.state === "Connecting" || modelData.state === "Stopping" ? "#f0b44d" : "#45d6a8" }
                                Label { text: modelData.exitCode >= 0 ? "exit " + modelData.exitCode : ""; color: "#a8bac5" }
                                Item { Layout.fillWidth: true }
                                Button { text: "Stop"; enabled: modelData.state === "Running" || modelData.state === "Starting" || modelData.state === "Connecting" || modelData.state === "Stop failed" || modelData.state === "Stop unconfirmed"; onClicked: root.processManager.stopProcess(modelData.name) }
                                Button { text: "Open log"; onClicked: { logs.showSource(root.processManager.logSource(modelData.name)); logs.raiseDock() } }
                            }
                            TextArea {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 92
                                readOnly: true
                                text: modelData.log
                                color: "#c9d6dc"
                                background: Rectangle { color: "#101820"; radius: 4 }
                                wrapMode: TextEdit.Wrap
                            }
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            text: root.processManager.log
            color: "#a8bac5"
            wrapMode: Text.Wrap
            elide: Text.ElideLeft
        }
    }
}
