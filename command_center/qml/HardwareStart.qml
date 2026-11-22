import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    implicitHeight: controls.implicitHeight + 36
    id: root
    required property var workflow
    required property var sessionController
    required property var processManager
    color: theme.surface
    radius: 12
    border.color: theme.line

    function saveOption(name, value) { workflow.setOption(name, value) }
    function boolOption(name) { return workflow.options[name] === undefined ? false : workflow.options[name] }

    ColumnLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Hardware start"); color: theme.ink; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            Rectangle {
                radius: 10
                color: workflow.state === "Ready" || workflow.state === "Accepted" ? theme.selected :
                       workflow.state === "Blocked" || workflow.state === "Failed" ? theme.dangerSurface : theme.warningSurface
                implicitWidth: stateLabel.implicitWidth + 20
                implicitHeight: stateLabel.implicitHeight + 10
                Label { id: stateLabel; anchors.centerIn: parent; text: workflow.state; color: theme.ink; font.bold: true }
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("%1 · %2 · %3 · domain %4").arg(uiText(sessionController.mode)).arg(sessionController.target).arg(sessionController.robot).arg(sessionController.rosDomain)
            color: theme.muted
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("A confirmed preflight is required before launch. Unknown or missing UR safety state blocks the start. This app does not clear safety stops.")
            color: theme.warning
            wrapMode: Text.WordWrap
        }

        GroupBox {
            title: qsTr("Components and launch options")
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    CheckBox { text: qsTr("Left UR arm"); checked: root.boolOption("arm_l"); onToggled: root.saveOption("arm_l", checked) }
                    CheckBox { text: qsTr("Right UR arm"); checked: root.boolOption("arm_r"); onToggled: root.saveOption("arm_r", checked) }
                    CheckBox { text: qsTr("MiR base"); checked: root.boolOption("launch_mir"); onToggled: root.saveOption("launch_mir", checked) }
                    CheckBox { text: qsTr("MiR cameras"); checked: root.boolOption("launch_cameras"); onToggled: root.saveOption("launch_cameras", checked) }
                }
                RowLayout {
                    CheckBox { text: qsTr("Integrated Cartesian controller"); checked: root.boolOption("integrated_controller"); onToggled: root.saveOption("integrated_controller", checked) }
                    CheckBox { text: qsTr("FT sensor"); checked: root.boolOption("use_ft_sensor"); onToggled: root.saveOption("use_ft_sensor", checked) }
                    CheckBox { text: qsTr("Require wrench"); checked: root.boolOption("require_wrench"); onToggled: root.saveOption("require_wrench", checked) }
                }
                RowLayout {
                    CheckBox { text: qsTr("Collision avoidance"); checked: root.boolOption("collision_avoidance"); onToggled: root.saveOption("collision_avoidance", checked) }
                    CheckBox { text: qsTr("Collision markers"); checked: root.boolOption("collision_markers"); onToggled: root.saveOption("collision_markers", checked) }
                    CheckBox { text: qsTr("Zero admittance"); checked: root.boolOption("zero_admittance"); onToggled: root.saveOption("zero_admittance", checked) }
                    CheckBox { text: qsTr("Launch MoveIt"); checked: root.boolOption("launch_moveit"); onToggled: root.saveOption("launch_moveit", checked) }
                }
                RowLayout {
                    Label { text: qsTr("Localization for next start"); color: theme.ink }
                    ComboBox {
                        id: localizationSource
                        enabled: root.boolOption("launch_mir")
                        textRole: "label"
                        model: [ {label: qsTr("MiR controller"), value: "mir"},
                                 {label: "Qualisys / Mocap", value: "mocap"},
                                 {label: qsTr("AMCL · saved map"), value: "amcl"} ]
                        currentIndex: root.workflow.options.localization_source === "mocap" ? 1 : root.workflow.options.localization_source === "amcl" ? 2 : 0
                        onActivated: root.saveOption("localization_source", model[currentIndex].value)
                    }
                    Label { text: root.workflow.localizationState; color: theme.warning }
                    Button { text: qsTr("Prepare AMCL"); enabled: !root.workflow.busy && root.workflow.options.localization_source === "amcl" && root.sessionController.connectionState === "Connected"; onClicked: root.workflow.prepareAmcl() }
                }
                Label { Layout.fillWidth: true; text: qsTr("Active localization: %1").arg(root.workflow.activeLocalization); color: theme.ink; wrapMode: Text.WordWrap }
                Label { Layout.fillWidth: true; text: root.workflow.localizationDetail; color: theme.muted; wrapMode: Text.WordWrap }
                RowLayout {
                    Label { text: qsTr("MoveIt speed: %1%").arg(root.workflow.options.moveit_speed_percent); color: theme.ink }
                    Slider {
                        Layout.fillWidth: true
                        from: 1; to: 100; stepSize: 1
                        value: root.workflow.options.moveit_speed_percent
                        onMoved: root.saveOption("moveit_speed_percent", Math.round(value))
                    }
                }
                RowLayout {
                    Label { text: qsTr("Remote workspace"); color: theme.ink }
                    TextField {
                        Layout.fillWidth: true
                        text: root.workflow.options.remote_workspace
                        placeholderText: "$HOME/colcon_ws"
                        onEditingFinished: root.saveOption("remote_workspace", text.trim())
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Run preflight"); enabled: !root.workflow.busy && root.sessionController.mode === "Hardware" && root.sessionController.connectionState === "Connected"; onClicked: root.workflow.runPreflight() }
            Button { text: qsTr("Start selected hardware…"); enabled: !root.workflow.busy && root.workflow.preflightReady; onClicked: startDialog.open() }
            Button { text: qsTr("Stop owned launch"); enabled: root.processState() === "Running" || root.processState() === "Starting" || root.processState() === "Connecting"; onClicked: root.processManager.stopProcess("Hardware · " + root.sessionController.robot) }
            Item { Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Sync source…"); enabled: !root.workflow.busy && root.sessionController.connectionState === "Connected" && root.sessionController.workspacePath.length > 0; onClicked: syncDialog.open() }
            Button { text: qsTr("Build workspace"); enabled: !root.workflow.busy && root.sessionController.connectionState === "Connected"; onClicked: root.workflow.buildWorkspace() }
            Label { Layout.fillWidth: true; text: qsTr("Sync and build are explicit, separate actions; launch does not run either implicitly."); color: theme.muted; wrapMode: Text.WordWrap }
        }

        Label { Layout.fillWidth: true; text: root.workflow.message; color: root.workflow.state === "Blocked" || root.workflow.state === "Failed" ? theme.danger : theme.ink; wrapMode: Text.WordWrap }
        Repeater {
            model: root.workflow.checks
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: checkRow.implicitHeight + 14
                radius: 8
                color: theme.canvas
                RowLayout {
                    id: checkRow
                    anchors.fill: parent; anchors.margins: 8; spacing: 10
                    Rectangle {
                        radius: 8; implicitWidth: state.implicitWidth + 16; implicitHeight: state.implicitHeight + 8
                        color: modelData.state === "Ready" ? theme.selected : modelData.state === "Blocked" ? theme.dangerSurface : modelData.state === "Unknown" ? theme.warningSurface : theme.warningSurface
                        Label { id: state; anchors.centerIn: parent; text: modelData.state; color: theme.ink; Accessible.name: modelData.state }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.name; color: theme.ink; font.bold: true }
                        Label { Layout.fillWidth: true; text: modelData.detail; color: theme.muted; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }

    function processState() {
        var name = "Hardware · " + sessionController.robot
        for (var i = 0; i < processManager.processes.length; ++i)
            if (processManager.processes[i].name === name) return processManager.processes[i].state
        return "Stopped"
    }

    AdaptiveMessageDialog {
        id: startDialog
        title: qsTr("Start hardware?")
        message: qsTr("Start the selected components on %1 for %2? All UR safety stops remain active.").arg(root.sessionController.target).arg(root.sessionController.robot)
        onAccepted: root.workflow.startHardware()
    }
    AdaptiveMessageDialog {
        id: syncDialog
        title: qsTr("Synchronize source?")
        message: qsTr("Copy source files from %1/src to %2:%3/src? Matching remote files may be overwritten. Remote-only files are preserved.").arg(root.sessionController.workspacePath).arg(root.sessionController.target).arg(root.workflow.options.remote_workspace)
        onAccepted: root.workflow.syncSource()
    }
}
