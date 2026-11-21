import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
Rectangle {
    implicitHeight: controls.implicitHeight + 36
 property var controller
 color: theme.surface; radius: 12; border.color: theme.line
 ColumnLayout {
     id: controls
  anchors.fill: parent; anchors.margins: 18
  RowLayout {
   Label { text: "Freedrive"; color: theme.ink; font.pixelSize: 20; font.bold: true }
   ComboBox { model: ["UR10_r", "UR10_l"]; enabled: controller && !controller.busy; onActivated: controller.selectArm(currentIndex === 1 ? "l" : "r"); Accessible.name: "Arm for Freedrive" }
   Button { text: "Activate"; enabled: controller && !controller.busy; onClicked: controller.enter() }
   Button { text: "Leave / cancel"; enabled: controller && controller.busy; onClicked: controller.leave() }
  }
  Label { Layout.fillWidth: true; text: controller ? controller.selectedArm + " · " + controller.status : "Unavailable"; color: theme.warning; wrapMode: Text.WordWrap }
 }
}
