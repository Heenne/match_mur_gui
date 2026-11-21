import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Window 2.12

Flickable {
    id: view

    default property alias page: pageSurface.data
    property real minimumPageWidth: pageSurface.panel ? pageSurface.panel.implicitWidth : 0
    property real minimumPageHeight: pageSurface.panel ? pageSurface.panel.implicitHeight : 0
    readonly property int scrollbarGutter: 12

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.AutoFlickIfNeeded
    contentWidth: Math.max(width, minimumPageWidth + scrollbarGutter)
    contentHeight: Math.max(height, minimumPageHeight + scrollbarGutter)

    ScrollBar.horizontal: ScrollBar { policy: size < 1 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
    ScrollBar.vertical: ScrollBar { policy: size < 1 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }

    Connections {
        target: view.Window.window
        function onActiveFocusItemChanged() {
            if (!view.visible) return
            const focused = target.activeFocusItem
            let ancestor = focused
            while (ancestor && ancestor !== pageSurface) ancestor = ancestor.parent
            if (!ancestor) return
            const rect = focused.mapToItem(pageSurface, 0, 0, focused.width, focused.height)
            if (rect.x < view.contentX) view.contentX = rect.x
            else if (rect.x + rect.width > view.contentX + view.width - view.scrollbarGutter)
                view.contentX = Math.min(view.contentWidth - view.width, rect.x + rect.width - view.width + view.scrollbarGutter)
            if (rect.y < view.contentY) view.contentY = rect.y
            else if (rect.y + rect.height > view.contentY + view.height - view.scrollbarGutter)
                view.contentY = Math.min(view.contentHeight - view.height, rect.y + rect.height - view.height + view.scrollbarGutter)
        }
    }

    readonly property Item surface: Item {
        id: pageSurface
        parent: view.contentItem
        width: Math.max(0, view.contentWidth - view.scrollbarGutter)
        height: Math.max(0, view.contentHeight - view.scrollbarGutter)
        readonly property Item panel: children.length ? children[0] : null
    }
}
