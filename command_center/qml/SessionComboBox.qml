import QtQuick 2.12
import QtQuick.Controls 2.5

ComboBox {
    id: control
    implicitHeight: 34

    background: Rectangle {
        radius: 6
        color: control.hovered ? theme.hover : theme.raised
        border.color: control.activeFocus ? theme.info : theme.line
    }
}
