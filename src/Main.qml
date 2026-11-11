import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1120
    height: 720
    minimumWidth: 820
    minimumHeight: 560
    visible: true
    title: qsTr("Omonitor")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property int selectedIndex: backend.monitors.length > 0 ? Math.min(layout.selectedIndex, backend.monitors.length - 1) : -1
    property bool helpVisible: false

    function identifyDisplays() {
        for (var i = 0; i < Qt.application.screens.length; ++i)
            identifier.createObject(win, { targetScreen: Qt.application.screens[i] })
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: backend.refresh() }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; onActivated: backend.compactLayout() }
    Shortcut { sequence: "I"; context: Qt.ApplicationShortcut; onActivated: win.identifyDisplays() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.apply() }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            id: layout
            anchors.fill: parent
            anchors.margins: 24
            spacing: 18
            property int selectedIndex: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    spacing: 2
                    Label {
                        text: qsTr("Displays")
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: qsTr("Displays are detected automatically · dragged displays snap together")
                        color: "#9b9ba3"
                        font.pixelSize: 14
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Remove gaps")
                    flat: true
                    enabled: backend.monitors.length > 1
                    onClicked: backend.compactLayout()
                }
                Button {
                    text: qsTr("Identify")
                    flat: true
                    onClicked: win.identifyDisplays()
                }
                Button {
                    text: qsTr("Detect displays")
                    flat: true
                    onClicked: backend.refresh()
                }
                Button {
                    text: qsTr("Apply")
                    highlighted: true
                    enabled: backend.monitors.length > 0 && !backend.pendingConfirmation
                    onClicked: backend.apply()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 18

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 480
                    color: "#151518"
                    radius: theme.radius
                    border.color: "#29292e"
                    clip: true

                    Item {
                        id: canvas
                        anchors.fill: parent
                        anchors.margins: 28

                        function logicalWidth(m) {
                            return ((m.transform === 1 || m.transform === 3) ? m.height : m.width) / m.scale
                        }
                        function logicalHeight(m) {
                            return ((m.transform === 1 || m.transform === 3) ? m.width : m.height) / m.scale
                        }
                        function minimumX() {
                            if (backend.monitors.length === 0)
                                return 0
                            var value = backend.monitors[0].x
                            for (var i = 1; i < backend.monitors.length; ++i)
                                value = Math.min(value, backend.monitors[i].x)
                            return value
                        }
                        function minimumY() {
                            if (backend.monitors.length === 0)
                                return 0
                            var value = backend.monitors[0].y
                            for (var i = 1; i < backend.monitors.length; ++i)
                                value = Math.min(value, backend.monitors[i].y)
                            return value
                        }
                        function maximumX() {
                            if (backend.monitors.length === 0)
                                return 1
                            var value = backend.monitors[0].x + logicalWidth(backend.monitors[0])
                            for (var i = 1; i < backend.monitors.length; ++i) {
                                var m = backend.monitors[i]
                                value = Math.max(value, m.x + logicalWidth(m))
                            }
                            return value
                        }
                        function maximumY() {
                            if (backend.monitors.length === 0)
                                return 1
                            var value = backend.monitors[0].y + logicalHeight(backend.monitors[0])
                            for (var i = 1; i < backend.monitors.length; ++i) {
                                var m = backend.monitors[i]
                                value = Math.max(value, m.y + logicalHeight(m))
                            }
                            return value
                        }

                        property real minX: minimumX()
                        property real minY: minimumY()
                        property real contentWidth: maximumX() - minX
                        property real contentHeight: maximumY() - minY
                        property real zoom: Math.max(0.04, Math.min(0.24,
                            Math.min((width - 30) / Math.max(1, contentWidth),
                                     (height - 30) / Math.max(1, contentHeight))))
                        property real originX: (width - contentWidth * zoom) / 2 - minX * zoom
                        property real originY: (height - contentHeight * zoom) / 2 - minY * zoom

                        Label {
                            anchors.centerIn: parent
                            visible: backend.monitors.length === 0
                            text: qsTr("No active displays found")
                            color: "#77777f"
                        }

                        Repeater {
                            model: backend.monitors

                            Rectangle {
                                id: screen
                                required property int index
                                required property var modelData
                                property real logicalW: canvas.logicalWidth(modelData)
                                property real logicalH: canvas.logicalHeight(modelData)
                                property real dragOffsetX: 0
                                property real dragOffsetY: 0
                                property point dragStart: Qt.point(0, 0)

                                x: canvas.originX + modelData.x * canvas.zoom
                                y: canvas.originY + modelData.y * canvas.zoom
                                width: logicalW * canvas.zoom
                                height: logicalH * canvas.zoom
                                transform: Translate { x: screen.dragOffsetX; y: screen.dragOffsetY }
                                color: index === win.selectedIndex ? Qt.lighter(theme.accent, 1.05) : "#29292f"
                                radius: Math.min(4, theme.radius)
                                border.width: index === win.selectedIndex ? 2 : 1
                                border.color: index === win.selectedIndex ? theme.accent : "#4a4a52"

                                Column {
                                    anchors.centerIn: parent
                                    width: parent.width - 20
                                    spacing: 3
                                    Label {
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                        text: modelData.name
                                        color: index === win.selectedIndex ? theme.accentForeground : "white"
                                        font.weight: Font.DemiBold
                                    }
                                    Label {
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                        text: modelData.mode.split("@")[0] + "  ·  " + Number(modelData.scale).toLocaleString(Qt.locale(), 'f', 2) + "×"
                                        color: index === win.selectedIndex ? theme.accentForeground : "#b9b9c0"
                                        font.pixelSize: 12
                                    }
                                }

                                TapHandler {
                                    onTapped: layout.selectedIndex = index
                                }
                                DragHandler {
                                    id: dragger
                                    target: null
                                    cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                    onTranslationChanged: {
                                        if (active) {
                                            screen.dragOffsetX = translation.x - screen.dragStart.x
                                            screen.dragOffsetY = translation.y - screen.dragStart.y
                                        }
                                    }
                                    onActiveChanged: {
                                        if (active) {
                                            screen.dragStart = translation
                                            layout.selectedIndex = index
                                        } else {
                                            var nextX = Math.round((screen.x + screen.dragOffsetX - canvas.originX) / canvas.zoom)
                                            var nextY = Math.round((screen.y + screen.dragOffsetY - canvas.originY) / canvas.zoom)
                                            screen.dragOffsetX = 0
                                            screen.dragOffsetY = 0
                                            backend.snapPosition(index, nextX, nextY)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    color: "#151518"
                    radius: theme.radius
                    border.color: "#29292e"

                    property var selectedMonitor: win.selectedIndex >= 0 ? backend.monitors[win.selectedIndex] : ({})

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 14

                        Label {
                            Layout.fillWidth: true
                            text: parent.parent.selectedMonitor.description || parent.parent.selectedMonitor.name || qsTr("Display")
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: parent.parent.selectedMonitor.name || ""
                            color: "#8f8f98"
                            visible: text.length > 0
                        }

                        Label { text: qsTr("Resolution and refresh rate"); color: "#b9b9c0" }
                        ComboBox {
                            id: modeBox
                            Layout.fillWidth: true
                            model: parent.parent.selectedMonitor.modes || []
                            currentIndex: indexOfValue(parent.parent.selectedMonitor.mode)
                            enabled: win.selectedIndex >= 0
                            onActivated: {
                                backend.setMode(win.selectedIndex, currentValue)
                                backend.compactLayout()
                            }
                        }

                        Label { text: qsTr("Scale"); color: "#b9b9c0" }
                        ComboBox {
                            id: scaleBox
                            Layout.fillWidth: true
                            model: [0.75, 1, 1.25, 1.5, 1.75, 2]
                            currentIndex: indexOfValue(Number(parent.parent.selectedMonitor.scale))
                            textRole: ""
                            delegate: ItemDelegate {
                                required property var modelData
                                width: scaleBox.width
                                text: Number(modelData).toLocaleString(Qt.locale(), 'f', 2) + "×"
                            }
                            contentItem: Label {
                                leftPadding: 12
                                verticalAlignment: Text.AlignVCenter
                                text: Number(scaleBox.currentValue || 1).toLocaleString(Qt.locale(), 'f', 2) + "×"
                            }
                            enabled: win.selectedIndex >= 0
                            onActivated: {
                                backend.setScale(win.selectedIndex, Number(currentValue))
                                backend.compactLayout()
                            }
                        }

                        Label { text: qsTr("Orientation"); color: "#b9b9c0" }
                        ComboBox {
                            id: rotationBox
                            Layout.fillWidth: true
                            model: [
                                { label: qsTr("Landscape"), value: 0 },
                                { label: qsTr("Portrait right"), value: 1 },
                                { label: qsTr("Landscape flipped"), value: 2 },
                                { label: qsTr("Portrait left"), value: 3 }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: indexOfValue(parent.parent.selectedMonitor.transform)
                            enabled: win.selectedIndex >= 0
                            onActivated: {
                                backend.setTransform(win.selectedIndex, Number(currentValue))
                                backend.compactLayout()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Position")
                                color: "#b9b9c0"
                            }
                            Label {
                                text: (parent.parent.parent.selectedMonitor.x || 0) + " × " +
                                      (parent.parent.parent.selectedMonitor.y || 0)
                                font.family: "monospace"
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Align top edges")
                            visible: backend.monitors.length > 1
                            onClicked: backend.alignTop(win.selectedIndex, win.selectedIndex === 0 ? 1 : 0)
                        }

                        Item { Layout.fillHeight: true }

                        Label {
                            Layout.fillWidth: true
                            visible: backend.error.length > 0
                            text: backend.error
                            color: "#ff6b6b"
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: backend.error.length === 0 && backend.status.length > 0
                            text: backend.status
                            color: "#8f8f98"
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: backend.pendingConfirmation
            color: "#b8000000"
            z: 20

            Rectangle {
                anchors.centerIn: parent
                width: 460
                height: 210
                color: "#1b1b1f"
                border.color: theme.accent
                radius: theme.radius

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Keep this display configuration?")
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The previous configuration will return in %1 seconds.").arg(backend.secondsRemaining)
                        color: "#b9b9c0"
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Button { text: qsTr("Revert"); onClicked: backend.revert() }
                        Button { text: qsTr("Keep"); highlighted: true; onClicked: backend.confirm() }
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#d9000000"
            z: 30
            TapHandler { onTapped: win.helpVisible = false }

            Rectangle {
                anchors.centerIn: parent
                width: 420
                height: 310
                color: "#1b1b1f"
                border.color: "#38383f"
                radius: theme.radius

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 12
                    Label { text: qsTr("Keyboard shortcuts"); font.pixelSize: 21; font.weight: Font.DemiBold }
                    Label { text: qsTr("Space    Apply configuration") }
                    Label { text: qsTr("A        Remove all gaps") }
                    Label { text: qsTr("I        Identify physical displays") }
                    Label { text: qsTr("R        Detect displays") }
                    Label { text: qsTr("?        Show or hide shortcuts") }
                    Label { text: qsTr("Q        Quit") }
                    Item { Layout.fillHeight: true }
                    Label { text: qsTr("Click anywhere to close"); color: "#8f8f98" }
                }
            }
        }
    }

    Component {
        id: identifier

        Window {
            id: identification
            required property var targetScreen
            screen: targetScreen
            color: "#d9000000"
            flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
            title: qsTr("Identify display")

            Rectangle {
                anchors.centerIn: parent
                width: 300
                height: 180
                color: theme.accent
                radius: theme.radius

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: identification.targetScreen.name
                        color: theme.accentForeground
                        font.pixelSize: 42
                        font.weight: Font.Bold
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: identification.targetScreen.width + " × " + identification.targetScreen.height
                        color: theme.accentForeground
                        font.pixelSize: 16
                    }
                }
            }

            Timer {
                interval: 3500
                running: true
                onTriggered: identification.destroy()
            }
            Component.onCompleted: showFullScreen()
        }
    }
}
