import QtQuick 2.12
import QtQuick.Controls 2.5

Rectangle {
    id: badge
    required property string label
    property color foreground: theme.focus
    color: theme.selected
    radius: 12
    implicitWidth: statusText.implicitWidth + 20
    implicitHeight: 25

    Label {
        id: statusText
        anchors.centerIn: parent
        text: badge.label
        color: badge.foreground
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
}
