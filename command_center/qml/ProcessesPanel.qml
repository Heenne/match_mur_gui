import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3 as NativeDialogs

Rectangle {
    implicitHeight: controls.implicitHeight + 36
    id: root
    required property var processManager
    property bool waitingForSimulationServer: false
    property int autoOpenAttempts: 0
    property bool autoOpenTimedOut: false
    readonly property bool localSimulation: session.mode === "Simulation" && isLocalTarget()
    readonly property string simulationContext: JSON.stringify([session.mode, session.target, session.rosDomain])
    readonly property string localLaunchState: processState("Gazebo simulation world")
    readonly property bool localLaunchActive: localLaunchState === "Starting" || localLaunchState === "Running"
    readonly property bool gazeboServerReady: processManager.gazeboServerStatus === "Available"
    readonly property string gazeboClientState: processState("Gazebo world view")
    readonly property bool gazeboClientActive: gazeboClientState === "Running" || gazeboClientState === "Starting"
    readonly property bool gazeboViewDuringRun: gazeboClientActive || gazeboClientState === "Stopping" ||
                                               (localSimulation && (gazeboServerReady || localLaunchActive || waitingForSimulationServer))
    readonly property bool gazeboViewChecked: gazeboViewDuringRun
                                              ? gazeboClientActive || waitingForSimulationServer
                                              : localSimulation && session.gazeboViewOnSimulationStart
    property string worldError: ""
    readonly property var availableWorlds: processManager.simulationWorlds.concat(
        session.customSimulationWorlds.filter(function(file) {
            return !processManager.simulationWorlds.some(function(world) { return world.file === file })
        }).map(function(file) { return { label: root.displayFile(file), file: file, map: "" } }))
    readonly property string worldFile: session.simulationWorldFile || (availableWorlds.length ? availableWorlds[0].file : "")
    readonly property string mapFile: session.simulationWorldFile ? session.simulationMapFile : (availableWorlds.length ? availableWorlds[0].map : "")
    readonly property bool worldSettingsEnabled: !localLaunchActive && localLaunchState !== "Stopping"
    readonly property bool scaleWorld: displayFile(worldFile) === "scale.world"
    color: theme.surface
    radius: 12
    border.color: theme.line

    function displayFile(file) {
        return file.split("/").pop()
    }
    function processState(name) {
        for (let i = 0; i < processManager.processes.length; ++i) {
            if (processManager.processes[i].name === name) return processManager.processes[i].state
        }
        return "Stopped"
    }
    function isLocalTarget() {
        return session.target === "localhost" || session.target === "127.0.0.1" || session.target === "::1"
    }
    onSimulationContextChanged: cancelGazeboViewRequest()
    onLocalLaunchActiveChanged: if (!localLaunchActive) cancelGazeboViewRequest()

    function cancelGazeboViewRequest() {
        waitingForSimulationServer = false
        autoOpenTimedOut = false
    }
    function requestGazeboView() {
        autoOpenTimedOut = false
        if (gazeboClientActive || (gazeboServerReady && processManager.startGazeboView())) return
        autoOpenAttempts = 0
        waitingForSimulationServer = true
        processManager.probeGazeboServer()
    }
    function recentLogLines(output) {
        const tail = output.slice(-2400).replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, "").trim()
        return tail.length ? tail.split(/[\r\n]+/).slice(-3) : []
    }
    function gazeboStatusText() {
        if (gazeboClientState === "Starting") return qsTr("Opening Gazebo world view…")
        if (gazeboClientState === "Running") return qsTr("Gazebo world view is open")
        if (gazeboClientState === "Stopping") return qsTr("Closing Gazebo world view…")
        if (waitingForSimulationServer) return qsTr("Waiting for Gazebo to open the world view…")
        if (!gazeboViewDuringRun) {
            return session.gazeboViewOnSimulationStart
                   ? qsTr("The Gazebo window will open when you start the simulation.")
                   : qsTr("The simulation will start without a Gazebo window.")
        }
        if (gazeboClientState === "Failed") return qsTr("Gazebo world view failed. Check the process log.")
        if (gazeboServerReady) return qsTr("Gazebo server available · world view can be opened")
        if (autoOpenTimedOut) return qsTr("No Gazebo server detected within 60 seconds. Check the simulation log.")
        if (processManager.gazeboServerStatus === "Failed") return qsTr("Could not check Gazebo Transport. See the session log.")
        if (processManager.gazeboServerStatus === "Not found") return qsTr("Waiting for a Gazebo server in the local transport partition.")
        return qsTr("Checking for a local Gazebo server…")
    }

    NativeDialogs.FileDialog {
        id: worldDialog
        title: qsTr("Choose Gazebo world")
        nameFilters: [qsTr("Gazebo worlds (*.world *.sdf)")]
        onAccepted: {
            session.addSimulationWorld(fileUrl.toString())
            root.worldError = ""
        }
    }
    NativeDialogs.FileDialog {
        id: mapDialog
        title: qsTr("Choose ROS map")
        nameFilters: [qsTr("ROS map YAML (*.yaml *.yml)")]
        onAccepted: session.setSimulationWorldFiles(root.worldFile, fileUrl.toString())
    }

    ColumnLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label { text: root.localSimulation ? qsTr("Simulation world") : qsTr("Processes"); color: theme.ink; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true
            visible: !root.isLocalTarget()
            text: qsTr("Start the MuR620 simulation over SSH on %1. The remote ROS 2 Jazzy workspace must be available in a login shell; SSH keys or an agent must allow non-interactive access.").arg(session.target)
            color: theme.muted
            wrapMode: Text.WordWrap
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            ComboBox {
                id: worldSelector
                width: 140
                height: 40
                visible: root.localSimulation
                enabled: root.worldSettingsEnabled
                textRole: "label"
                model: root.availableWorlds.some(function(world) { return world.file === root.worldFile }) || !root.worldFile
                       ? root.availableWorlds
                       : root.availableWorlds.concat([{ label: root.displayFile(root.worldFile), file: root.worldFile, map: root.mapFile }])
                currentIndex: model.findIndex(function(world) { return world.file === root.worldFile })
                Accessible.name: qsTr("Gazebo world")
                onActivated: {
                    const world = model[index]
                    if (world.file !== root.worldFile) session.setSimulationWorldFiles(world.file, world.map)
                    root.worldError = ""
                }
            }
            ActionButton {
                primary: true
                text: !root.isLocalTarget() ? qsTr("Start remote simulation")
                      : root.localLaunchActive ? qsTr("Stop simulation world") : qsTr("Start simulation world")
                enabled: session.mode === "Simulation" &&
                         (!root.isLocalTarget() || root.localLaunchActive ||
                          (root.worldSettingsEnabled && root.worldFile !== ""))
                onClicked: {
                    if (!root.isLocalTarget()) {
                        root.processManager.startRemoteSimulation(session.target, session.rosDomain, session.robot)
                        return
                    }
                    if (root.localLaunchActive) {
                        root.cancelGazeboViewRequest()
                        root.processManager.stopProcess("Gazebo simulation world")
                        return
                    }
                    root.worldError = ""
                    if (!root.processManager.startSimulationWorld(root.worldFile, root.mapFile, session.rosDomain)) {
                        root.worldError = qsTr("Could not start the world. Check the selected files and the session log.")
                    } else if (session.gazeboViewOnSimulationStart) {
                        root.requestGazeboView()
                    }
                }
            }
            Switch {
                id: gazeboViewSwitch
                objectName: "gazeboViewSwitch"
                text: root.gazeboViewDuringRun ? qsTr("Gazebo world view") : qsTr("Gazebo window on start")
                checked: root.gazeboViewChecked
                enabled: root.gazeboClientState !== "Stopping" &&
                         (root.localSimulation || root.gazeboClientActive)
                implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
                implicitHeight: 38
                padding: 8
                spacing: 8
                font.weight: Font.DemiBold

                onToggled: {
                    if (!root.gazeboViewDuringRun) {
                        session.setGazeboViewOnSimulationStart(checked)
                    } else if (checked) {
                        root.requestGazeboView()
                    } else {
                        root.cancelGazeboViewRequest()
                        root.processManager.stopProcess("Gazebo world view")
                    }
                    checked = Qt.binding(function() { return root.gazeboViewChecked })
                }

                indicator: Rectangle {
                    implicitWidth: 44
                    implicitHeight: 24
                    x: gazeboViewSwitch.leftPadding
                    y: (gazeboViewSwitch.height - height) / 2
                    radius: height / 2
                    color: !gazeboViewSwitch.enabled ? theme.surface : gazeboViewSwitch.checked ? theme.accent : theme.raised
                    border.color: gazeboViewSwitch.visualFocus ? theme.focus : theme.subtle

                    Rectangle {
                        width: 18
                        height: width
                        x: 3 + (parent.width - width - 6) * gazeboViewSwitch.position
                        anchors.verticalCenter: parent.verticalCenter
                        radius: width / 2
                        color: !gazeboViewSwitch.enabled ? theme.subtle : gazeboViewSwitch.checked ? theme.accentInk : theme.ink
                        Behavior on x {
                            enabled: !gazeboViewSwitch.pressed
                            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                        }
                    }
                }

                contentItem: Text {
                    leftPadding: gazeboViewSwitch.indicator.width + gazeboViewSwitch.spacing
                    text: gazeboViewSwitch.text
                    font: gazeboViewSwitch.font
                    color: gazeboViewSwitch.enabled ? theme.ink : theme.subtle
                    verticalAlignment: Text.AlignVCenter
                }
            }
            SimulationFlows {
                width: 108
                height: 40
                visible: root.localSimulation
                processManager: root.processManager
                profile: session
                worlds: root.availableWorlds
            }
            ActionButton { text: qsTr("Stop app-owned processes"); onClicked: { root.cancelGazeboViewRequest(); root.processManager.stopAll() } }
            ActionButton { text: qsTr("Open session log"); onClicked: { logs.showSource("All logs"); logs.raiseDock() } }
        }
        Label {
            Layout.fillWidth: true
            visible: root.localSimulation
            text: root.gazeboStatusText()
            color: root.gazeboClientState === "Failed" ? theme.danger
                   : root.gazeboServerReady ? theme.focus : theme.muted
            wrapMode: Text.WordWrap
        }
        Flow {
            visible: root.localSimulation
            Layout.fillWidth: true
            spacing: 8
            ActionButton {
                text: qsTr("Choose Gazebo world…")
                enabled: root.worldSettingsEnabled
                onClicked: worldDialog.open()
            }
            ActionButton {
                text: qsTr("Choose ROS map…")
                enabled: root.worldSettingsEnabled && root.worldFile !== ""
                onClicked: mapDialog.open()
            }
            ActionButton {
                text: qsTr("Clear ROS map")
                visible: root.mapFile !== ""
                enabled: root.worldSettingsEnabled
                onClicked: session.setSimulationWorldFiles(root.worldFile, "")
            }
        }
        Label {
            Layout.fillWidth: true
            visible: root.localSimulation
            text: root.mapFile ? qsTr("ROS map: %1").arg(root.displayFile(root.mapFile)) : qsTr("ROS map: none")
            color: theme.muted
            wrapMode: Text.WrapAnywhere
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
                if (root.gazeboClientActive || (root.gazeboServerReady && root.processManager.startGazeboView())) {
                    root.waitingForSimulationServer = false
                } else if (++root.autoOpenAttempts >= 120) {
                    root.waitingForSimulationServer = false
                    root.autoOpenTimedOut = true
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: root.localSimulation && root.worldError !== ""
            text: root.worldError
            color: theme.danger
            wrapMode: Text.WordWrap
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: theme.line }
        Label {
            visible: root.localSimulation
            text: qsTr("Spawn robots")
            color: theme.ink
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            visible: root.localSimulation
            text: root.localLaunchActive && root.processManager.simulationDomain !== session.rosDomain
                  ? qsTr("This world is running in ROS domain %1. Select that domain to spawn or control its robots.").arg(root.processManager.simulationDomain)
                  : qsTr("Choose a model and start position for each robot. Control selects its ROS namespace. Connect to the simulation, then enable jog in Robot.")
            wrapMode: Text.WordWrap
            color: theme.muted
        }
        Repeater {
            model: root.localSimulation ? ["mur620a", "mur620b", "mur620c", "mur620d"] : []
            delegate: Flow {
                id: slotControls
                required property string modelData
                required property int index
                readonly property string runState: root.processState("Simulation · " + modelData)
                readonly property bool slotAvailable: ["Stopped", "Exited", "Failed"].indexOf(runState) !== -1
                readonly property bool matchingDomain: root.processManager.simulationDomain === session.rosDomain
                property bool spawnRejected: false
                Layout.fillWidth: true
                spacing: 8

                Label {
                    width: 80
                    height: 40
                    text: slotControls.modelData
                    verticalAlignment: Text.AlignVCenter
                    color: theme.ink
                    font.weight: Font.DemiBold
                }
                ComboBox {
                    id: modelChoice
                    width: 125
                    model: [qsTr("MiR base"), "MuR620"]
                    currentIndex: 1
                    enabled: slotControls.slotAvailable
                    Accessible.name: qsTr("Robot model")
                }
                Repeater {
                    model: [
                        {axis: "X", value: (root.scaleWorld ? 44 : -2) + (slotControls.index % 2) * 2},
                        {axis: "Y", value: (root.scaleWorld ? 44 : -2) + Math.floor(slotControls.index / 2) * 2}
                    ]
                    delegate: Row {
                        required property var modelData
                        required property int index
                        spacing: 4
                        Label { text: modelData.axis; height: 40; verticalAlignment: Text.AlignVCenter; color: theme.muted }
                        TextField {
                            id: coordinateField
                            width: 60
                            height: 40
                            text: String(modelData.value)
                            enabled: slotControls.slotAvailable
                            validator: DoubleValidator { locale: "C"; notation: DoubleValidator.StandardNotation }
                            Accessible.name: qsTr("Start position %1 in meters").arg(modelData.axis)
                        }
                        property alias coordinate: coordinateField.text
                        property alias validCoordinate: coordinateField.acceptableInput
                    }
                    id: poseFields
                }
                ActionButton {
                    text: qsTr("Spawn")
                    enabled: root.localLaunchActive && root.gazeboServerReady && slotControls.matchingDomain &&
                             slotControls.slotAvailable && poseFields.itemAt(0) && poseFields.itemAt(1) &&
                             poseFields.itemAt(0).validCoordinate && poseFields.itemAt(1).validCoordinate
                    onClicked: {
                        slotControls.spawnRejected = !root.processManager.spawnSimulationRobot(
                            slotControls.modelData, modelChoice.currentIndex === 0 ? "mir" : "mur620",
                            Number(poseFields.itemAt(0).coordinate), Number(poseFields.itemAt(1).coordinate), 0)
                    }
                }
                ActionButton {
                    text: qsTr("Control")
                    enabled: slotControls.runState === "Running" && slotControls.matchingDomain
                    onClicked: session.selectSimulationRobot(slotControls.modelData)
                }
                Label {
                    height: 40
                    verticalAlignment: Text.AlignVCenter
                    text: slotControls.spawnRejected ? qsTr("Spawn request rejected") : uiText(slotControls.runState)
                    color: slotControls.spawnRejected || slotControls.runState === "Failed" ? theme.danger : theme.muted
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: theme.line }
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
                        color: theme.canvas
                        radius: 8
                        clip: true
                        ColumnLayout {
                            id: processColumn
                            anchors.fill: parent
                            anchors.margins: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: uiText(modelData.name) + " · " + (modelData.host || qsTr("local")); color: theme.ink; font.bold: true; wrapMode: Text.Wrap }
                                Label { text: uiText(modelData.state); color: modelData.state === "Failed" || modelData.state === "Stop failed" || modelData.state === "Stop unconfirmed" ? theme.danger : modelData.state === "Connecting" || modelData.state === "Stopping" ? theme.warning : theme.accent }
                                Label { text: modelData.exitCode >= 0 ? qsTr("exit %1").arg(modelData.exitCode) : ""; color: theme.muted }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Button { text: qsTr("Stop"); enabled: modelData.state === "Running" || modelData.state === "Starting" || modelData.state === "Connecting" || modelData.state === "Stop failed" || modelData.state === "Stop unconfirmed"; onClicked: root.processManager.stopProcess(modelData.name) }
                                Button { text: qsTr("Open log"); onClicked: { logs.showSource(root.processManager.logSource(modelData.name)); logs.raiseDock() } }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: logLines.implicitHeight + 16
                                visible: processCard.recentLines.length > 0
                                color: theme.canvas
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
                                            color: theme.ink
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
