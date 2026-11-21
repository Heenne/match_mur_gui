import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Item {
    id: page
    required property var sessionController
    required property var processManager
    readonly property var runs: processManager.processes
    signal pageRequested(string name)
    implicitHeight: content.implicitHeight

    Column {
        id: content
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        Rectangle {
            width: parent.width
            height: summary.implicitHeight + 36
            radius: 10
            color: theme.surface
            border.color: theme.line

            Column {
                id: summary
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 18
                spacing: 8

                Label { text: "Overview"; color: theme.muted; font.weight: Font.DemiBold }
                Label {
                    width: parent.width
                    text: page.sessionController.mode === "Simulation" ? "Simulation workspace" : "Hardware workspace"
                    color: theme.ink
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Label {
                    width: parent.width
                    color: theme.muted
                    text: page.sessionController.mode === "Simulation"
                          ? "Start and monitor the selected robot in Gazebo. Use Diagnosis to check its ROS graph."
                          : "Check the selected robot and its ROS graph before starting hardware workflows."
                    wrapMode: Text.WordWrap
                }
                Flow {
                    width: parent.width
                    spacing: 8
                    ActionButton { text: "Open processes"; primary: true; onClicked: page.pageRequested("Processes") }
                    ActionButton { text: "Open diagnosis"; onClicked: page.pageRequested("Diagnosis") }
                }
            }
        }

        Flow {
            id: metrics
            width: parent.width
            spacing: 8
            readonly property int columns: width >= 560 ? 4 : 2
            Repeater {
                model: [
                    {name: "Preset", value: page.sessionController.preset},
                    {name: "Robot", value: page.sessionController.robot},
                    {name: "Target", value: page.sessionController.target},
                    {name: "ROS domain", value: String(page.sessionController.rosDomain)}
                ]
                delegate: Rectangle {
                    required property var modelData
                    width: Math.floor((metrics.width - (metrics.columns - 1) * metrics.spacing) / metrics.columns)
                    height: 68
                    radius: 8
                    color: theme.surface
                    border.color: theme.line

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 3
                        Label { text: modelData.name; color: theme.muted; font.pixelSize: 12 }
                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: modelData.value
                            color: theme.ink
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Accessible.name: text
                        }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: processesLayout.implicitHeight + 28
            radius: 8
            color: theme.surface
            border.color: theme.line

            Column {
                id: processesLayout
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 14
                spacing: 8
                Label { text: "Managed processes"; color: theme.ink; font.weight: Font.DemiBold }
                Label { visible: page.runs.length === 0; text: "No managed processes"; color: theme.muted }
                Repeater {
                    model: page.runs
                    delegate: RowLayout {
                        required property var modelData
                        width: processesLayout.width
                        Label { Layout.fillWidth: true; text: modelData.name; color: theme.ink; elide: Text.ElideRight }
                        StatusBadge {
                            label: modelData.state
                            foreground: modelData.state === "Running" ? theme.accent : theme.muted
                            color: modelData.state === "Running" ? theme.selected : theme.raised
                        }
                    }
                }
            }
        }
        Label {
            visible: page.sessionController.lastError.length > 0
            width: parent.width
            text: page.sessionController.lastError
            color: theme.danger
            wrapMode: Text.WrapAnywhere
        }
    }
}
