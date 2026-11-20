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
            text: "Start the local MuR620 simulation in one action. Gazebo GUI is disabled; each app-started process has its own status and captured output."
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Button { text: "Start simulation"; onClicked: root.processManager.startSimulation(session.robot) }
            Button { text: "Open Gazebo world view"; onClicked: root.processManager.startGazeboView() }
            Button { text: "Stop app processes"; onClicked: root.processManager.stopAll() }
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
                                Label { text: modelData.state; color: modelData.state === "Failed" ? "#f07178" : "#45d6a8" }
                                Label { text: modelData.exitCode >= 0 ? "exit " + modelData.exitCode : ""; color: "#a8bac5" }
                                Item { Layout.fillWidth: true }
                                Button { text: "Stop"; enabled: modelData.state === "Running" || modelData.state === "Starting"; onClicked: root.processManager.stopProcess(modelData.name) }
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
