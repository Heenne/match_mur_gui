import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    property var controller
    color: "#1b2933"; radius: 12; border.color: "#2b3c47"
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 8
        Label { text: "Controlled UR shutdown"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
        Label { Layout.fillWidth: true; text: "Separate from arm stop and process stop. This powers down the selected UR control boxes; no parking pose is performed. Motion remains locked for these arms until the app is restarted."; color: "#f0b44d"; wrapMode: Text.WordWrap }
        RowLayout {
            CheckBox { id: left; text: "UR10_l"; checked: controller && controller.leftSelected; enabled: controller && !controller.busy; onClicked: controller.setSelected("l", checked) }
            CheckBox { id: right; text: "UR10_r"; checked: controller && controller.rightSelected; enabled: controller && !controller.busy; onClicked: controller.setSelected("r", checked) }
            Button { text: "Review shutdown"; enabled: controller && !controller.busy; onClicked: if (controller.prepare()) confirmation.open() }
        }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: controller ? controller.results : "Unavailable"; color: "#dce6eb"; wrapMode: Text.WordWrap }
    }
    Connections { target: controller; function onChanged() { left.checked = controller.leftSelected; right.checked = controller.rightSelected } }
    Dialog {
        id: confirmation
        title: "Confirm UR shutdown"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: controller.confirm()
        contentItem: Label { text: "Shut down these selected control boxes? Secure workpieces first. Local operation is required to power on again.\n\n" + (controller ? controller.targets : ""); wrapMode: Text.WordWrap; width: 460 }
    }
}
