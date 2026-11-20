import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: root
    required property var processManager
    color: "#1b2933"
    radius: 12
    border.color: "#2b3c47"

    function processState(name) {
        for (var i = 0; i < processManager.processes.length; ++i) {
            if (processManager.processes[i].name === name) return processManager.processes[i].state
        }
        return "Stopped"
    }
    function simulationServerReady() {
        // The managed launch is authoritative for the local server before /clock
        // becomes visible through a ROS monitor.
        return session.simulationServerAvailable || processState("MUR620 simulation") === "Running"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label { text: "Processes"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true
            text: "Start the simulation server and its ROS bridge. The Gazebo world view is a separately managed client; stopping it leaves the server and /clock running."
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Button {
                text: "Start simulation"
                onClicked: {
                    if (root.processManager.startSimulation(session.robot) && session.gazeboViewOnSimulationStart) {
                        root.waitingForSimulationServer = true
                        root.autoOpenAttempts = 0
                        root.autoOpenTimedOut = false
                    }
                }
            }
            Button {
                text: root.processState("Gazebo world view") === "Running" || root.processState("Gazebo world view") === "Starting" ? "Close Gazebo world view" : "Open Gazebo world view"
                enabled: root.processState("Gazebo world view") === "Running" || root.processState("Gazebo world view") === "Starting" || root.simulationServerReady()
                onClicked: {
                    if (root.processState("Gazebo world view") === "Running" || root.processState("Gazebo world view") === "Starting") root.processManager.stopProcess("Gazebo world view")
                    else root.processManager.startGazeboView()
                }
            }
            Button { text: "Stop app processes"; onClicked: root.processManager.stopAll() }
        }
        Label {
            Layout.fillWidth: true
            text: root.simulationServerReady() ? (session.simulationServerAvailable ? "Simulation server available (/clock publisher detected)" : "Simulation launch is running in this app; Gazebo client is managed separately") : (root.autoOpenTimedOut ? "Simulation server was not detected within 60 seconds; Gazebo client was not started." : "Gazebo world view needs a running simulation server. Connect to the simulation ROS domain to check /clock.")
            color: root.simulationServerReady() ? "#45d6a8" : "#a8bac5"
            wrapMode: Text.WordWrap
        }
        Timer {
            id: autoOpenTimer
            interval: 500
            repeat: true
            running: root.waitingForSimulationServer
            onTriggered: {
                if (root.simulationServerReady()) {
                    root.waitingForSimulationServer = false
                    if (root.processState("Gazebo world view") !== "Running" && root.processState("Gazebo world view") !== "Starting") root.processManager.startGazeboView()
                } else if (++root.autoOpenAttempts >= 120) {
                    root.waitingForSimulationServer = false
                    root.autoOpenTimedOut = true
                }
            }
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

    property bool waitingForSimulationServer: false
    property int autoOpenAttempts: 0
    property bool autoOpenTimedOut: false
}
