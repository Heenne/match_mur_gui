import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: root
    required property var processManager
    property bool waitingForSimulationServer: false
    property int autoOpenAttempts: 0
    property bool autoOpenTimedOut: false
    color: "#1b2933"
    radius: 12
    border.color: "#2b3c47"

    function processState(name) {
        for (let i = 0; i < processManager.processes.length; ++i) {
            if (processManager.processes[i].name === name) return processManager.processes[i].state
        }
        return "Stopped"
    }
    function isLocalTarget() {
        return session.target === "localhost" || session.target === "127.0.0.1" || session.target === "::1"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label { text: "Processes"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true
            text: root.isLocalTarget()
                  ? "Start the local MuR620 simulation in one action. Gazebo can run headless; its world view is a separate client."
                  : "Start the MuR620 simulation over SSH on " + session.target + ". The remote ROS 2 Jazzy workspace must be available in a login shell; SSH keys or an agent must allow non-interactive access."
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Button {
                text: root.isLocalTarget() ? "Start simulation" : "Start remote simulation"
                enabled: session.mode === "Simulation"
                onClicked: {
                    if (root.isLocalTarget()) {
                        if (root.processManager.startSimulation(session.robot) && session.gazeboViewOnSimulationStart) {
                            root.waitingForSimulationServer = true
                            root.autoOpenAttempts = 0
                            root.autoOpenTimedOut = false
                            if (session.connectionState !== "Connected") session.connectToTarget()
                        }
                    } else {
                        root.processManager.startRemoteSimulation(session.target, session.rosDomain, session.robot)
                    }
                }
            }
            Button {
                text: root.processState("Gazebo world view") === "Running" || root.processState("Gazebo world view") === "Starting" ? "Close Gazebo world view" : "Open Gazebo world view"
                enabled: root.isLocalTarget() &&
                         (root.processState("Gazebo world view") === "Running" || root.processState("Gazebo world view") === "Starting" || session.simulationServerAvailable)
                onClicked: {
                    if (root.processState("Gazebo world view") === "Running" || root.processState("Gazebo world view") === "Starting") root.processManager.stopProcess("Gazebo world view")
                    else root.processManager.startGazeboView()
                }
            }
            Button { text: "Stop app-owned processes"; onClicked: root.processManager.stopAll() }
        }
        Label {
            Layout.fillWidth: true
            text: session.simulationServerAvailable ? "Simulation server available (/clock publisher detected)" : (root.autoOpenTimedOut ? "Server was not confirmed by /clock within 60 seconds; Gazebo client was not started." : "Gazebo world view needs a confirmed /clock publisher. Connect to the simulation ROS domain.")
            color: session.simulationServerAvailable ? "#45d6a8" : "#a8bac5"
            wrapMode: Text.WordWrap
        }
        Timer {
            id: autoOpenTimer
            interval: 500
            repeat: true
            running: root.waitingForSimulationServer
            onTriggered: {
                if (session.simulationServerAvailable) {
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
