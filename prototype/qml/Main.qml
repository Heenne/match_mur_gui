import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

ApplicationWindow {
    id: window
    visible: true
    width: 1440
    height: 900
    minimumWidth: 840
    minimumHeight: 560
    title: "MuR Command Center · Layout prototype"
    color: "#101820"

    property int variant: 1
    property int selectedLog: 0
    property string mode: "Simulation"
    property string preset: "Local simulation"
    property string robot: "mur620d"
    property var variants: ["Trial field", "Development", "Split work"]
    property color surface: "#1b2933"
    property color muted: "#a8bac5"
    property color accent: "#45d6a8"

    function pillColor(label) {
        if (label === "Warning") return "#f0b44d"
        if (label === "Connected" || label === "Running" || label === "Base online" || label === "Left arm ready") return accent
        return "#5c7180"
    }

    component Pill: Rectangle {
        required property string label
        property color fill: window.pillColor(label)
        radius: 12
        color: fill
        implicitWidth: pillText.implicitWidth + 20
        implicitHeight: 28
        Label { id: pillText; anchors.centerIn: parent; text: parent.label; color: "#102029"; font.pixelSize: 12; font.bold: true }
    }

    component Panel: Rectangle {
        default property alias contents: body.data
        property string heading: ""
        color: window.surface
        radius: 12
        border.color: "#2b3c47"
        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12
            Label { text: parent.parent.heading; color: "#f2f6f8"; font.pixelSize: 17; font.bold: true }
        }
    }

    component SessionPanel: Panel {
        heading: "Robot status"
        Layout.fillWidth: true
        Layout.fillHeight: true
        ColumnLayout {
            Layout.fillWidth: true
            Label { text: window.robot + " · online"; color: "#f2f6f8"; font.pixelSize: 23; font.bold: true }
            RowLayout { spacing: 8; Pill { label: "Base online" }; Pill { label: "Left arm ready" } }
            Label { text: "Battery · 82%     Pose · Map frame"; color: window.muted }
            Label { text: "Sim time · 00:18:42     Clock advancing"; color: window.muted }
            Item { Layout.fillHeight: true }
            Button { text: "Open robot controls"; Layout.fillWidth: true }
        }
    }

    component VisualizationPanel: Panel {
        heading: "Visualization"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#d9e1e5"
            Label { anchors.centerIn: parent; text: "RViz preview\nMap · mur620d · TF"; color: "#26343d"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 19 }
        }
    }

    component LogsPanel: Panel {
        heading: "Launch logs"
        Layout.fillWidth: true
        Layout.fillHeight: true
        RowLayout {
            Layout.fillWidth: true
            Button { text: "Gazebo · gazebo.log"; checkable: true; checked: window.selectedLog === 0; onClicked: window.selectedLog = 0 }
            Button { text: "Bringup · bringup.log"; checkable: true; checked: window.selectedLog === 1; onClicked: window.selectedLog = 1 }
        }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: window.selectedLog === 0 ? "[INFO] world loaded\n[INFO] /clock advancing\n[INFO] spawn mur620d complete" : "[INFO] robot_state_publisher ready\n[WARN] Controller discovery delayed\n[INFO] arm controllers active"; color: "#b8d6c6"; font.family: "monospace"; wrapMode: Text.Wrap; verticalAlignment: Text.AlignTop; padding: 12; background: Rectangle { color: "#111d24"; radius: 8 } }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 3
                Label { text: "MuR Command Center"; color: "#f2f6f8"; font.pixelSize: 25; font.bold: true }
                Label { text: "Prototype · shared session context across three layouts"; color: window.muted }
            }
            Item { Layout.fillWidth: true }
            ComboBox { model: ["Simulation", "Hardware"]; currentIndex: mode === "Simulation" ? 0 : 1; onActivated: window.mode = currentText }
            ComboBox { model: ["Local simulation", "Trial field", "Development"]; currentIndex: model.indexOf(window.preset); onActivated: window.preset = currentText }
            ComboBox { model: ["mur620d", "mur620b"]; currentIndex: model.indexOf(window.robot); onActivated: window.robot = currentText }
        }
        RowLayout {
            Layout.fillWidth: true
            Pill { label: window.mode }
            Pill { label: "Connected" }
            Pill { label: "Running" }
            Label { text: window.preset + "  ·  localhost  ·  ROS 2 Domain 42"; color: window.muted }
            Item { Layout.fillWidth: true }
            Repeater {
                model: window.variants
                Button {
                    required property string modelData
                    text: modelData
                    checkable: true
                    checked: index === window.variant
                    onClicked: window.variant = index
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            radius: 8
            color: "#493920"
            border.color: "#a77d36"
            implicitHeight: warningLabel.implicitHeight + 20
            Label { id: warningLabel; anchors.fill: parent; anchors.margins: 10; text: "⚠ Controller discovery delayed — inspect Bringup log"; color: "#ffd782"; font.bold: true; verticalAlignment: Text.AlignVCenter }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.variant
            RowLayout { spacing: 12; SessionPanel { Layout.preferredWidth: 300 }; VisualizationPanel { Layout.fillWidth: true }; LogsPanel { Layout.preferredWidth: 390 } }
            ColumnLayout { spacing: 12; RowLayout { Layout.fillWidth: true; VisualizationPanel { Layout.fillWidth: true }; SessionPanel { Layout.preferredWidth: 320 } }; LogsPanel { Layout.fillWidth: true; Layout.preferredHeight: 260 } }
            RowLayout { spacing: 12; VisualizationPanel { Layout.fillWidth: true }; LogsPanel { Layout.fillWidth: true } }
        }
    }
}
