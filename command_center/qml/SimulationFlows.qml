import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Item {
    id: root
    required property var processManager
    required property var profile
    property var worlds: []
    property var runningBlocks: []
    property int runIndex: -1
    property int elapsed: 0
    property string message: ""
    property string runContext: ""
    property bool openWindow: false
    readonly property bool busy: runIndex >= 0


    function state(name) {
        for (let row of processManager.processes)
            if (row.name === name) return row.state
        return "Stopped"
    }
    function fail(text) {
        message = text
        runIndex = -1
    }
    function loadFlow(index) {
        blocks.clear()
        if (index < 0 || index >= profile.simulationFlows.length) {
            flowName.text = ""
            return
        }
        let flow = profile.simulationFlows[index]
        flowName.text = flow.name
        for (let block of flow.blocks) blocks.append(block)
    }
    function saveFlow() {
        let name = flowName.text.trim()
        if (!name || blocks.count === 0 || blocks.get(0).kind !== "world") {
            message = qsTr("Enter a name and add a world block first.")
            return
        }
        let entries = []
        for (let i = 0; i < blocks.count; ++i) {
            let b = blocks.get(i)
            entries.push({kind: b.kind, world: b.world, map: b.map, robot: b.robot,
                          model: b.model, x: b.x, y: b.y})
        }
        let flows = profile.simulationFlows.slice()
        let index = flows.findIndex(function(flow) { return flow.name === name })
        if (index < 0) flows.push({name: name, blocks: entries})
        else flows[index] = {name: name, blocks: entries}
        profile.saveSimulationFlows(flows)
        saved.currentIndex = flows.findIndex(function(flow) { return flow.name === name })
        message = qsTr("Flow saved.")
    }
    function runFlow() {
        if (profile.mode !== "Simulation" || !["localhost", "127.0.0.1", "::1"].includes(profile.target)) {
            message = qsTr("Select a local simulation profile first.")
            return
        }
        if (blocks.count === 0 || blocks.get(0).kind !== "world" || !blocks.get(0).world) {
            message = qsTr("Add a world block first.")
            return
        }
        let names = []
        let snapshot = []
        for (let i = 0; i < blocks.count; ++i) {
            let b = blocks.get(i)
            if (i > 0 && (b.kind !== "robot" || names.includes(b.robot) ||
                !Number.isFinite(Number(b.x)) || !Number.isFinite(Number(b.y)) ||
                String(b.x).trim() === "" || String(b.y).trim() === "")) {
                message = qsTr("Use each robot once and enter valid coordinates.")
                return
            }
            names.push(b.robot)
            snapshot.push({kind: b.kind, world: b.world, map: b.map, robot: b.robot,
                           model: b.model, x: b.x, y: b.y})
        }
        if (!processManager.startSimulationWorld(snapshot[0].world, snapshot[0].map, profile.rosDomain)) {
            message = qsTr("The world could not be started. Check the files and process log.")
            return
        }
        runningBlocks = snapshot
        runContext = JSON.stringify([profile.mode, profile.target, profile.rosDomain])
        openWindow = profile.gazeboViewOnSimulationStart
        runIndex = 0
        elapsed = 0
        message = qsTr("Starting the world…")
    }

    Timer {
        interval: 500
        repeat: true
        running: root.busy
        onTriggered: {
            if (root.runContext !== JSON.stringify([profile.mode, profile.target, profile.rosDomain])) {
                root.fail(qsTr("Profile changed. Flow stopped; started processes remain available."))
                return
            }
            const worldState = root.state("Gazebo simulation world")
            if (["Failed", "Exited", "Stopped", "Stopping"].includes(worldState)) {
                root.fail(qsTr("World stopped. Check the process log."))
                return
            }
            if (++root.elapsed > 240) {
                root.fail(qsTr("Timed out. Check the process log."))
                return
            }
            if (root.runIndex === 0) {
                processManager.probeGazeboServer()
                if (processManager.gazeboServerStatus !== "Available") return
                if (root.openWindow && !processManager.startGazeboView()) return
                root.runIndex = 1
                root.elapsed = 0
            }
            if (root.runIndex >= root.runningBlocks.length) {
                root.runIndex = -1
                root.message = qsTr("Launch requests sent. Check process logs and controller status.")
                return
            }
            let block = root.runningBlocks[root.runIndex]
            let name = "Simulation · " + block.robot
            if (!processManager.spawnSimulationRobot(block.robot, block.model, Number(block.x), Number(block.y), 0)) {
                root.fail(qsTr("Could not start %1. Check the process log.").arg(block.robot))
                return
            }
            root.message = qsTr("Starting %1…").arg(block.robot)
            root.runIndex++
            root.elapsed = 0
        }
    }

    ActionButton {
        objectName: "openSimulationFlows"
        text: qsTr("Flows…")
        onClicked: editor.open()
    }
    Dialog {
        id: editor
        objectName: "simulationFlowsDialog"
        palette.window: theme.surface
        palette.windowText: theme.ink
        palette.base: theme.raised
        palette.alternateBase: theme.surface
        palette.text: theme.ink
        palette.button: theme.raised
        palette.buttonText: theme.ink
        palette.highlight: theme.selected
        palette.highlightedText: theme.ink
        title: qsTr("Simulation flows")
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 780)
        height: Math.min(parent.height - 24, 760)
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Flow {
                Layout.fillWidth: true
                spacing: 8
                FlowComboBox {
                    id: saved
                    objectName: "savedFlowSelector"
                    width: 190
                    model: profile.simulationFlows.map(function(flow) { return flow.name })
                    currentIndex: -1
                    displayText: currentIndex < 0 ? qsTr("Saved flows") : currentText
                    onActivated: root.loadFlow(index)
                }
                ActionButton {
                    text: qsTr("New")
                    enabled: !root.busy
                    onClicked: { saved.currentIndex = -1; root.loadFlow(-1); root.message = "" }
                }
                TextField { id: flowName; objectName: "flowName"; width: 175; placeholderText: qsTr("Flow name"); enabled: !root.busy }
                ActionButton { objectName: "saveFlow"; text: qsTr("Save"); enabled: !root.busy; onClicked: root.saveFlow() }
                ActionButton {
                    text: qsTr("Delete")
                    enabled: !root.busy && saved.currentIndex >= 0
                    onClicked: {
                        let flows = profile.simulationFlows.slice()
                        flows.splice(saved.currentIndex, 1)
                        profile.saveSimulationFlows(flows)
                        saved.currentIndex = -1
                        root.loadFlow(-1)
                    }
                }
            }
            ScrollView {
                id: scroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                Column {
                    width: scroll.availableWidth
                    spacing: 0
                    Repeater {
                        model: ListModel { id: blocks }
                        delegate: Column {
                            id: blockDelegate
                            required property int index
                            property int blockIndex: index
                            width: parent.width
                            spacing: 0
                            Rectangle {
                                visible: index > 0
                                width: 2; height: 20
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: theme.focus
                            }
                            Rectangle {
                                width: parent.width
                                height: blockContent.implicitHeight + 32
                                color: theme.raised
                                border.color: theme.line
                                radius: 8
                                ColumnLayout {
                                    id: blockContent
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 8
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            Layout.fillWidth: true
                                            text: blocks.get(index).kind === "world" ? qsTr("Simulation world") : qsTr("Spawn robot")
                                            color: theme.ink
                                            font.bold: true
                                        }
                                        Button {
                                            text: qsTr("Remove")
                                            enabled: !root.busy
                                            onClicked: blocks.remove(index)
                                        }
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        visible: blocks.get(index).kind === "world"
                                        FlowComboBox {
                                            objectName: "flowWorldSelector"
                                            width: 230
                                            enabled: !root.busy
                                            textRole: "label"
                                            model: root.worlds
                                            currentIndex: root.worlds.findIndex(function(w) { return w.file === blocks.get(index).world })
                                            onActivated: {
                                                blocks.setProperty(blockDelegate.blockIndex, "world", model[index].file)
                                                blocks.setProperty(blockDelegate.blockIndex, "map", model[index].map)
                                            }
                                        }
                                        Label {
                                            height: 40
                                            verticalAlignment: Text.AlignVCenter
                                            text: blocks.get(index).map ? qsTr("ROS map included") : qsTr("No ROS map")
                                            color: theme.muted
                                        }
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        visible: blocks.get(index).kind === "robot"
                                        FlowComboBox {
                                            objectName: "flowRobotSelector"
                                            width: 135
                                            enabled: !root.busy
                                            model: ["mur620a", "mur620b", "mur620c", "mur620d"]
                                            currentIndex: model.indexOf(blocks.get(index).robot)
                                            onActivated: blocks.setProperty(blockDelegate.blockIndex, "robot", currentText)
                                        }
                                        FlowComboBox {
                                            objectName: "flowModelSelector"
                                            width: 135
                                            enabled: !root.busy
                                            model: [qsTr("MiR base"), "MuR620"]
                                            currentIndex: blocks.get(index).model === "mir" ? 0 : 1
                                            onActivated: blocks.setProperty(blockDelegate.blockIndex, "model", index === 0 ? "mir" : "mur620")
                                        }
                                        Label { text: "X"; color: theme.muted; height: 40; verticalAlignment: Text.AlignVCenter }
                                        TextField {
                                            width: 80
                                            enabled: !root.busy
                                            text: blocks.get(index).x
                                            validator: DoubleValidator { locale: "C" }
                                            onTextEdited: blocks.setProperty(blockDelegate.blockIndex, "x", text)
                                            Accessible.name: qsTr("Start position X")
                                        }
                                        Label { text: "Y"; color: theme.muted; height: 40; verticalAlignment: Text.AlignVCenter }
                                        TextField {
                                            width: 80
                                            enabled: !root.busy
                                            text: blocks.get(index).y
                                            validator: DoubleValidator { locale: "C" }
                                            onTextEdited: blocks.setProperty(blockDelegate.blockIndex, "y", text)
                                            Accessible.name: qsTr("Start position Y")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                ActionButton {
                    objectName: "addFlowWorld"
                    text: qsTr("Add world")
                    enabled: !root.busy && blocks.count === 0 && root.worlds.length > 0
                    onClicked: blocks.append({kind: "world", world: root.worlds[0].file, map: root.worlds[0].map, robot: "", model: "", x: "", y: ""})
                }
                ActionButton {
                    objectName: "addFlowRobot"
                    text: qsTr("Add robot")
                    enabled: !root.busy && blocks.count > 0 && blocks.count < 5
                    onClicked: {
                        let robots = ["mur620a", "mur620b", "mur620c", "mur620d"]
                        blocks.append({kind: "robot", world: "", map: "", robot: robots[blocks.count - 1], model: "mur620", x: String((blocks.count - 1) * 2), y: "0"})
                    }
                }
                ActionButton {
                    primary: true
                    text: qsTr("Start flow")
                    enabled: !root.busy && blocks.count > 0
                    onClicked: root.runFlow()
                }
                ActionButton {
                    text: qsTr("Cancel flow")
                    visible: root.busy
                    onClicked: root.fail(qsTr("Flow cancelled. Started processes remain available."))
                }
            }
            Label { Layout.fillWidth: true; text: root.message; color: theme.muted; wrapMode: Text.WordWrap }
        }
    }
}
