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
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Home and TCP alignment"); color: theme.ink; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            ComboBox {
                model: ["UR10_r", "UR10_l"]
                currentIndex: controller && controller.selectedArm === "UR10_l" ? 1 : 0
                enabled: controller && !controller.busy
                onActivated: controller.selectArm(currentIndex === 1 ? "l" : "r")
                Accessible.name: qsTr("Arm for Home and alignment")
            }
        }
        Label { Layout.fillWidth: true; text: controller ? controller.readiness : qsTr("Unavailable"); color: controller && controller.ready ? theme.focus : theme.warning; wrapMode: Text.WordWrap }
        RowLayout {
            Label { text: qsTr("Home pose"); color: theme.ink }
            TextField { id: home; text: controller ? controller.homePose : ""; enabled: controller && !controller.busy; onEditingFinished: controller.setHomePose(text); Accessible.name: qsTr("Named Home pose") }
            Label { text: qsTr("Speed %1%").arg(Math.round(controller ? controller.velocityScaling * 100 : 0)); color: theme.ink }
            Slider { from: 1; to: 100; value: controller ? controller.velocityScaling * 100 : 20; enabled: controller && !controller.busy; onMoved: controller.setVelocityScaling(value) }
            Button { text: qsTr("Move Home"); enabled: controller && controller.ready && !controller.busy; onClicked: { controller.setHomePose(home.text); controller.startHome() } }
        }
        RowLayout {
            Label { text: qsTr("TCP plane"); color: theme.ink }
            ComboBox { id: plane; model: ["XY", "XZ", "YZ"]; enabled: controller && !controller.busy; onActivated: controller.setAlignmentPlane(currentText) }
            Label { text: qsTr("TCP Z direction"); color: theme.ink }
            ComboBox { model: plane.currentText === "XY" ? ["+Z", "-Z"] : plane.currentText === "XZ" ? ["+Y", "-Y"] : ["+X", "-X"]; enabled: controller && !controller.busy; onActivated: controller.setAlignmentDirection(currentText) }
            Button { text: qsTr("Align TCP"); enabled: controller && controller.ready && !controller.busy; onClicked: controller.startAlignment() }
        }
        RowLayout {
            Label { text: qsTr("Angular speed %1 rad/s").arg(controller ? controller.angularVelocity.toFixed(2) : "0.10"); color: theme.ink }
            Slider { from: 0.03; to: 0.30; value: controller ? controller.angularVelocity : 0.10; enabled: controller && !controller.busy; onMoved: controller.setAngularVelocity(value) }
        }
        Label { Layout.fillWidth: true; text: controller ? qsTr("Reference %1 · %2").arg(controller.referenceFrame).arg(controller.alignmentPreview) : ""; color: theme.muted; wrapMode: Text.WordWrap }
        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Cancel motion"); enabled: controller && controller.busy; onClicked: controller.cancel() }
            Label { Layout.fillWidth: true; text: controller ? controller.status + " · " + controller.result : ""; color: theme.warning; wrapMode: Text.WordWrap }
        }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: controller ? controller.progress : ""; color: theme.muted; wrapMode: Text.WordWrap; elide: Text.ElideLeft }
    }
}
