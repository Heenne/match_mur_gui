import QtQuick 2.12
import QtQuick.Controls 2.5

ComboBox {
    id: control
    palette.window: theme.surface
    palette.windowText: theme.ink
    palette.base: theme.raised
    palette.text: theme.ink
    palette.button: theme.raised
    palette.buttonText: theme.ink
    palette.highlight: theme.selected
    palette.highlightedText: theme.ink
    MouseArea {
        anchors.fill: parent
        enabled: control.enabled
        preventStealing: true
        z: 1
        onClicked: {
            control.forceActiveFocus(Qt.MouseFocusReason)
            if (control.popup.visible) control.popup.close()
            else control.popup.open()
        }
    }
    contentItem: Text {
        objectName: "flowComboText"
        leftPadding: 10
        rightPadding: control.indicator ? control.indicator.width + 6 : 10
        text: control.displayText
        font: control.font
        color: control.enabled ? theme.ink : theme.subtle
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    delegate: ItemDelegate {
        width: control.width
        text: control.textRole ? modelData[control.textRole] : modelData
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: parent.text
            font: control.font
            color: theme.ink
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: parent.highlighted || parent.hovered ? theme.selected : theme.surface
        }
    }
}
