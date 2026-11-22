import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: panel
    required property var controller
    required property var sessionController
    implicitWidth: 660
    implicitHeight: Math.max(300, controls.implicitHeight + 32)
    color: theme.surface
    radius: 12
    border.color: theme.line

    ColumnLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Lift"); color: theme.ink; font.pixelSize: 20; font.bold: true }
            ComboBox {
                id: sideSelector
                model: ["left", "right"]
                currentIndex: model.indexOf(panel.controller.side)
                onActivated: panel.controller.selectSide(currentText)
                Accessible.name: qsTr("Lift side")
            }
            Item { Layout.fillWidth: true }
            Label {
                text: panel.sessionController.mode + " · " + panel.sessionController.robot
                color: theme.muted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: 10
                color: panel.controller.heightMeters > 0 ? theme.raised : theme.warningSurface
                ColumnLayout {
                    anchors.centerIn: parent
                    Label { text: qsTr("CURRENT HEIGHT"); color: theme.muted; font.pixelSize: 11; font.bold: true }
                    Label {
                        text: panel.controller.ageSeconds < 0 ? "— m" : panel.controller.heightMeters.toFixed(3) + " m"
                        color: theme.ink; font.pixelSize: 22; font.bold: true
                        Accessible.name: qsTr("Lift height %1").arg(text)
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: 10
                color: theme.raised
                ColumnLayout {
                    anchors.centerIn: parent
                    Label { text: qsTr("DATA AGE"); color: theme.muted; font.pixelSize: 11; font.bold: true }
                    Label {
                        text: panel.controller.ageSeconds < 0 ? qsTr("Not received") : panel.controller.ageSeconds.toFixed(1) + " s"
                        color: panel.controller.ageSeconds > 2.0 ? theme.warning : theme.ink
                        font.pixelSize: 18; font.bold: true
                        Accessible.name: qsTr("Lift data age %1").arg(text)
                    }
                }
            }
            ColumnLayout {
                Label { text: qsTr("STEP (m)"); color: theme.muted; font.pixelSize: 11; font.bold: true }
                SpinBox {
                    id: stepSpin
                    from: 1; to: 50; value: Math.round(panel.controller.stepMeters * 1000)
                    stepSize: 1
                    editable: true
                    textFromValue: function(value, locale) { return (value / 1000).toFixed(3) }
                    valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 1000) }
                    onValueModified: panel.controller.setStepMeters(value / 1000)
                    Accessible.name: qsTr("Lift step in meters")
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Allowed range"); color: theme.muted }
            Label { text: qsTr("Min"); color: theme.muted }
            SpinBox {
                id: minSpin
                from: 5; to: 259; value: Math.round(panel.controller.minimumMeters * 1000)
                editable: true
                textFromValue: function(value, locale) { return (value / 1000).toFixed(3) }
                valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 1000) }
                onValueModified: panel.controller.setLimits(value / 1000, maxSpin.value / 1000)
                Accessible.name: qsTr("Lift minimum height in meters")
            }
            Label { text: qsTr("Max"); color: theme.muted }
            SpinBox {
                id: maxSpin
                from: 6; to: 260; value: Math.round(panel.controller.maximumMeters * 1000)
                editable: true
                textFromValue: function(value, locale) { return (value / 1000).toFixed(3) }
                valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 1000) }
                onValueModified: panel.controller.setLimits(minSpin.value / 1000, value / 1000)
                Accessible.name: qsTr("Lift maximum height in meters")
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "STOP"
                enabled: panel.controller.holding
                onClicked: panel.controller.stop("Stop button")
                Accessible.name: qsTr("Stop lift movement")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Lower")
                enabled: panel.controller.canLiftDown && panel.sessionController.mode === "Hardware"
                onPressed: panel.controller.press("down")
                onReleased: panel.controller.release()
                onCanceled: panel.controller.stop("lower button canceled")
                Accessible.name: qsTr("Lower selected lift")
            }
            Button {
                text: qsTr("Raise")
                enabled: panel.controller.canLiftUp && panel.sessionController.mode === "Hardware"
                onPressed: panel.controller.press("up")
                onReleased: panel.controller.release()
                onCanceled: panel.controller.stop("raise button canceled")
                Accessible.name: qsTr("Raise selected lift")
            }
            Label {
                Layout.fillWidth: true
                text: panel.controller.status
                color: panel.controller.status.indexOf("Ready") === 0 || panel.controller.status.indexOf("Target sent") === 0
                    ? theme.accent : (panel.controller.status.indexOf("Stale") === 0 || panel.controller.status.indexOf("limit") >= 0 ? theme.warning : theme.muted)
                wrapMode: Text.WordWrap
                Accessible.name: panel.controller.status
            }
        }
    }
}
