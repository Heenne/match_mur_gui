import QtQuick 2.12
import QtQuick.Controls 2.5

Dialog {
    id: dialog

    required property string message
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: Math.min(520, Math.max(0, parent.width - 24))
    height: Math.min(340, Math.max(0, parent.height - 24))

    contentItem: ScrollView {
        id: messageView
        clip: true
        contentWidth: availableWidth
        contentHeight: body.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        Label {
            id: body
            width: messageView.availableWidth
            padding: 12
            text: dialog.message
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
    }
}
