import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3 as NativeDialogs
import "." as Panels

Item {
    id: window
    width: 1440
    height: 900
    implicitWidth: 1440
    implicitHeight: 900
    property var pages: ["Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings", "Mocap"]
    property color muted: "#a8bac5"
    property color foreground: "#f2f6f8"
    readonly property bool connectionFailed: session.connectionState.indexOf("Failed") === 0
    readonly property string connectionStatus: connectionFailed ? "Failed" : session.connectionState

    function statusColor(status) {
        if (status === "Disconnected") return "#f0b44d"
        if (status.indexOf("Failed") === 0 || status === "Error") return "#f07178"
        if (status === "Connecting") return "#f0b44d"
        return "#45d6a8"
    }
    function selectPage(page) { session.selectView(page) }

    Keys.onPressed: {
        if (event.key >= Qt.Key_1 && event.key <= Qt.Key_8) {
            selectPage(pages[event.key - Qt.Key_1]); event.accepted = true
        }
    }
    Component.onCompleted: forceActiveFocus()

    Rectangle { anchors.fill: parent; color: "#101820" }

    component StatusPill: Rectangle {
        required property string label
        radius: 12
        color: window.statusColor(label)
        implicitWidth: statusText.implicitWidth + 20
        implicitHeight: 28
        Label { id: statusText; anchors.centerIn: parent; text: parent.label; color: "#102029"; font.pixelSize: 12; font.bold: true }
    }
    component Panel: Rectangle {
        required property string heading
        color: "#1b2933"; radius: 12; border.color: "#2b3c47"
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 14
            Label { text: parent.parent.heading; color: window.foreground; font.pixelSize: 20; font.bold: true }
            Label { Layout.fillWidth: true; Layout.fillHeight: true; text: parent.parent.heading + " content will appear here."; color: window.muted; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignTop }
        }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 14
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Label { Layout.fillWidth: true; text: "MuR Command Center"; color: window.foreground; font.pixelSize: 25; font.bold: true; elide: Text.ElideRight }
                Label { Layout.fillWidth: true; text: session.mode + " · " + session.preset + " · " + session.target + " · domain " + session.rosDomain + " · " + session.robot; color: window.muted; elide: Text.ElideRight }
            }
            ComboBox { model: ["Simulation", "Hardware"]; currentIndex: model.indexOf(session.mode); onActivated: session.selectMode(currentText) }
            ComboBox { model: session.presets; currentIndex: model.indexOf(session.preset); onActivated: session.selectPreset(currentText) }
            Button { text: "New preset"; onClicked: presetDialog.open() }
            ComboBox {
                id: targetCombo
                editable: true
                model: ["localhost", "lab-host"]
                currentIndex: model.indexOf(session.target)
                Component.onCompleted: editText = session.target
                onActivated: session.selectTarget(currentText)
                onAccepted: session.selectTarget(editText)
            }
            Connections {
                target: session
                function onChanged() { targetCombo.editText = session.target }
            }
            ComboBox { model: ["mur620d", "mur620b", "mur620c"]; currentIndex: model.indexOf(session.robot); onActivated: session.selectRobot(currentText) }
        }
        Flow {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 6
            Repeater {
                model: window.pages
                Button {
                    required property string modelData
                    width: Math.max(74, implicitContentWidth + leftPadding + rightPadding)
                    text: modelData
                    checkable: true
                    checked: session.view === modelData
                    onClicked: selectPage(modelData)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            StatusPill { label: session.mode }
            StatusPill { label: window.connectionStatus }
            Button { text: session.connectionState === "Connected" ? "Disconnect" : "Connect"; enabled: session.connectionState !== "Connecting"; onClicked: session.connectionState === "Connected" ? session.disconnectFromTarget() : session.connectToTarget() }
            Item { Layout.fillWidth: true }
        }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: window.connectionFailed
            text: session.connectionState
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 3
            elide: Text.ElideRight
            color: "#ff9b8e"
            Accessible.name: text
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            currentIndex: Math.max(0, window.pages.indexOf(session.view))
            Rectangle {
                color: "#1b2933"; radius: 12; border.color: "#2b3c47"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Label { text: "Overview"; color: window.foreground; font.pixelSize: 20; font.bold: true }
                    Label { text: session.mode + " · " + session.preset + " · " + session.target + " · domain " + session.rosDomain + " · " + session.robot; color: window.muted }
                    Label { text: "Connection: " + window.connectionStatus; color: window.foreground }
                    Label { text: "Managed processes"; color: window.foreground; font.bold: true }
                    Label { visible: processes.processes.length === 0; text: "No managed processes"; color: window.muted }
                    Repeater {
                        model: processes.processes
                        delegate: Label { required property var modelData; text: modelData.name + " · " + modelData.state + (modelData.host ? " · " + modelData.host : ""); color: window.foreground }
                    }
                    Label { visible: session.lastError.length > 0; text: session.lastError; color: "#ff9b8e"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                }
            }
            Panel { heading: "Visualization" }
            Rectangle {
                color: "transparent"
                ScrollView {
                    anchors.fill: parent; clip: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Panels.SimBaseJog { Layout.fillWidth: true; Layout.preferredHeight: 260; visible: session.mode === "Simulation" }
                        Panels.UrShutdown { Layout.fillWidth: true; Layout.preferredHeight: 300; controller: urShutdown; visible: session.mode === "Hardware" }
                        Panels.UrFreedrive { Layout.fillWidth: true; Layout.preferredHeight: 150; controller: urFreedrive; visible: session.mode === "Hardware" }
                        Panels.UrMotionActions { Layout.fillWidth: true; Layout.preferredHeight: 480; controller: urMotionActions; visible: session.mode === "Hardware" }
                        Panels.UrArmJog { Layout.fillWidth: true; Layout.preferredHeight: 610; controller: urArmJog; visible: session.mode === "Hardware" }
                        Panel { Layout.fillWidth: true; Layout.preferredHeight: 100; heading: "Robot controls are available in the hardware arm panel"; visible: session.mode !== "Simulation" && session.view !== "Robot" }
                        Panels.HardwareStart { Layout.fillWidth: true; Layout.preferredHeight: 770; workflow: hardwareStart; sessionController: session; processManager: processes; visible: session.mode === "Hardware" }
                        Panels.MirHardwareControl {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 470
                            visible: session.mode === "Hardware"
                        }
                        Panels.LiftPanel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 300
                            controller: liftController
                            sessionController: session
                        }
                        Panels.GoalsPoses { Layout.fillWidth: true; Layout.preferredHeight: 440; goals: goalsPoses; sessionController: session }
                    }
                }
            }
            Panels.ProcessesPanel { processManager: processes }
            Rectangle {
                color: "#1b2933"; radius: 12; border.color: "#2b3c47"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Label { text: "ROS graph diagnosis"; color: window.foreground; font.pixelSize: 20; font.bold: true }
                    Label { text: session.target + " · ROS domain " + session.rosDomain + " · " + session.robot + " · " + window.connectionStatus; color: window.muted }
                    ScrollView {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        Column {
                            width: parent.width; spacing: 8
                            Repeater {
                                model: session.diagnostics
                                delegate: Label {
                                    required property string modelData
                                    width: parent.width; text: modelData; color: window.foreground
                                    wrapMode: Text.WrapAnywhere; font.pixelSize: 14
                                    Accessible.name: modelData
                                }
                            }
                        }
                    }
                }
            }
            Panel { heading: "Logs" }
            Rectangle {
                color: "#1b2933"; radius: 12; border.color: "#2b3c47"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Label { text: "Profiles"; color: window.foreground; font.pixelSize: 20; font.bold: true }
                    Label { Layout.fillWidth: true; text: "Edit and save the selected profile. Workspace and RViz paths are machine-local references."; color: window.muted; wrapMode: Text.WordWrap }
                    RowLayout {
                        Button { text: "Edit profile"; onClicked: { profileName.text = session.preset; profileMode.currentIndex = profileMode.model.indexOf(session.mode); profileTarget.text = session.target; profileDomain.text = String(session.rosDomain); profileRobot.text = session.robot; profileVisualization.currentIndex = profileVisualization.model.indexOf(session.visualization); profileWorkspace.text = session.workspacePath; profileConfig.text = session.configPath; profileGazeboOnStart.checked = session.gazeboViewOnSimulationStart; navGoal.text = session.navigationGoal; navCancel.text = session.navigationCancel; navPose.text = session.navigationPose; simTopic.text = session.simCommandTopic; mirTopic.text = session.mirCommandTopic; mirType.currentIndex = mirType.model.indexOf(session.mirCommandType); profileDialog.open() } }
                        Button { text: "Export preset…"; onClicked: exportDialog.open() }
                        Button { text: "Import preset…"; onClicked: importFileDialog.open() }
                    }
                    CheckBox {
                        text: "Gazebo world view on simulation start"
                        checked: session.gazeboViewOnSimulationStart
                        onToggled: session.setGazeboViewOnSimulationStart(checked)
                        Accessible.name: text
                    }
                    Label { Layout.fillWidth: true; text: "Opens the separately managed client when the app's simulation launch is running or /clock is discovered in the selected ROS domain."; color: window.muted; wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: session.lastError; visible: session.lastError.length > 0; color: "#ff9b8e"; wrapMode: Text.WordWrap; Accessible.name: session.lastError }
                    Item { Layout.fillHeight: true }
                }
            }
            Panels.MocapPanel { controller: mocap; sessionController: session }
        }
    }
    Dialog {
        id: presetDialog; title: "Create preset"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        contentItem: TextField { id: presetName; placeholderText: "Preset name" }
        onAccepted: { if (presetName.text.trim().length) session.selectPreset(presetName.text.trim()); presetName.clear() }
    }
    Dialog {
        id: profileDialog; title: "Edit profile"; modal: true; standardButtons: Dialog.Cancel
        anchors.centerIn: parent; width: Math.min(window.width - 48, 520)
        contentItem: ColumnLayout {
            TextField { id: profileName; Layout.fillWidth: true; placeholderText: "Profile name" }
            ComboBox { id: profileMode; Layout.fillWidth: true; model: ["Simulation", "Hardware"] }
            TextField { id: profileTarget; Layout.fillWidth: true; placeholderText: "Target host" }
            TextField { id: profileDomain; Layout.fillWidth: true; placeholderText: "ROS domain (0–232)"; validator: IntValidator { bottom: 0; top: 232 } }
            TextField { id: profileRobot; Layout.fillWidth: true; placeholderText: "Robot" }
            ComboBox { id: profileVisualization; Layout.fillWidth: true; model: ["RViz", "MuR data view"] }
            TextField { id: profileWorkspace; Layout.fillWidth: true; placeholderText: "Workspace path (machine-local)" }
            TextField { id: profileConfig; Layout.fillWidth: true; placeholderText: "RViz config path (machine-local)" }
            CheckBox { id: profileGazeboOnStart; text: "Gazebo world view on simulation start" }
            Label { Layout.fillWidth: true; text: "Navigation: Nav2 action / PoseStamped source in Simulation; MiR goal / cancel / pose topics in Hardware."; color: window.muted; wrapMode: Text.WordWrap }
            TextField { id: navGoal; Layout.fillWidth: true; placeholderText: "Navigation goal topic or action (optional)" }
            TextField { id: navCancel; Layout.fillWidth: true; placeholderText: "Hardware navigation cancel topic (optional)" }
            TextField { id: navPose; Layout.fillWidth: true; placeholderText: "Navigation pose topic (optional)" }
            TextField { id: simTopic; Layout.fillWidth: true; placeholderText: "Simulation TwistStamped command topic (optional)" }
            TextField { id: mirTopic; Layout.fillWidth: true; placeholderText: "MiR command topic (optional, absolute)" }
            ComboBox { id: mirType; Layout.fillWidth: true; model: ["", "geometry_msgs/msg/Twist", "geometry_msgs/msg/TwistStamped"] }
            Button { text: "Save profile"; Layout.alignment: Qt.AlignRight; onClicked: if (session.validateEndpoints(profileRobot.text, mirTopic.text, mirType.currentText, simTopic.text, navGoal.text, navCancel.text, navPose.text) && session.saveProfile(profileName.text, profileMode.currentText, profileTarget.text, profileRobot.text, profileVisualization.currentText, profileWorkspace.text, profileConfig.text, Number(profileDomain.text), profileGazeboOnStart.checked) && session.setMirEndpoint(mirTopic.text, mirType.currentText) && session.setSimEndpoint(simTopic.text) && session.setNavigationEndpoints(navGoal.text, navCancel.text, navPose.text)) profileDialog.close() }
        }
    }
    NativeDialogs.FileDialog {
        id: exportDialog; title: "Export portable preset"; selectExisting: false
        nameFilters: ["MuR preset bundle (*.murpreset.json)", "JSON files (*.json)"]
        onAccepted: session.exportPreset(fileUrl)
    }
    NativeDialogs.FileDialog {
        id: importFileDialog; title: "Choose portable preset"; selectExisting: true
        nameFilters: ["MuR preset bundle (*.murpreset.json *.json)", "JSON files (*.json)"]
        onAccepted: { importSource.text = fileUrl.toString(); importDialog.open() }
    }
    Dialog {
        id: importDialog; title: "Import preset and map local paths"; modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent; width: Math.min(window.width - 48, 520)
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: "Map paths from the exporting machine to local files. Leave a field blank to import that reference as unassigned."; color: window.muted; wrapMode: Text.WordWrap }
            TextField { id: importSource; Layout.fillWidth: true; visible: false }
            TextField { id: workspaceMapping; Layout.fillWidth: true; placeholderText: "Local workspace path (optional)" }
            TextField { id: configMapping; Layout.fillWidth: true; placeholderText: "Local RViz config path (optional)" }
        }
        onAccepted: session.importPreset(importSource.text, workspaceMapping.text, configMapping.text)
    }
}
