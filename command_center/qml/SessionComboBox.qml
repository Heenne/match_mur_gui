import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Window 2.12

ComboBox {
    id: control
    implicitHeight: 34

    background: Rectangle {
        radius: 6
        color: control.hovered ? theme.hover : theme.raised
        border.color: control.activeFocus ? theme.focus : theme.line
    }

    delegate: ItemDelegate {
        width: control.width
        height: 40
        text: control.textRole ? model[control.textRole] : modelData
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: parent.text
            font: control.font
            color: theme.ink
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: parent.highlighted || parent.hovered ? theme.selected : theme.surface
        }
    }

    popup: Popup {
        id: choices
        y: control.height
        width: control.width
        topMargin: 6
        bottomMargin: 6
        padding: 1
        height: Math.min(control.count * 40 + 2, 240,
                         control.Window.height - topMargin - bottomMargin)
        contentItem: ListView {
            clip: true
            model: choices.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar {}
        }
        background: Rectangle {
            color: theme.surface
            border.color: theme.line
            radius: 6
        }
    }

    MouseArea {
        z: 2
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: control.editable ? parent.width - width : 0
        width: control.editable ? 30 : parent.width
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.forceActiveFocus()
            if (choices.visible)
                choices.close()
            else
                choices.open()
        }
    }
}
