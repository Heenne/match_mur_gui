import QtQuick 2.12
import QtQuick.Controls 2.5

Button {
    id: control
    property bool inRail: false
    checkable: true
    implicitWidth: Math.max(76, contentItem.implicitWidth + 24)
    implicitHeight: 36
    padding: 10
    font.weight: checked ? Font.DemiBold : Font.Normal

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? theme.accent : theme.muted
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: control.inRail ? Text.AlignLeft : Text.AlignHCenter
    }
    background: Rectangle {
        radius: 6
        color: control.checked ? theme.selected : control.hovered ? theme.hover : "transparent"
        border.color: control.visualFocus ? theme.info : control.checked ? theme.selectedLine : "transparent"
    }
}
