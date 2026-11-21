import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    implicitHeight: controls.implicitHeight + 36
    id: root
    required property var processManager
    property bool waitingForSimulationServer: false
    property int autoOpenAttempts: 0
    property bool autoOpenTimedOut: false
    property string autoOpenContext: ""
    readonly property bool localSimulation: session.mode === "Simulation" && isLocalTarget()
    readonly property bool gazeboServerReady: processManager.gazeboServerStatus === "Available"
    readonly property string gazeboClientState: processState("Gazebo world view")
    readonly property bool gazeboClientActive: gazeboClientState === "Running" || gazeboClientState === "Starting"
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
    function currentContext() {
        return JSON.stringify([session.preset, session.mode, session.target, session.rosDomain, session.robot])
    }
    function recentLogLines(output) {
        const tail = output.slice(-2400).replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, "").trim()
        return tail.length ? tail.split(/[\r\n]+/).slice(-3) : []
    }
    function gazeboStatusText() {
        if (gazeboServerReady) return "Gazebo server available · world view can be opened"
        if (autoOpenTimedOut) return "No Gazebo server detected within 60 seconds. Check the simulation log."
        if (processManager.gazeboServerStatus === "Failed") return "Could not check Gazebo Transport. See the session log."
        if (processManager.gazeboServerStatus === "Not found") return "Waiting for a Gazebo server in the local transport partition."
        return "Checking for a local Gazebo server…"
    }

    ColumnLayout {
        id: controls
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
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Button {
                text: root.isLocalTarget() ? "Start simulation" : "Start remote simulation"
                enabled: session.mode === "Simulation"
                onClicked: {
                    if (root.isLocalTarget()) {
                        if (root.processManager.startSimulation(session.robot) && session.gazeboViewOnSimulationStart) {
                            root.autoOpenContext = root.currentContext()
                            root.waitingForSimulationServer = true
                            root.autoOpenAttempts = 0
                            root.autoOpenTimedOut = false
                        }
                    } else {
                        root.processManager.startRemoteSimulation(session.target, session.rosDomain, session.robot)
                    }
                }
            }
            Button {
                text: root.gazeboClientActive ? "Close Gazebo world view" : "Open Gazebo world view"
                enabled: root.gazeboClientActive || (root.localSimulation && root.gazeboServerReady)
                onClicked: {
                    if (root.gazeboClientActive) root.processManager.stopProcess("Gazebo world view")
                    else root.processManager.startGazeboView()
                }
            }
            Button { text: "Stop app-owned processes"; onClicked: { root.waitingForSimulationServer = false; root.processManager.stopAll() } }
            Button { text: "Open session log"; onClicked: { logs.showSource("All logs"); logs.raiseDock() } }
        }
        Label {
            Layout.fillWidth: true
            visible: root.localSimulation
            text: root.gazeboStatusText()
            color: root.gazeboServerReady ? "#45d6a8" : "#a8bac5"
            wrapMode: Text.WordWrap
        }
        Timer {
            interval: 2000
            repeat: true
            triggeredOnStart: true
            running: root.localSimulation && (root.visible || root.waitingForSimulationServer)
            onTriggered: root.processManager.probeGazeboServer()
        }
        Timer {
            id: autoOpenTimer
            interval: 500
            repeat: true
            running: root.waitingForSimulationServer
            onTriggered: {
                if (root.autoOpenContext !== root.currentContext()) {
                    root.waitingForSimulationServer = false
                } else if (root.gazeboClientActive || (root.gazeboServerReady && root.processManager.startGazeboView())) {
                    root.waitingForSimulationServer = false
                } else if (++root.autoOpenAttempts >= 120) {
                    root.waitingForSimulationServer = false
                    root.autoOpenTimedOut = true
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#354752" }
        ScrollView {
            id: processList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            Layout.preferredHeight: 160
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true
            Column {
                width: processList.availableWidth
                spacing: 10
                Repeater {
                    model: root.processManager.processes
                    delegate: Rectangle {
                        id: processCard
                        required property var modelData
                        readonly property var recentLines: root.recentLogLines(modelData.log)
                        width: parent.width
                        implicitHeight: processColumn.implicitHeight + 20
                        color: "#14212a"
                        radius: 8
                        clip: true
                        ColumnLayout {
                            id: processColumn
                            anchors.fill: parent
                            anchors.margins: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: modelData.name + " · " + (modelData.host || "local"); color: "#f2f6f8"; font.bold: true; wrapMode: Text.Wrap }
                                Label { text: modelData.state; color: modelData.state === "Failed" || modelData.state === "Stop failed" || modelData.state === "Stop unconfirmed" ? "#f07178" : modelData.state === "Connecting" || modelData.state === "Stopping" ? "#f0b44d" : "#45d6a8" }
                                Label { text: modelData.exitCode >= 0 ? "exit " + modelData.exitCode : ""; color: "#a8bac5" }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Button { text: "Stop"; enabled: modelData.state === "Running" || modelData.state === "Starting" || modelData.state === "Connecting" || modelData.state === "Stop failed" || modelData.state === "Stop unconfirmed"; onClicked: root.processManager.stopProcess(modelData.name) }
                                Button { text: "Open log"; onClicked: { logs.showSource(root.processManager.logSource(modelData.name)); logs.raiseDock() } }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: logLines.implicitHeight + 16
                                visible: processCard.recentLines.length > 0
                                color: "#101820"
                                radius: 4
                                clip: true
                                ColumnLayout {
                                    id: logLines
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2
                                    Repeater {
                                        model: processCard.recentLines
                                        delegate: Label {
                                            required property string modelData
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: modelData
                                            textFormat: Text.PlainText
                                            elide: Text.ElideRight
                                            font.family: "monospace"
                                            color: "#c9d6dc"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
