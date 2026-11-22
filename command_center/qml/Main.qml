import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3 as NativeDialogs
import "." as Panels

Control {
    id: window
    width: 1440
    height: 900
    implicitWidth: 1440
    implicitHeight: 900
    palette.window: theme.surface
    palette.windowText: theme.ink
    palette.base: theme.canvas
    palette.alternateBase: theme.surface
    palette.text: theme.ink
    palette.button: theme.raised
    palette.buttonText: theme.ink
    palette.highlight: theme.accent
    palette.highlightedText: theme.accentInk
    palette.light: theme.hover
    palette.midlight: theme.line
    palette.mid: theme.subtle
    palette.dark: theme.canvas
    palette.shadow: theme.canvas
    font.pixelSize: 14
    property var pages: [
        QT_TRANSLATE_NOOP("CommandCenter", "Overview"), QT_TRANSLATE_NOOP("CommandCenter", "Visualization"),
        QT_TRANSLATE_NOOP("CommandCenter", "Robot"), QT_TRANSLATE_NOOP("CommandCenter", "Processes"),
        QT_TRANSLATE_NOOP("CommandCenter", "Diagnosis"), QT_TRANSLATE_NOOP("CommandCenter", "Logs"),
        QT_TRANSLATE_NOOP("CommandCenter", "Settings"), QT_TRANSLATE_NOOP("CommandCenter", "Mocap")
    ]
    readonly property bool wideLayout: width >= 1140
    readonly property bool connectionFailed: session.connectionState.indexOf("Failed") === 0
    readonly property string connectionStatus: connectionFailed ? "Failed" : session.connectionState

    function uiText(value) {
        return qsTranslate("CommandCenter", value)
    }

    function selectPage(page) {
        session.selectView(page)
        workspaceViewport.contentX = 0
        workspaceViewport.contentY = 0
    }

    Keys.onPressed: {
        if (event.key >= Qt.Key_1 && event.key <= Qt.Key_8) {
            selectPage(pages[event.key - Qt.Key_1]); event.accepted = true
        }
    }
    Component.onCompleted: forceActiveFocus()

    Rectangle { anchors.fill: parent; color: theme.canvas }

    component FieldLabel: Label {
        color: theme.muted
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    component HeaderChoice: Button {
        id: choice
        checkable: true
        implicitWidth: 40
        implicitHeight: 40
        font.pixelSize: 14
        font.weight: checked ? Font.DemiBold : Font.Normal
        contentItem: Text {
            text: choice.text
            font: choice.font
            color: choice.checked ? theme.ink : theme.muted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 6
            color: choice.checked ? theme.selected : choice.hovered || choice.down ? theme.hover : "transparent"
            border.color: choice.visualFocus ? theme.focus : "transparent"
            Rectangle {
                visible: choice.checked
                width: 16
                height: 2
                radius: 1
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                anchors.horizontalCenter: parent.horizontalCenter
                color: theme.accent
            }
        }
    }

    Rectangle {
        id: titleBar
        objectName: "titleBar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 54
        color: theme.surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 8

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: "MuR Command Center"
                color: theme.ink
                font.pixelSize: 24
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Row {
                spacing: 4
                HeaderChoice {
                    objectName: "languageDE"
                    text: "DE"
                    checked: appearance.language === "de"
                    Accessible.name: qsTr("German")
                    onClicked: appearance.language = "de"
                }
                HeaderChoice {
                    objectName: "languageEN"
                    text: "EN"
                    checked: appearance.language === "en"
                    Accessible.name: qsTr("English")
                    onClicked: appearance.language = "en"
                }
            }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 24; color: theme.line }
            Row {
                spacing: 4
                HeaderChoice {
                    objectName: "themeLight"
                    text: "☀"
                    font.pixelSize: 19
                    checked: appearance.lightTheme
                    Accessible.name: qsTr("Light")
                    onClicked: appearance.lightTheme = true
                }
                HeaderChoice {
                    objectName: "themeDark"
                    text: "☾"
                    font.pixelSize: 19
                    checked: !appearance.lightTheme
                    Accessible.name: qsTr("Dark")
                    onClicked: appearance.lightTheme = false
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.line }
    }

    Panels.AdaptivePage {
        id: workspaceViewport
        objectName: "workspaceViewport"
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        minimumPageWidth: 480
        minimumPageHeight: Math.max(300, workspaceLayout.Layout.minimumHeight + 36)
        ColumnLayout {
            id: workspaceLayout
            anchors.fill: parent; anchors.margins: 16; spacing: 16
            Rectangle {
                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                implicitHeight: sessionFields.implicitHeight + 20
                color: theme.surface
                border.color: theme.line
                radius: 8
                Flow {
                    id: sessionFields
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 10
                    spacing: 8
                    Column {
                        spacing: 3
                        FieldLabel { text: qsTr("Mode") }
                        Panels.SessionComboBox { objectName: "modeSelector"; width: 122; model: ["Simulation", "Hardware"]; currentIndex: model.indexOf(session.mode); onActivated: session.selectMode(currentText); displayText: window.uiText(currentText) }
                    }
                    Column {
                        spacing: 3
                        FieldLabel { text: qsTr("Preset") }
                        Row {
                            spacing: 6
                            Panels.SessionComboBox { objectName: "presetSelector"; width: 162; model: session.presets; currentIndex: model.indexOf(session.preset); onActivated: session.selectPreset(currentText) }
                            Panels.ActionButton { text: qsTr("New preset"); height: 34; onClicked: presetDialog.open() }
                        }
                    }
                    Column {
                        spacing: 3
                        FieldLabel { text: qsTr("Target host") }
                        Panels.SessionComboBox {
                            id: targetCombo
                            objectName: "targetSelector"
                            width: 151
                            editable: true
                            model: ["localhost", "lab-host"]
                            currentIndex: model.indexOf(session.target)
                            Component.onCompleted: editText = session.target
                            onActivated: session.selectTarget(currentText)
                            onAccepted: session.selectTarget(editText)
                        }
                        Connections {
                            target: session
                            function onChanged() { targetCombo.editText = session.target }
                        }
                    }
                    Column {
                        spacing: 3
                        FieldLabel { text: qsTr("Robot") }
                        Panels.SessionComboBox { objectName: "robotSelector"; width: 112; model: ["mur620a", "mur620b", "mur620c", "mur620d"]; currentIndex: model.indexOf(session.robot); onActivated: session.selectRobot(currentText) }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("ROS domain %1").arg(session.rosDomain)
                    color: theme.muted
                    elide: Text.ElideRight
                }
                Panels.StatusBadge {
                    label: window.uiText(session.mode)
                    foreground: session.mode === "Simulation" ? theme.info : theme.warning
                    color: session.mode === "Simulation" ? theme.infoSurface : theme.warningSurface
                }
                Panels.StatusBadge {
                    label: window.uiText(window.connectionStatus)
                    foreground: window.connectionFailed ? theme.danger : session.connectionState === "Connected" ? theme.focus : theme.warning
                    color: window.connectionFailed ? theme.dangerSurface : session.connectionState === "Connected" ? theme.selected : theme.warningSurface
                }
                Panels.ActionButton {
                    primary: session.connectionState !== "Connected"
                    text: session.connectionState === "Connected" ? qsTr("Disconnect") : qsTr("Connect")
                    enabled: session.connectionState !== "Connecting"
                    onClicked: session.connectionState === "Connected" ? session.disconnectFromTarget() : session.connectToTarget()
                }
            }
            Flow {
                visible: !window.wideLayout
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: implicitHeight
                spacing: 4
                Repeater {
                    model: window.pages
                    Panels.NavigationButton {
                        required property string modelData
                        width: implicitWidth
                        text: window.uiText(modelData)
                        checked: session.view === modelData
                        onClicked: selectPage(modelData)
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: implicitHeight
                visible: window.connectionFailed
            text: qsTr("Connection failed · %1").arg(session.connectionState.slice("Failed: ".length))
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 3
                elide: Text.ElideRight
                color: theme.danger
                padding: 10
                Accessible.name: text
                background: Rectangle { color: theme.dangerSurface; radius: 6 }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: window.wideLayout ? navigation.implicitHeight + 20 : 120
                spacing: 12
                Rectangle {
                    visible: window.wideLayout
                    Layout.preferredWidth: 162
                    Layout.fillHeight: true
                    radius: 8
                    color: theme.sidebar
                    border.color: theme.line
                    ColumnLayout {
                        id: navigation
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 9
                        spacing: 4
                        Label { text: qsTr("Workspace"); color: theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.leftMargin: 8; Layout.bottomMargin: 4 }
                        Repeater {
                            model: window.pages
                            Panels.NavigationButton {
                                required property string modelData
                                Layout.fillWidth: true
                                inRail: true
                                text: window.uiText(modelData)
                                checked: session.view === modelData
                                onClicked: selectPage(modelData)
                            }
                        }
                    }
                }
                StackLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 120
                    clip: true
                    currentIndex: Math.max(0, window.pages.indexOf(session.view))
                    Panels.AdaptivePage {
                        minimumPageWidth: 320
                        Panels.OverviewPage {
                            anchors.fill: parent
                            sessionController: session
                            processManager: processes
                            onPageRequested: window.selectPage(name)
                        }
                    }
                    Panels.AdaptivePage {
                        minimumPageWidth: 300
                        Rectangle {
                            anchors.fill: parent
                            implicitHeight: rvizSummary.implicitHeight + 36
                            color: theme.surface; radius: 10; border.color: theme.line
                            Column {
                                id: rvizSummary
                                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                                anchors.margins: 18; spacing: 10
                                Label { text: qsTr("Visualization"); color: theme.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                                Label {
                                    width: parent.width
                                    text: session.connectionState === "Connected"
                                          ? qsTr("RViz displays the current session in its own dock. You can float or redock it from the title bar.")
                                          : qsTr("Connect to the selected profile to open RViz. The profile needs a readable RViz configuration.")
                                    color: theme.muted
                                    wrapMode: Text.WordWrap
                                }
                                Panels.ActionButton { text: qsTr("Profile settings"); onClicked: window.selectPage("Settings") }
                            }
                        }
                    }
                    Panels.AdaptivePage {
                        minimumPageWidth: session.mode === "Hardware" ? 730 : 660
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12
                            Panels.SimBaseJog { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 260); visible: session.mode === "Simulation" }
                            Panels.UrShutdown { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 300); controller: urShutdown; visible: session.mode === "Hardware" }
                            Panels.UrFreedrive { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 150); controller: urFreedrive; visible: session.mode === "Hardware" }
                            Panels.UrMotionActions { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 480); controller: urMotionActions; visible: session.mode === "Hardware" }
                            Panels.UrArmJog { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 610); controller: urArmJog; visible: session.mode === "Hardware" }
                            Panels.HardwareStart { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 770); workflow: hardwareStart; sessionController: session; processManager: processes; visible: session.mode === "Hardware" }
                            Panels.MirHardwareControl {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(implicitHeight, 470)
                                visible: session.mode === "Hardware"
                            }
                            Panels.LiftPanel {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(implicitHeight, 300)
                                controller: liftController
                                sessionController: session
                            }
                            Panels.GoalsPoses { Layout.fillWidth: true; Layout.preferredHeight: Math.max(implicitHeight, 440); goals: goalsPoses; sessionController: session }
                            Item { Layout.fillHeight: true }
                        }
                    }
                    Panels.AdaptivePage {
                        minimumPageWidth: 460
                        Panels.ProcessesPanel { anchors.fill: parent; processManager: processes }
                    }
                    Panels.AdaptivePage {
                        minimumPageWidth: 360
                        Rectangle {
                            anchors.fill: parent
                            implicitHeight: diagnosisLayout.implicitHeight + 36
                            color: theme.surface; radius: 12; border.color: theme.line
                            Column {
                                id: diagnosisLayout
                                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                                anchors.margins: 18; spacing: 12
                                Label { text: qsTr("ROS graph diagnosis"); color: theme.ink; font.pixelSize: 20; font.bold: true }
                                Label { width: parent.width; text: qsTr("%1 · ROS domain %2 · %3 · %4").arg(session.target).arg(session.rosDomain).arg(session.robot).arg(window.uiText(window.connectionStatus)); color: theme.muted; wrapMode: Text.WrapAnywhere }
                                Repeater {
                                    model: session.diagnostics
                                    delegate: Label {
                                        required property string modelData
                                        width: parent.width
                                        text: window.uiText(modelData); color: theme.ink
                                        wrapMode: Text.WrapAnywhere; font.pixelSize: 14
                                        Accessible.name: text
                                    }
                                }
                            }
                        }
                    }
                    Panels.AdaptivePage {
                        minimumPageWidth: 260
                        Rectangle {
                            anchors.fill: parent
                            implicitHeight: logPageLayout.implicitHeight + 36
                            color: theme.surface; radius: 12; border.color: theme.line
                            ColumnLayout {
                                id: logPageLayout
                                anchors.fill: parent; anchors.margins: 18
                                Label { text: qsTr("Logs"); color: theme.ink; font.pixelSize: 20; font.bold: true }
                                Button { text: qsTr("Open log dock"); onClicked: logs.raiseDock() }
                                Item { Layout.fillHeight: true }
                            }
                        }
                    }
                    Panels.AdaptivePage {
                        minimumPageWidth: 420
                        Rectangle {
                            anchors.fill: parent
                            implicitHeight: settingsLayout.implicitHeight + 36
                            color: theme.surface; radius: 12; border.color: theme.line
                            ColumnLayout {
                                id: settingsLayout
                                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                                anchors.margins: 18; spacing: 12
                                Label { text: qsTr("Profiles"); color: theme.ink; font.pixelSize: 20; font.bold: true }
                                Label { Layout.fillWidth: true; text: qsTr("Edit and save the selected profile. Workspace and RViz paths are machine-local references."); color: theme.muted; wrapMode: Text.WordWrap }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Button { text: qsTr("Edit profile"); onClicked: { profileName.text = session.preset; profileMode.currentIndex = profileMode.model.indexOf(session.mode); profileTarget.text = session.target; profileDomain.text = String(session.rosDomain); profileRobot.text = session.robot; profileVisualization.currentIndex = profileVisualization.model.indexOf(session.visualization); profileWorkspace.text = session.workspacePath; profileConfig.text = session.configPath; profileGazeboOnStart.checked = session.gazeboViewOnSimulationStart; navGoal.text = session.navigationGoal; navCancel.text = session.navigationCancel; navPose.text = session.navigationPose; simTopic.text = session.simCommandTopic; mirTopic.text = session.mirCommandTopic; mirType.currentIndex = mirType.model.indexOf(session.mirCommandType); profileDialog.open() } }
                                    Button { text: qsTr("Export preset…"); onClicked: exportDialog.open() }
                                    Button { text: qsTr("Import preset…"); onClicked: importFileDialog.open() }
                                }
                                CheckBox {
                                    text: qsTr("Gazebo world view on simulation start")
                                    checked: session.gazeboViewOnSimulationStart
                                    onToggled: session.setGazeboViewOnSimulationStart(checked)
                                    Accessible.name: text
                                }
                                Label { Layout.fillWidth: true; text: qsTr("Opens the separately managed client when a local Gazebo server is discovered."); color: theme.muted; wrapMode: Text.WordWrap }
                                Label { Layout.fillWidth: true; text: session.lastError; visible: session.lastError.length > 0; color: theme.danger; wrapMode: Text.WordWrap; Accessible.name: session.lastError }
                            }
                        }
                    }
                    Panels.AdaptivePage {
                        objectName: "mocapPage"
                        minimumPageWidth: 440
                        Panels.MocapPanel { anchors.fill: parent; controller: mocap; sessionController: session }
                    }
                }
            }
        }
    }
    Dialog {
        id: presetDialog; title: qsTr("Create preset"); modal: true
        anchors.centerIn: parent
        contentItem: TextField { id: presetName; placeholderText: qsTr("Preset name") }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: qsTr("Create"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: { if (presetName.text.trim().length) session.selectPreset(presetName.text.trim()); presetName.clear() }
    }
    Dialog {
        id: profileDialog; title: qsTr("Edit profile"); modal: true
        anchors.centerIn: parent; width: Math.min(window.width - 24, 520)
        height: Math.min(window.height - 24, 680)
        contentItem: ScrollView {
            id: profileScroll
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: profileScroll.availableWidth
                TextField { id: profileName; Layout.fillWidth: true; placeholderText: qsTr("Profile name") }
                ComboBox { id: profileMode; Layout.fillWidth: true; model: ["Simulation", "Hardware"]; displayText: window.uiText(currentText) }
                TextField { id: profileTarget; Layout.fillWidth: true; placeholderText: qsTr("Target host") }
                TextField { id: profileDomain; Layout.fillWidth: true; placeholderText: qsTr("ROS domain (0–232)"); validator: IntValidator { bottom: 0; top: 232 } }
                TextField { id: profileRobot; Layout.fillWidth: true; placeholderText: qsTr("Robot") }
                ComboBox {
                    id: profileVisualization
                    Layout.fillWidth: true
                    model: ["RViz", QT_TRANSLATE_NOOP("CommandCenter", "MuR data view")]
                    displayText: window.uiText(currentText)
                    delegate: ItemDelegate {
                        width: profileVisualization.width
                        text: window.uiText(modelData)
                        highlighted: profileVisualization.highlightedIndex === index
                    }
                }
                TextField { id: profileWorkspace; Layout.fillWidth: true; placeholderText: qsTr("Workspace path (machine-local)") }
                TextField { id: profileConfig; Layout.fillWidth: true; placeholderText: qsTr("RViz config path (machine-local)") }
                CheckBox { id: profileGazeboOnStart; text: qsTr("Gazebo world view on simulation start") }
                Label { Layout.fillWidth: true; text: qsTr("Navigation: Nav2 action / PoseStamped source in Simulation; MiR goal / cancel / pose topics in Hardware."); color: theme.muted; wrapMode: Text.WordWrap }
                TextField { id: navGoal; Layout.fillWidth: true; placeholderText: qsTr("Navigation goal topic or action (optional)") }
                TextField { id: navCancel; Layout.fillWidth: true; placeholderText: qsTr("Hardware navigation cancel topic (optional)") }
                TextField { id: navPose; Layout.fillWidth: true; placeholderText: qsTr("Navigation pose topic (optional)") }
                TextField { id: simTopic; Layout.fillWidth: true; placeholderText: qsTr("Simulation TwistStamped command topic (optional)") }
                TextField { id: mirTopic; Layout.fillWidth: true; placeholderText: qsTr("MiR command topic (optional, absolute)") }
                ComboBox { id: mirType; Layout.fillWidth: true; model: ["", "geometry_msgs/msg/Twist", "geometry_msgs/msg/TwistStamped"] }
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: qsTr("Save profile"); DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: if (session.validateEndpoints(profileRobot.text, mirTopic.text, mirType.currentText, simTopic.text, navGoal.text, navCancel.text, navPose.text) && session.saveProfile(profileName.text, profileMode.currentText, profileTarget.text, profileRobot.text, profileVisualization.currentText, profileWorkspace.text, profileConfig.text, Number(profileDomain.text), profileGazeboOnStart.checked) && session.setMirEndpoint(mirTopic.text, mirType.currentText) && session.setSimEndpoint(simTopic.text) && session.setNavigationEndpoints(navGoal.text, navCancel.text, navPose.text)) profileDialog.close() }
        }
    }
    NativeDialogs.FileDialog {
        id: exportDialog; title: qsTr("Export portable preset"); selectExisting: false
        nameFilters: [qsTr("MuR preset bundle (*.murpreset.json)"), qsTr("JSON files (*.json)")]
        onAccepted: session.exportPreset(fileUrl)
    }
    NativeDialogs.FileDialog {
        id: importFileDialog; title: qsTr("Choose portable preset"); selectExisting: true
        nameFilters: [qsTr("MuR preset bundle (*.murpreset.json *.json)"), qsTr("JSON files (*.json)")]
        onAccepted: { importSource.text = fileUrl.toString(); importDialog.open() }
    }
    Dialog {
        id: importDialog; title: qsTr("Import preset and map local paths"); modal: true
        anchors.centerIn: parent; width: Math.min(window.width - 24, 520)
        height: Math.min(window.height - 24, 260)
        contentItem: ScrollView {
            id: importScroll
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: importScroll.availableWidth
                Label { Layout.fillWidth: true; text: qsTr("Map paths from the exporting machine to local files. Leave a field blank to import that reference as unassigned."); color: theme.muted; wrapMode: Text.WordWrap }
                TextField { id: importSource; Layout.fillWidth: true; visible: false }
                TextField { id: workspaceMapping; Layout.fillWidth: true; placeholderText: qsTr("Local workspace path (optional)") }
                TextField { id: configMapping; Layout.fillWidth: true; placeholderText: qsTr("Local RViz config path (optional)") }
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: qsTr("Import"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: session.importPreset(importSource.text, workspaceMapping.text, configMapping.text)
    }
}
