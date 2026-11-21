import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    property var controller
    color: "#1b2933"; radius: 12; border.color: "#2b3c47"
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Home and TCP alignment"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            ComboBox {
                model: ["UR10_r", "UR10_l"]
                currentIndex: controller && controller.selectedArm === "UR10_l" ? 1 : 0
                enabled: controller && !controller.busy
                onActivated: controller.selectArm(currentIndex === 1 ? "l" : "r")
                Accessible.name: "Arm for Home and alignment"
            }
        }
        Label { Layout.fillWidth: true; text: controller ? controller.readiness : "Unavailable"; color: controller && controller.ready ? "#45d6a8" : "#f0b44d"; wrapMode: Text.WordWrap }
        RowLayout {
            Label { text: "Home pose"; color: "#dce6eb" }
            TextField { id: home; text: controller ? controller.homePose : ""; enabled: controller && !controller.busy; onEditingFinished: controller.setHomePose(text); Accessible.name: "Named Home pose" }
            Label { text: "Speed " + Math.round(controller ? controller.velocityScaling * 100 : 0) + "%"; color: "#dce6eb" }
            Slider { from: 1; to: 100; value: controller ? controller.velocityScaling * 100 : 20; enabled: controller && !controller.busy; onMoved: controller.setVelocityScaling(value) }
            Button { text: "Move Home"; enabled: controller && controller.ready && !controller.busy; onClicked: { controller.setHomePose(home.text); controller.startHome() } }
        }
        RowLayout {
            Label { text: "TCP plane"; color: "#dce6eb" }
            ComboBox { id: plane; model: ["XY", "XZ", "YZ"]; enabled: controller && !controller.busy; onActivated: controller.setAlignmentPlane(currentText) }
            Label { text: "TCP Z direction"; color: "#dce6eb" }
            ComboBox { model: plane.currentText === "XY" ? ["+Z", "-Z"] : plane.currentText === "XZ" ? ["+Y", "-Y"] : ["+X", "-X"]; enabled: controller && !controller.busy; onActivated: controller.setAlignmentDirection(currentText) }
            Button { text: "Align TCP"; enabled: controller && controller.ready && !controller.busy; onClicked: controller.startAlignment() }
        }
        RowLayout {
            Label { text: "Angular speed " + (controller ? controller.angularVelocity.toFixed(2) : "0.10") + " rad/s"; color: "#dce6eb" }
            Slider { from: 0.03; to: 0.30; value: controller ? controller.angularVelocity : 0.10; enabled: controller && !controller.busy; onMoved: controller.setAngularVelocity(value) }
        }
        Label { Layout.fillWidth: true; text: controller ? "Reference: " + controller.referenceFrame + " · " + controller.alignmentPreview : ""; color: "#a8bac5"; wrapMode: Text.WordWrap }
        RowLayout {
            Layout.fillWidth: true
            Button { text: "Cancel motion"; enabled: controller && controller.busy; onClicked: controller.cancel() }
            Label { Layout.fillWidth: true; text: controller ? controller.status + " · " + controller.result : ""; color: "#f0b44d"; wrapMode: Text.WordWrap }
        }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: controller ? controller.progress : ""; color: "#a8bac5"; wrapMode: Text.WordWrap; elide: Text.ElideLeft }
    }
}
