import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    implicitHeight: controls.implicitHeight + 36
    property var controller
    color: theme.surface; radius: 12; border.color: theme.line
    ColumnLayout {
        id: controls
        anchors.fill: parent; anchors.margins: 18; spacing: 8
        Label { text: qsTr("Controlled UR shutdown"); color: theme.ink; font.pixelSize: 20; font.bold: true }
        Label { Layout.fillWidth: true; text: qsTr("Separate from arm stop and process stop. This powers down the selected UR control boxes; no parking pose is performed. Motion remains locked for these arms until the app is restarted."); color: theme.warning; wrapMode: Text.WordWrap }
        RowLayout {
            CheckBox { id: left; text: "UR10_l"; checked: controller && controller.leftSelected; enabled: controller && !controller.busy; onClicked: controller.setSelected("l", checked) }
            CheckBox { id: right; text: "UR10_r"; checked: controller && controller.rightSelected; enabled: controller && !controller.busy; onClicked: controller.setSelected("r", checked) }
            Button { text: qsTr("Review shutdown"); enabled: controller && !controller.busy; onClicked: if (controller.prepare()) confirmation.open() }
        }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: controller ? controller.results : qsTr("Unavailable"); color: theme.ink; wrapMode: Text.WordWrap }
    }
    Connections { target: controller; function onChanged() { left.checked = controller.leftSelected; right.checked = controller.rightSelected } }
    AdaptiveMessageDialog {
        id: confirmation
        title: qsTr("Confirm UR shutdown")
        message: qsTr("Shut down these selected control boxes? Secure workpieces first. Local operation is required to power on again.\n\n") + (controller ? controller.targets : "")
        onAccepted: controller.confirm()
    }
}
