import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3 as NativeDialogs

Rectangle {
    id: root
    required property var goals
    required property var sessionController
    color: theme.surface; radius: 12; border.color: theme.line
    implicitHeight: Math.max(440, controls.implicitHeight + 36)
    ColumnLayout {
        id: controls
        anchors.fill: parent; anchors.margins: 18; spacing: 10
        Label { text: "Goals and saved poses"; color: theme.ink; font.pixelSize: 20; font.bold: true }
        Label { Layout.fillWidth: true; text: goals.adapterStatus(sessionController.robot, sessionController.mode); color: theme.muted; wrapMode: Text.WordWrap }
        Label { Layout.fillWidth: true; text: "Selected robot: " + sessionController.robot + " · frame: " + frame.text + " · position [m] · yaw [deg]"; color: theme.muted }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: poseName; Layout.fillWidth: true; placeholderText: "Pose name"; Accessible.name: "Pose name" }
            TextField { id: frame; Layout.preferredWidth: 130; text: "map"; placeholderText: "Frame"; Accessible.name: "Pose frame" }
            TextField { id: x; Layout.preferredWidth: 100; text: "0"; placeholderText: "X [m]"; validator: DoubleValidator { bottom: -1000; top: 1000 } }
            TextField { id: y; Layout.preferredWidth: 100; text: "0"; placeholderText: "Y [m]"; validator: DoubleValidator { bottom: -1000; top: 1000 } }
            TextField { id: yaw; Layout.preferredWidth: 100; text: "0"; placeholderText: "Yaw [deg]"; validator: DoubleValidator { bottom: -180; top: 180 } }
        }
        RowLayout {
            Button { text: "Drive coordinates"; enabled: !goals.busy; onClicked: goals.send(sessionController.robot, sessionController.mode, poseName.text, frame.text, Number(x.text), Number(y.text), Number(yaw.text)) }
            Button { text: "Save / update"; onClicked: goals.save(poseName.text, frame.text, Number(x.text), Number(y.text), Number(yaw.text)) }
            Button { text: "Save current robot pose"; enabled: !goals.busy; onClicked: goals.saveCurrent(sessionController.robot, sessionController.mode, poseName.text) }
            Item { Layout.fillWidth: true }
            Button { text: "Import MiR poses…"; onClicked: importFileDialog.open() }
            Button { text: "Reload"; onClicked: goals.reload() }
        }
        RowLayout {
            Layout.fillWidth: true
            ListView {
                id: poseList
                Layout.fillWidth: true; Layout.preferredHeight: 130; clip: true
                model: goals.poses
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    text: modelData.name + "  (" + modelData.frame_id + ": " + Number(modelData.x).toFixed(3) + ", " + Number(modelData.y).toFixed(3) + ", " + Number(modelData.yaw_deg || modelData.yaw || 0).toFixed(1) + "°)"
                    onClicked: { poseList.currentIndex = index; poseName.text = modelData.name; frame.text = modelData.frame_id; x.text = modelData.x; y.text = modelData.y; yaw.text = modelData.yaw_deg || modelData.yaw || 0 }
                }
            }
            ColumnLayout {
                Button { text: "Drive selected"; enabled: poseList.currentIndex >= 0 && !goals.busy; onClicked: goals.sendSaved(sessionController.robot, sessionController.mode, goals.poses[poseList.currentIndex].name) }
                Button { text: "Delete selected"; enabled: poseList.currentIndex >= 0; onClicked: goals.remove(goals.poses[poseList.currentIndex].name) }
                Button { text: "Cancel navigation"; enabled: !goals.busy; onClicked: goals.cancelSelected(sessionController.robot, sessionController.mode) }
            }
        }
        Label { Layout.fillWidth: true; text: goals.status; color: goals.status.toLowerCase().indexOf("failed") >= 0 || goals.status.toLowerCase().indexOf("rejected") >= 0 || goals.status.toLowerCase().indexOf("unavailable") >= 0 || goals.status.toLowerCase().indexOf("not sent") >= 0 || goals.status.toLowerCase().indexOf("did not accept") >= 0 || goals.status.toLowerCase().indexOf("stale") >= 0 || goals.status.indexOf("Could not") >= 0 ? theme.danger : theme.muted; wrapMode: Text.WordWrap; Accessible.name: goals.status }
    }
    NativeDialogs.FileDialog {
        id: importFileDialog
        title: "Import existing MiR poses"
        selectExisting: true
        nameFilters: ["Pose JSON files (*.json)", "All files (*)"]
        onAccepted: goals.importFile(fileUrl.toString())
    }
}
