import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
Rectangle {
 property var controller
 color: "#1b2933"; radius: 12; border.color: "#2b3c47"
 ColumnLayout {
  anchors.fill: parent; anchors.margins: 18
  RowLayout {
   Label { text: "Freedrive"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
   ComboBox { model: ["UR10_r", "UR10_l"]; enabled: controller && !controller.busy; onActivated: controller.selectArm(currentIndex === 1 ? "l" : "r"); Accessible.name: "Arm for Freedrive" }
   Button { text: "Activate"; enabled: controller && !controller.busy; onClicked: controller.enter() }
   Button { text: "Leave / cancel"; enabled: controller && controller.busy; onClicked: controller.leave() }
  }
  Label { Layout.fillWidth: true; text: controller ? controller.selectedArm + " · " + controller.status : "Unavailable"; color: "#f0b44d"; wrapMode: Text.WordWrap }
 }
}
