import QtQuick 2.12
import QtQuick.Controls 2.5

Button {
    id: control
    property bool primary: false
    implicitWidth: Math.max(88, contentItem.implicitWidth + 24)
    implicitHeight: 38
    padding: 10
    font.weight: Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? theme.subtle : control.primary ? theme.accentInk : theme.ink
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 6
        color: !control.enabled ? theme.surface : control.primary
               ? (control.down ? Qt.darker(theme.accent, 1.2) : theme.accent)
               : (control.hovered || control.down ? theme.hover : theme.raised)
        border.color: control.visualFocus ? theme.info : control.primary ? color : theme.line
    }
}
