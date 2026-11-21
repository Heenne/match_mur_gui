import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Rectangle {
    id: panel
    required property var controller
    required property var sessionController
    implicitWidth: 660
    implicitHeight: 300
    color: "#1b2933"
    radius: 12
    border.color: "#2b3c47"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: "Lift"; color: "#f2f6f8"; font.pixelSize: 20; font.bold: true }
            ComboBox {
                id: sideSelector
                model: ["left", "right"]
                currentIndex: model.indexOf(panel.controller.side)
                onActivated: panel.controller.selectSide(currentText)
                Accessible.name: "Lift side"
            }
            Item { Layout.fillWidth: true }
            Label {
                text: panel.sessionController.mode + " · " + panel.sessionController.robot
                color: "#a8bac5"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: 10
                color: panel.controller.heightMeters > 0 ? "#243640" : "#3a3130"
                ColumnLayout {
                    anchors.centerIn: parent
                    Label { text: "CURRENT HEIGHT"; color: "#a8bac5"; font.pixelSize: 11; font.bold: true }
                    Label {
                        text: panel.controller.ageSeconds < 0 ? "— m" : panel.controller.heightMeters.toFixed(3) + " m"
                        color: "#f2f6f8"; font.pixelSize: 22; font.bold: true
                        Accessible.name: "Lift height " + text
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: 10
                color: "#243640"
                ColumnLayout {
                    anchors.centerIn: parent
                    Label { text: "DATA AGE"; color: "#a8bac5"; font.pixelSize: 11; font.bold: true }
                    Label {
                        text: panel.controller.ageSeconds < 0 ? "Not received" : panel.controller.ageSeconds.toFixed(1) + " s"
                        color: panel.controller.ageSeconds > 2.0 ? "#ffb15b" : "#f2f6f8"
                        font.pixelSize: 18; font.bold: true
                        Accessible.name: "Lift data age " + text
                    }
                }
            }
            ColumnLayout {
                Label { text: "STEP (m)"; color: "#a8bac5"; font.pixelSize: 11; font.bold: true }
                SpinBox {
                    id: stepSpin
                    from: 1; to: 50; value: Math.round(panel.controller.stepMeters * 1000)
                    stepSize: 1
                    editable: true
                    textFromValue: function(value, locale) { return (value / 1000).toFixed(3) }
                    valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 1000) }
                    onValueModified: panel.controller.setStepMeters(value / 1000)
                    Accessible.name: "Lift step in meters"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label { text: "Allowed range"; color: "#a8bac5" }
            Label { text: "Min"; color: "#a8bac5" }
            SpinBox {
                id: minSpin
                from: 5; to: 259; value: Math.round(panel.controller.minimumMeters * 1000)
                editable: true
                textFromValue: function(value, locale) { return (value / 1000).toFixed(3) }
                valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 1000) }
                onValueModified: panel.controller.setLimits(value / 1000, maxSpin.value / 1000)
                Accessible.name: "Lift minimum height in meters"
            }
            Label { text: "Max"; color: "#a8bac5" }
            SpinBox {
                id: maxSpin
                from: 6; to: 260; value: Math.round(panel.controller.maximumMeters * 1000)
                editable: true
                textFromValue: function(value, locale) { return (value / 1000).toFixed(3) }
                valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 1000) }
                onValueModified: panel.controller.setLimits(minSpin.value / 1000, value / 1000)
                Accessible.name: "Lift maximum height in meters"
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "STOP"
                enabled: panel.controller.holding
                onClicked: panel.controller.stop("Stop button")
                Accessible.name: "Stop lift movement"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Lower"
                enabled: panel.controller.canLiftDown && panel.sessionController.mode === "Hardware"
                onPressed: panel.controller.press("down")
                onReleased: panel.controller.release()
                onCanceled: panel.controller.stop("lower button canceled")
                Accessible.name: "Lower selected lift"
            }
            Button {
                text: "Raise"
                enabled: panel.controller.canLiftUp && panel.sessionController.mode === "Hardware"
                onPressed: panel.controller.press("up")
                onReleased: panel.controller.release()
                onCanceled: panel.controller.stop("raise button canceled")
                Accessible.name: "Raise selected lift"
            }
            Label {
                Layout.fillWidth: true
                text: panel.controller.status
                color: panel.controller.status.indexOf("Ready") === 0 || panel.controller.status.indexOf("Target sent") === 0
                    ? "#45d6a8" : (panel.controller.status.indexOf("Stale") === 0 || panel.controller.status.indexOf("limit") >= 0 ? "#ffb15b" : "#a8bac5")
                wrapMode: Text.WordWrap
                Accessible.name: panel.controller.status
            }
        }
    }
}
