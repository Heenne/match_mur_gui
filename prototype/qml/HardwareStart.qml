import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: root
    required property var workflow
    required property var sessionController
    required property var processManager
    color: "#1b2933"
    radius: 12
    border.color: "#2b3c47"

    function saveOption(name, value) { workflow.setOption(name, value) }
    function boolOption(name) { return workflow.options[name] === undefined ? false : workflow.options[name] }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: "Hardware start"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            Rectangle {
                radius: 10
                color: workflow.state === "Ready" || workflow.state === "Accepted" ? "#17634f" :
                       workflow.state === "Blocked" || workflow.state === "Failed" ? "#713b40" : "#69531f"
                implicitWidth: stateLabel.implicitWidth + 20
                implicitHeight: stateLabel.implicitHeight + 10
                Label { id: stateLabel; anchors.centerIn: parent; text: workflow.state; color: "white"; font.bold: true }
            }
        }
        Label {
            Layout.fillWidth: true
            text: sessionController.mode + " · " + sessionController.target + " · " + sessionController.robot + " · domain " + sessionController.rosDomain
            color: "#a8bac5"
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            text: "A confirmed preflight is required before launch. Unknown or missing UR safety state blocks the start. This app does not clear safety stops."
            color: "#f0b44d"
            wrapMode: Text.WordWrap
        }

        GroupBox {
            title: "Components and launch options"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    CheckBox { text: "Left UR arm"; checked: root.boolOption("arm_l"); onToggled: root.saveOption("arm_l", checked) }
                    CheckBox { text: "Right UR arm"; checked: root.boolOption("arm_r"); onToggled: root.saveOption("arm_r", checked) }
                    CheckBox { text: "MiR base"; checked: root.boolOption("launch_mir"); onToggled: root.saveOption("launch_mir", checked) }
                    CheckBox { text: "MiR cameras"; checked: root.boolOption("launch_cameras"); onToggled: root.saveOption("launch_cameras", checked) }
                }
                RowLayout {
                    CheckBox { text: "Integrated Cartesian controller"; checked: root.boolOption("integrated_controller"); onToggled: root.saveOption("integrated_controller", checked) }
                    CheckBox { text: "FT sensor"; checked: root.boolOption("use_ft_sensor"); onToggled: root.saveOption("use_ft_sensor", checked) }
                    CheckBox { text: "Require wrench"; checked: root.boolOption("require_wrench"); onToggled: root.saveOption("require_wrench", checked) }
                }
                RowLayout {
                    CheckBox { text: "Collision avoidance"; checked: root.boolOption("collision_avoidance"); onToggled: root.saveOption("collision_avoidance", checked) }
                    CheckBox { text: "Collision markers"; checked: root.boolOption("collision_markers"); onToggled: root.saveOption("collision_markers", checked) }
                    CheckBox { text: "Zero admittance"; checked: root.boolOption("zero_admittance"); onToggled: root.saveOption("zero_admittance", checked) }
                    CheckBox { text: "Launch MoveIt"; checked: root.boolOption("launch_moveit"); onToggled: root.saveOption("launch_moveit", checked) }
                }
                RowLayout {
                    Label { text: "Localization for next start"; color: "#dce6eb" }
                    ComboBox {
                        id: localizationSource
                        enabled: root.boolOption("launch_mir")
                        textRole: "label"
                        model: [ {label: "MiR controller", value: "mir"},
                                 {label: "Qualisys / Mocap", value: "mocap"},
                                 {label: "AMCL · saved map", value: "amcl"} ]
                        currentIndex: root.workflow.options.localization_source === "mocap" ? 1 : root.workflow.options.localization_source === "amcl" ? 2 : 0
                        onActivated: root.saveOption("localization_source", model[currentIndex].value)
                    }
                    Label { text: root.workflow.localizationState; color: "#f0b44d" }
                    Button { text: "Prepare AMCL"; enabled: !root.workflow.busy && root.workflow.options.localization_source === "amcl" && root.sessionController.connectionState === "Connected"; onClicked: root.workflow.prepareAmcl() }
                }
                Label { Layout.fillWidth: true; text: "Active localization: " + root.workflow.activeLocalization; color: "#dce6eb"; wrapMode: Text.WordWrap }
                Label { Layout.fillWidth: true; text: root.workflow.localizationDetail; color: "#a8bac5"; wrapMode: Text.WordWrap }
                RowLayout {
                    Label { text: "MoveIt speed: " + root.workflow.options.moveit_speed_percent + "%"; color: "#dce6eb" }
                    Slider {
                        Layout.fillWidth: true
                        from: 1; to: 100; stepSize: 1
                        value: root.workflow.options.moveit_speed_percent
                        onMoved: root.saveOption("moveit_speed_percent", Math.round(value))
                    }
                }
                RowLayout {
                    Label { text: "Remote workspace"; color: "#dce6eb" }
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
            Button { text: "Run preflight"; enabled: !root.workflow.busy && root.sessionController.mode === "Hardware" && root.sessionController.connectionState === "Connected"; onClicked: root.workflow.runPreflight() }
            Button { text: "Start selected hardware…"; enabled: !root.workflow.busy && root.workflow.preflightReady; onClicked: startDialog.open() }
            Button { text: "Stop owned launch"; enabled: root.processState() === "Running" || root.processState() === "Starting" || root.processState() === "Connecting"; onClicked: root.processManager.stopProcess("Hardware · " + root.sessionController.robot) }
            Item { Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: "Sync source…"; enabled: !root.workflow.busy && root.sessionController.connectionState === "Connected" && root.sessionController.workspacePath.length > 0; onClicked: syncDialog.open() }
            Button { text: "Build workspace"; enabled: !root.workflow.busy && root.sessionController.connectionState === "Connected"; onClicked: root.workflow.buildWorkspace() }
            Label { Layout.fillWidth: true; text: "Sync and build are explicit, separate actions; launch does not run either implicitly."; color: "#a8bac5"; wrapMode: Text.WordWrap }
        }

        Label { Layout.fillWidth: true; text: root.workflow.message; color: root.workflow.state === "Blocked" || root.workflow.state === "Failed" ? "#f07178" : "#c9d6dc"; wrapMode: Text.WordWrap }
        Repeater {
            model: root.workflow.checks
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: checkRow.implicitHeight + 14
                radius: 8
                color: "#14212a"
                RowLayout {
                    id: checkRow
                    anchors.fill: parent; anchors.margins: 8; spacing: 10
                    Rectangle {
                        radius: 8; implicitWidth: state.implicitWidth + 16; implicitHeight: state.implicitHeight + 8
                        color: modelData.state === "Ready" ? "#17634f" : modelData.state === "Blocked" ? "#713b40" : modelData.state === "Unknown" ? "#4b3f30" : "#69531f"
                        Label { id: state; anchors.centerIn: parent; text: modelData.state; color: "white"; Accessible.name: modelData.state }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.name; color: "#f2f6f8"; font.bold: true }
                        Label { Layout.fillWidth: true; text: modelData.detail; color: "#a8bac5"; wrapMode: Text.WordWrap }
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

    Dialog {
        id: startDialog
        modal: true
        width: 520
        title: "Start hardware?"
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            width: 480
            text: "Start only the selected components on " + root.sessionController.target + " for " + root.sessionController.robot + "? The app will not clear any UR safety stop."
            color: "#f2f6f8"; wrapMode: Text.WordWrap; padding: 18
        }
        onAccepted: root.workflow.startHardware()
    }
    Dialog {
        id: syncDialog
        modal: true
        width: 520
        title: "Synchronize source?"
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            width: 480
            text: "Copy source files from " + root.sessionController.workspacePath + "/src to " + root.sessionController.target + ":" + root.workflow.options.remote_workspace + "/src. Existing matching remote files may be overwritten; remote-only files are preserved."
            color: "#f2f6f8"; wrapMode: Text.WordWrap; padding: 18
        }
        onAccepted: root.workflow.syncSource()
    }
}
