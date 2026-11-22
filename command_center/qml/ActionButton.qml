import QtQuick 2.12
import QtQuick.Controls 2.5

Button {
    id: control
    property bool primary: false
    implicitWidth: Math.max(88, contentItem.implicitWidth + 32)
    implicitHeight: 40
    topPadding: 8
    bottomPadding: 8
    leftPadding: 16
    rightPadding: 16
    font.weight: Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? theme.subtle : control.primary && !control.hovered && !control.down ? theme.accentInk : theme.ink
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 6
        color: !control.enabled ? theme.surface : control.hovered || control.down ? theme.hover
               : control.primary ? theme.accent : theme.raised
        border.color: control.visualFocus ? theme.focus : control.primary && !control.hovered ? color : theme.line
    }
}
