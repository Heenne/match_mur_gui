import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Item {
    id: window
    width: 1440
    height: 900
    implicitWidth: 1440
    implicitHeight: 900
    property var pages: ["Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings"]
    property color muted: "#a8bac5"
    property color foreground: "#f2f6f8"

    function statusColor(status) {
        if (status === "Disconnected") return "#f0b44d"
        if (status.indexOf("Failed") === 0 || status === "Error") return "#f07178"
        if (status === "Connecting") return "#f0b44d"
        return "#45d6a8"
    }
    function selectPage(page) { session.selectView(page) }

    Keys.onPressed: {
        if (event.key >= Qt.Key_1 && event.key <= Qt.Key_7) {
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
                Label { text: "MuR Command Center"; color: window.foreground; font.pixelSize: 25; font.bold: true }
                Label { text: session.mode + " · " + session.preset + " · " + session.target + " · domain " + session.rosDomain + " · " + session.robot; color: window.muted }
            }
            Item { Layout.fillWidth: true }
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
        RowLayout {
            Layout.fillWidth: true
            StatusPill { label: session.mode }
            StatusPill { label: session.connectionState }
            Button { text: session.connectionState === "Connected" ? "Disconnect" : "Connect"; enabled: session.connectionState !== "Connecting"; onClicked: session.connectionState === "Connected" ? session.disconnectFromTarget() : session.connectToTarget() }
            Label { text: session.connectionState + " · existing processes are not managed"; color: window.muted }
            Item { Layout.fillWidth: true }
            Repeater {
                model: window.pages
                Button { required property string modelData; text: modelData; checkable: true; checked: session.view === modelData; onClicked: selectPage(modelData) }
            }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: Math.max(0, window.pages.indexOf(session.view))
            Panel { heading: "Overview" }
            Panel { heading: "Visualization" }
            Panel { heading: "Robot" }
            Panel { heading: "Processes" }
            Rectangle {
                color: "#1b2933"; radius: 12; border.color: "#2b3c47"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Label { text: "ROS graph diagnosis"; color: window.foreground; font.pixelSize: 20; font.bold: true }
                    Label { text: session.target + " · ROS domain " + session.rosDomain + " · " + session.robot + " · " + session.connectionState; color: window.muted }
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
            Panel { heading: "Settings" }
        }
        Label { text: "Disconnected · status is shown by text and color"; color: "#f0b44d"; Accessible.name: "Disconnected" }
    }
    Dialog {
        id: presetDialog; title: "Create preset"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        contentItem: TextField { id: presetName; placeholderText: "Preset name" }
        onAccepted: { if (presetName.text.trim().length) session.selectPreset(presetName.text.trim()); presetName.clear() }
    }
}
