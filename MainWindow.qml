import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import parameters 1.0

import OpenGLUnderQML 1.0

Window {
    id: mainWindow
    width: 1280
    height: 640
    color: "#363636"
    minimumHeight: 768
    minimumWidth: 1024
    title: qsTr("MatViz3D")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    FontLoader {
        id: montserrat
        source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf"
    }

    Loader {
        id: aboutLoader
        source: "AboutView.qml"
        active: false
    }

    Loader {
        id: statisticsLoader
        source: "StatisticsView.qml"
        active: false
    }

    Loader {
        id: materialdataLoader
        source: "MaterialDatabaseView.qml"
        active: false
    }

    Loader {
        id: stressAnalysisLoader
        source: "StressAnalysisView.qml"
        active: false
    }

    Loader
    {
        id: textureLoader
        source: "TextureView.qml"
        active: false
        onLoaded: item.visible = true
    }

    function openTextureEditor() {
        textureLoader.active = true
        if (textureLoader.item) {
            textureLoader.item.visible = true
            textureLoader.item.raise()
        }
    }

    // Action fields in the algorithm parameter panel do not know about windows;
    // they raise an id and this is where it becomes a window.
    Connections {
        target: schemaController
        function onActionTriggered(action) {
            if (action === "open_texture_editor")
                mainWindow.openTextureEditor()
        }
    }

    ColumnLayout {
            id: mainLayout
            anchors.fill: parent
            spacing: 0

        Rectangle {
            id: menuBar_rec
            Layout.fillWidth: true
            Layout.preferredHeight: 67
            color: "#282828"

            RowLayout {
                id: mb_row
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                spacing: 15

                Item {
                    id: _item1_menuBar
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 49
                    Layout.alignment: Qt.AlignVCenter

                    Image {
                        id: iconMenu_img
                        width: 48
                        height: 48
                        source: "qrc:/img/iconMenu.png"
                        fillMode: Image.PreserveAspectFit
                        anchors.centerIn: parent
                    }
                }

                Item {
                    id: _item2_menuBar
                    Layout.preferredWidth: 83
                    Layout.preferredHeight: 49
                    Layout.alignment: Qt.AlignVCenter

                    MenuBar {
                        id: fileMenuBar
                        width: 43
                        height: 49
                        background: Rectangle {
                            color: "#00000000"
                            border.color: "#00000000"
                            border.width: 0
                            width: 43
                            height: 49
                        }

                        contentItem: Text {
                            id: menuFileText
                            text: qsTr("File")
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        Menu {
                            id: fileMenu
                            title: qsTr("   ")
                            font.pixelSize: 14
                            font.family: inter.name

                            Action
                            {
                                text: qsTr("Save as PNG");
                                icon.source: "qrc:/img/fileMenu/save_png.svg"
                                onTriggered: exportController.saveAsImage(glWidget, _itemFieldView.legendVertical, _itemFieldView.visible)
                            }
                            Action
                            {
                                text: qsTr("Save as SVG");
                                icon.source: "qrc:/img/fileMenu/save_svg.svg"
                                //onTriggered: exportController.saveAsSVG(glWidget)
                                onTriggered: exportController.saveAsVectorSVG(glWidget)
                            }
                            Action
                            {
                                text: qsTr("Make screenshot");
                                icon.source: "qrc:/img/fileMenu/make_screenshot.svg"
                                onTriggered: exportController.copyToClipboard(glWidget, _itemFieldView.legendVertical, _itemFieldView.visible)
                            }
                            MenuSeparator { }
                            Action
                            {
                                text: qsTr("Export to wrl");
                                icon.source: "qrc:/img/fileMenu/export_wrl.svg"
                                onTriggered: exportController.exportToVRML()
                            }
                            Action
                            {
                                text: qsTr("Export to csv");
                                icon.source: "qrc:/img/fileMenu/export_csv.svg"
                                onTriggered: exportController.exportToCSV()
                            }
                            Action
                            {
                                text: qsTr("Save as HDF5");
                                icon.source: "qrc:/img/fileMenu/save_hdf5.svg"
                                onTriggered: exportController.exportToHDF5()
                            }
                            Action
                            {
                                text: qsTr("Open project");
                                icon.source: "qrc:/img/fileMenu/open_project.svg"
                                onTriggered: hdf5ProjectController.openFileDialog()
                            }
                            MenuSeparator { }
                            Action {
                                text: qsTr("Estimate stresses")
                                icon.source: "qrc:/img/fileMenu/estimate_stresses.svg"
                                onTriggered: {
                                    stressAnalysisLoader.active = true;
                                    stressAnalysisLoader.item.visible = true;
                                }
                            }

                            Action {
                                text: qsTr("Texture Editor")
                                icon.source: "qrc:/img/fileMenu/grain_generator.svg"
                                onTriggered: {
                                    textureLoader.active = true
                                    if (textureLoader.item)
                                        textureLoader.item.visible = true
                                }
                            }

                            Action {
                                text: qsTr("Edit material data")
                                icon.source: "qrc:/img/fileMenu/edit_material_data.svg"
                                onTriggered: {
                                    materialdataLoader.active = true;
                                    materialdataLoader.item.visible = true;
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: hoverMenuFileText
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: menuFileText.font.weight = Font.Bold;
                        onExited: menuFileText.font.weight = Font.Normal;

                        onClicked: {
                            fileMenu.open()
                        }
                    }
                }

                Item {
                    id: _item3_menuBar
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 49
                    Layout.alignment: Qt.AlignVCenter

                    MenuBar {
                        id: windowMenuBar
                        width: 88
                        height: 49
                        background: Rectangle {
                            color: "#00000000"
                            border.color: "#00000000"
                            border.width: 0
                            width: 88
                            height: 49
                        }

                        contentItem: Text {
                            id: menuWindowText
                            text: qsTr("Window")
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        Menu {
                            id: windowMenu
                            title: qsTr("   ")
                            font.pixelSize: 16
                            font.family: montserrat.name

                            MenuItem {
                                id: checkAll
                                text: qsTr("All")
                                checkable: true
                                checked: true
                                onTriggered: {
                                    checkConsole.checked = checked;
                                    checkData.checked = checked;
                                    checkToolBar.checked = checked;
                                    checkLoadStep.checked = checked;
                                    checkConsole.triggered();
                                    checkData.triggered();
                                    checkToolBar.triggered();
                                    checkLoadStep.triggered();
                                }
                            }
                            MenuItem { id: checkConsole; text: qsTr("Console"); checkable: true; checked: true; onTriggered: { _itemConsole.visible = checked; if (!checked) checkAll.checked = false; } }
                            MenuItem { id: checkData; text: qsTr("Data"); checkable: true; checked: true; onTriggered: { _itemData.visible = checked; if (!checked) checkAll.checked = false; } }
                            MenuItem { id: checkToolBar; text: qsTr("Tool Bar"); checkable: true; checked: true; onTriggered: { _itemToolBar.visible = checked; if (!checked) checkAll.checked = false; } }
                            MenuItem {
                                id: checkLoadStep
                                text: qsTr("Load Step Control")
                                checkable: true
                                checked: true
                                onTriggered: {
                                    if (!checked) checkAll.checked = false;
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: hoverMenuWindowText
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: menuWindowText.font.weight = Font.Bold;
                        onExited: menuWindowText.font.weight = Font.Normal;

                        onClicked: {
                            windowMenu.open()
                        }
                    }
                }

                Item {
                    id: _item4_menuBar
                    Layout.preferredWidth: 146
                    Layout.preferredHeight: 49
                    Layout.alignment: Qt.AlignVCenter

                    Button {
                        id: buttonStatistics
                        x: 0
                        y: 0
                        width: 95
                        height: parent.height
                        text: qsTr("Statistics")

                        background: Rectangle {
                            id: buttonStatisticsBackground
                            width: 109
                            height: 49
                            color: "transparent"
                            border.color: "transparent"
                            border.width: 1

                            MouseArea {
                                id: hoverButtonStatistics
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onEntered: buttonStatisticsText.font.weight = Font.Bold;
                                onExited: buttonStatisticsText.font.weight = Font.Normal;
                            }
                        }

                        contentItem: Text {
                            id: buttonStatisticsText
                            text: buttonStatistics.text
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors {
                                left: parent.left
                                leftMargin: 1
                                verticalCenter: parent.verticalCenter
                            }
                        }

                        onClicked: {
                            statisticsController.analyze()
                            statisticsLoader.active = true;
                            statisticsLoader.item.visible = true;
                        }
                    }
                }

                Item {
                    id: _item5_menuBar
                    Layout.preferredWidth: 115
                    Layout.preferredHeight: 49
                    Layout.alignment: Qt.AlignVCenter

                    Button {
                        id: buttonAbout
                        x: 0
                        y: 0
                        width: 75
                        height: parent.height
                        text: qsTr("About")

                        background: Rectangle {
                            id: buttonAboutBackground
                            width: 75
                            height: 49
                            color: "transparent"
                            border.color: "transparent"
                            border.width: 1

                            MouseArea {
                                id: hoverButtonAbout
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onEntered: buttonAboutText.font.weight = Font.Bold;
                                onExited: buttonAboutText.font.weight = Font.Normal;
                            }
                        }

                        contentItem: Text {
                            id: buttonAboutText
                            text: buttonAbout.text
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors {
                                left: parent.left
                                leftMargin: 1
                                verticalCenter: parent.verticalCenter
                            }
                        }

                        onClicked: {
                            aboutLoader.active = true;
                            aboutLoader.item.visible = true;
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    id: _text_menuBar
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    color: "#00897b"
                    textFormat: Text.RichText
                    text: qsTr("MatViz<span style='color: #00564D;'>3D</span>")
                    font.pixelSize: 36
                    font.styleName: "Bold"
                    font.family: inter.name
                }
            }
        }

        Item {
            id: workspaceArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            OpenGLWidgetQML {
                id: glWidget
                anchors.fill: parent

                // Axis labels for the corner triad. Positions come from
                // glWidget, which projects the triad's own MVP -- anchors
                // would only pin them to a fixed corner, not to the axes.
                Item {
                    id: axisLabelOverlay
                    objectName: "axisLabelOverlay"
                    anchors.fill: parent

                    Text {
                        text: "X"
                        color: "#FF4444"
                        font.pixelSize: 14
                        font.bold: true
                        x: glWidget.axisLabelX.x - width / 2
                        y: glWidget.axisLabelX.y - height / 2
                    }
                    Text {
                        text: "Y"
                        color: "#44FF44"
                        font.pixelSize: 14
                        font.bold: true
                        x: glWidget.axisLabelY.x - width / 2
                        y: glWidget.axisLabelY.y - height / 2
                    }
                    Text {
                        text: "Z"
                        color: "#4488FF"
                        font.pixelSize: 14
                        font.bold: true
                        x: glWidget.axisLabelZ.x - width / 2
                        y: glWidget.axisLabelZ.y - height / 2
                    }
                }
            }

            MouseArea {
                preventStealing: false
                propagateComposedEvents: true
                anchors.fill: parent
                hoverEnabled: true
                onPressed: (mouse) => {
                    glWidget.forceActiveFocus()
                    console.log("Force Active focus")
                    mouse.accepted = false
                }
            }

            Keys.onPressed: (event) => {
                console.log("Key pressed:", event.key, "Text:", event.text)
                if (event.key === Qt.Key_D && (event.modifiers & Qt.AltModifier)) {
                    glWidget.toggleDebugMode()
                }
                if (event.key === Qt.Key_D && (event.modifiers & Qt.ControlModifier)) {
                    glWidget.toggleFaceCulling()
                }
            }

            Item {
                id: _itemData
                // Anchored top-to-bottom rather than given a fixed height: the
                // parameter ScrollView below is the fillHeight item, so every
                // extra pixel of window height goes to the parameter list.
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: 20
                width: mainWindow.width < 1250 ? 310 : 350
                visible: true

                Rectangle {
                    color: "#80282828"
                    radius: 13
                    anchors.fill: parent

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            Layout.leftMargin: 29
                            Layout.rightMargin: 29
                            Layout.topMargin: 10

                            Text {
                                Layout.fillWidth: true
                                color: "#d9d9d9"
                                text: qsTr("Data")
                                font.pixelSize: 14
                                font.family: montserrat.name
                            }

                            Image {
                                source: "qrc:/img/closeData.png"
                                Layout.preferredWidth: 10
                                Layout.preferredHeight: 10
                                fillMode: Image.PreserveAspectFit

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        _itemData.visible = false
                                        checkData.checked = false
                                        checkAll.checked = false
                                    }
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Text {
                                    color: "#c6c6c6"
                                    text: qsTr("Algorithm:")
                                    font.pixelSize: 20
                                    font.styleName: "Bold"
                                    font.family: inter.name
                                }

                                ComboBox {
                                    id: comboBox
                                    width: 224
                                    height: 28
                                    leftPadding: 10
                                    font.pointSize: 10
                                    font.family: montserrat.name
                                    editable: true
                                    currentIndex: -1
                                    displayText: currentIndex === -1 ? "---" : currentText
                                    background: Rectangle {
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                    }
                                    // The registry is the list: an algorithm
                                    // that self-registers shows up here with
                                    // no edit to this file.
                                    model: schemaController.algorithmNames
                                    Connections {
                                        target: Parameters
                                        function onAlgorithmChanged() {
                                            var idx = comboBox.find(Parameters.algorithm)
                                            if (idx !== -1) comboBox.currentIndex = idx
                                        }
                                    }
                                    function applySelection(index) {
                                        if (index !== -1) {
                                            Qt.callLater(() => {
                                                Parameters.setAlgorithm(currentText)
                                                schemaController.onAlgorithmSelected(currentText)
                                            })
                                        }
                                    }
                                    onActivated: applySelection(currentIndex)
                                    onAccepted: {
                                        // Typing is a filter over the registry,
                                        // not a way to invent an algorithm the
                                        // factory cannot build.
                                        var idx = find(editText)
                                        if (idx === -1) {
                                            editText = currentIndex === -1 ? "" : currentText
                                        } else {
                                            currentIndex = idx
                                            applySelection(idx)
                                        }
                                    }
                                }
                            }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: 20
                            Layout.rightMargin: 20
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            DynamicParamBlock {
                                id: dataParamsBlock
                                width: parent.width
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80

                            Button {
                                id: start_button
                                width: 105
                                height: 50
                                anchors.centerIn: parent
                                text: qsTr("START")

                                background: Rectangle {
                                    id: buttonBackground2
                                    radius: 12
                                    color: "#282828"
                                    border.color: "#969696"
                                    border.width: 1

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onEntered: buttonBackground2.color = "#3a3a3a"
                                        onExited: buttonBackground2.color = "#282828"

                                    }
                                }
                                contentItem: Text {
                                    text: start_button.text
                                    font.pixelSize: 20
                                    font.family: inter.name
                                    font.bold: true
                                    color: "#CFCECE"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    if (comboBox.currentIndex === -1) return
                                    mainWindowWrapper.onStartButton()
                                }
                            }
                        }
                    }
                }
            }

            Item {
                id: _itemToolBar
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.margins: 20
                width: 450
                height: 26
                visible: true

                RowLayout {
                    anchors.fill: parent
                    spacing: 15

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        property bool widgetsHidden: false
                        source: widgetsHidden ? "qrc:/img/toolBar/viewIconHidden.svg" : "qrc:/img/toolBar/viewIcon.svg"
                        fillMode: Image.PreserveAspectFit

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Toggle sidebar panels")
                            onClicked: {
                                parent.widgetsHidden = !parent.widgetsHidden
                                _itemData.visible = !parent.widgetsHidden
                                _itemConsole.visible = !parent.widgetsHidden
                                checkData.checked = !parent.widgetsHidden
                                checkConsole.checked = !parent.widgetsHidden
                                checkAll.checked = !parent.widgetsHidden
                            }
                        }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/saveIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        MouseArea { 
                            anchors.fill: parent; 
                            cursorShape: Qt.PointingHandCursor; 
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Export structure to HDF5")
                            onClicked: exportController.exportToHDF5(); 
                        }
                    }

                    Image {
                        id: gifToggle
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26

                        property bool isActive: false

                        source: isActive ? "qrc:/img/toolBar/gifIcon_on.svg" : "qrc:/img/toolBar/gifIcon_off.svg"
                        fillMode: Image.PreserveAspectFit
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Toggle GIF recording")
                            onClicked: {
                                gifToggle.isActive = !gifToggle.isActive;
                                Parameters.setIsGifRecording(gifToggle.isActive);
                            }
                        }
                    }

                    Item {
                        id: screenshotGroup
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 26

                        Row {
                            anchors.fill: parent
                            spacing: 1

                            Item {
                                width: 26
                                height: 26

                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/img/toolBar/screenIcon.svg"
                                    fillMode: Image.PreserveAspectFit
                                    scale: 1.2
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: qsTr("Copy screenshot to clipboard")
                                    onClicked: exportController.copyToClipboard(glWidget, _itemFieldView.legendVertical, _itemFieldView.visible)
                                }
                            }

                            Item {
                                id: screenshotDropdownBtn
                                width: 15
                                height: 26

                                Canvas {
                                    id: dropdownChevron
                                    anchors.centerIn: parent
                                    width: 8
                                    height: 5
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.beginPath();
                                        ctx.moveTo(1, 1);
                                        ctx.lineTo(4, 4);
                                        ctx.lineTo(7, 1);
                                        ctx.strokeStyle = (screenshotDropdownArea.containsMouse || screenshotPopup.visible) ? "#00897b" : "#CFCECE";
                                        ctx.lineWidth = 1.5;
                                        ctx.lineCap = "round";
                                        ctx.lineJoin = "round";
                                        ctx.stroke();
                                    }
                                }

                                MouseArea {
                                    id: screenshotDropdownArea
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: qsTr("Screenshot settings")
                                    onEntered: dropdownChevron.requestPaint()
                                    onExited: dropdownChevron.requestPaint()
                                    onClicked: screenshotPopup.visible ? screenshotPopup.close() : screenshotPopup.open()
                                }
                            }
                        }

                        Popup {
                            id: screenshotPopup
                            x: (screenshotGroup.width - width) / 2
                            y: screenshotGroup.height + 8
                            width: 210
                            padding: 10
                            modal: false
                            focus: false
                            closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
                            onVisibleChanged: dropdownChevron.requestPaint()

                            background: Rectangle {
                                color: "#E6282828"
                                radius: 8
                                border.color: "#5A5A5A"
                                border.width: 1
                            }

                            contentItem: Column {
                                spacing: 4

                                CheckBox {
                                    id: autoCropCheck
                                    text: qsTr("Auto-crop empty borders")
                                    checked: exportController.autoCrop
                                    font.family: montserrat.name
                                    font.pixelSize: 11
                                    spacing: 8
                                    padding: 4
                                    hoverEnabled: true

                                    indicator: Rectangle {
                                        implicitWidth: 15
                                        implicitHeight: 15
                                        x: autoCropCheck.leftPadding
                                        anchors.verticalCenter: parent.verticalCenter
                                        radius: 3
                                        color: autoCropCheck.checked ? "#00897b" : (autoCropCheck.hovered ? "#383838" : "#2A2A2A")
                                        border.color: autoCropCheck.checked ? "#00897b" : (autoCropCheck.hovered ? "#808080" : "#555555")
                                        border.width: 1.5

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: "#ffffff"
                                            visible: autoCropCheck.checked
                                        }
                                    }

                                    contentItem: Text {
                                        text: autoCropCheck.text
                                        font: autoCropCheck.font
                                        color: autoCropCheck.hovered ? "#ffffff" : "#d0d0d0"
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: autoCropCheck.indicator.width + autoCropCheck.spacing
                                    }

                                    onToggled: exportController.autoCrop = checked
                                }

                                CheckBox {
                                    id: highDpiCheck
                                    text: qsTr("High DPI (300 DPI)")
                                    checked: exportController.highDpi
                                    font.family: montserrat.name
                                    font.pixelSize: 11
                                    spacing: 8
                                    padding: 4
                                    hoverEnabled: true

                                    indicator: Rectangle {
                                        implicitWidth: 15
                                        implicitHeight: 15
                                        x: highDpiCheck.leftPadding
                                        anchors.verticalCenter: parent.verticalCenter
                                        radius: 3
                                        color: highDpiCheck.checked ? "#00897b" : (highDpiCheck.hovered ? "#383838" : "#2A2A2A")
                                        border.color: highDpiCheck.checked ? "#00897b" : (highDpiCheck.hovered ? "#808080" : "#555555")
                                        border.width: 1.5

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: "#ffffff"
                                            visible: highDpiCheck.checked
                                        }
                                    }

                                    contentItem: Text {
                                        text: highDpiCheck.text
                                        font: highDpiCheck.font
                                        color: highDpiCheck.hovered ? "#ffffff" : "#d0d0d0"
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: highDpiCheck.indicator.width + highDpiCheck.spacing
                                    }

                                    onToggled: exportController.highDpi = checked
                                }
                            }
                        }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/zoom-inIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        scale: 1.2
                        MouseArea { 
                            anchors.fill: parent; 
                            cursorShape: Qt.PointingHandCursor; 
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Zoom in")
                            onClicked: glWidget.zoomIn(); 
                        }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/zoom-outIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        scale: 1.2
                        MouseArea { 
                            anchors.fill: parent; 
                            cursorShape: Qt.PointingHandCursor; 
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Zoom out")
                            onClicked: glWidget.zoomOut(); 
                        }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/zoom-fitIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        scale: 1.2
                        MouseArea { 
                            anchors.fill: parent; 
                            cursorShape: Qt.PointingHandCursor; 
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Zoom to fit")
                            onClicked: glWidget.zoomToFit(); 
                        }
                    }

                    Item {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26

                        Image {
                            anchors.fill: parent
                            source: "qrc:/img/toolBar/cubeIcon.svg"
                            fillMode: Image.PreserveAspectFit

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                ToolTip.visible: containsMouse
                                ToolTip.text: qsTr("Standard views")
                                onClicked: cubeMenu.popup()
                            }
                        }

                        Menu {
                            id: cubeMenu
                            width: 70

                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/isometric_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Isometric view")
                                onClicked: mainWindowWrapper.isometricViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/dimetric_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Dimetric view")
                                onClicked: mainWindowWrapper.dimetricViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/isometric_down_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Isometric view (down)")
                                onClicked: mainWindowWrapper.isometricDownViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/dimetric_down_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Dimetric view (down)")
                                onClicked: mainWindowWrapper.dimetricDownViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/front_Cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Front view")
                                onClicked: mainWindowWrapper.frontViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/back_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Back view")
                                onClicked: mainWindowWrapper.backViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/top_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Top view")
                                onClicked: mainWindowWrapper.topViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/bottom_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Bottom view")
                                onClicked: mainWindowWrapper.bottomViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/left_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Left view")
                                onClicked: mainWindowWrapper.leftViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/rigft_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Right view")
                                onClicked: mainWindowWrapper.rightViewButton()
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 26
                        color: "#5A5A5A"
                    }

                    RowLayout {
                        spacing: 15

                        Image {
                            id: animToggle
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26

                            property bool isActive: false

                            source: isActive ? "qrc:/img/toolBar/animation_on.svg" : "qrc:/img/toolBar/animation_off.svg"
                            fillMode: Image.PreserveAspectFit

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                ToolTip.visible: containsMouse
                                ToolTip.text: qsTr("Toggle animation")
                                onClicked: {
                                    animToggle.isActive = !animToggle.isActive;
                                    Parameters.setIsAnimation(animToggle.isActive);

                                    if (!animToggle.isActive) {
                                        speedPopup.close();
                                    }
                                }
                            }
                        }

                        Item {
                            id: speedToggle
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 26
                            visible: animToggle.isActive

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                ToolTip.visible: containsMouse
                                ToolTip.text: qsTr("Animation speed")
                                onClicked: speedPopup.visible ? speedPopup.close() : speedPopup.open()
                            }

                            Text {
                                anchors.centerIn: parent
                                text: (animationSpeedSlider.value / 250).toFixed(1) + "x"
                                font.family: montserrat.name
                                font.pixelSize: 13
                                font.bold: true
                                color: speedPopup.visible ? "#00897b" : "#CFCECE"
                            }

                            Popup {
                                id: speedPopup
                                x: (speedToggle.width - width) / 2
                                y: speedToggle.height + 10
                                width: 200
                                padding: 14
                                modal: false
                                focus: false
                                closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

                                background: Rectangle {
                                    color: "#80282828"
                                    radius: 13
                                    border.color: "#5A5A5A"
                                    border.width: 1
                                }

                                contentItem: Column {
                                    spacing: 8

                                    Text {
                                        text: qsTr("Animation speed")
                                        color: "#d9d9d9"
                                        font.pixelSize: 13
                                        font.family: montserrat.name
                                    }

                                    Slider {
                                        id: animationSpeedSlider
                                        width: 172
                                        height: 18
                                        from: 0
                                        to: 500
                                        value: 250
                                        onValueChanged: {
                                            var delay = Math.round(to - value)
                                            glWidget.setDelayAnimation(delay)
                                        }
                                        Component.onCompleted: glWidget.setDelayAnimation(Math.round(to - value))
                                    }
                                }
                            }
                        }

                        Image {
                            id: wireframeToggle
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26

                            property bool isActive: false

                            source: isActive ? "qrc:/img/toolBar/wireframe_on.svg" : "qrc:/img/toolBar/wireframe_off.svg"
                            fillMode: Image.PreserveAspectFit

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                ToolTip.visible: containsMouse
                                ToolTip.text: qsTr("Toggle wireframe")
                                onClicked: {
                                    wireframeToggle.isActive = !wireframeToggle.isActive;
                                    glWidget.setPlotWireFrame(wireframeToggle.isActive);
                                }
                            }
                        }

                        Image {
                            id: orientationToggle
                            Layout.alignment: Qt.AlignVCenter

                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36

                            property bool isActive: false

                            source: isActive ? "qrc:/img/toolBar/orientation_on.svg" : "qrc:/img/toolBar/orientation_off.svg"
                            fillMode: Image.PreserveAspectFit

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                ToolTip.visible: containsMouse
                                ToolTip.text: qsTr("Toggle grain orientations")
                                onClicked: {
                                    orientationToggle.isActive = !orientationToggle.isActive;
                                    glWidget.setShowOrientations(orientationToggle.isActive);
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 26
                        color: "#5A5A5A"
                    }

                    Item {
                        id: explodedToggle
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26

                        Image {
                            anchors.fill: parent
                            source: "qrc:/img/toolBar/exploded_view.svg"
                            fillMode: Image.PreserveAspectFit
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 6
                            color: "transparent"
                            border.width: 1
                            border.color: explodedPopup.visible ? "#00897b" : "transparent"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Toggle exploded view")
                            onClicked: explodedPopup.visible ? explodedPopup.close() : explodedPopup.open()
                        }

                        Popup {
                            id: explodedPopup
                            x: (explodedToggle.width - width) / 2
                            y: explodedToggle.height + 10
                            width: 200
                            padding: 14
                            modal: false
                            focus: false
                            closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

                            background: Rectangle {
                                color: "#80282828"
                                radius: 13
                                border.color: "#5A5A5A"
                                border.width: 1
                            }

                            contentItem: Column {
                                spacing: 8

                                Text {
                                    text: qsTr("Exploded view")
                                    color: "#d9d9d9"
                                    font.pixelSize: 13
                                    font.family: montserrat.name
                                }

                                Slider {
                                    id: explodedSlider
                                    width: 172
                                    height: 18
                                    from: 0
                                    to: 20
                                    value: 0
                                    onValueChanged: glWidget.explodedValueChanged(value)
                                }
                            }
                        }
                    }
                }
            }

        }

        Item {
            id: _itemConsole
            Layout.fillWidth: true
            Layout.preferredHeight: mainWindow.height < 780 ? 170 : 220
            visible: true

            Rectangle {
                anchors.fill: parent
                color: "#282828"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    Image {
                        Layout.alignment: Qt.AlignRight
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        source: "qrc:/img/closeData.png"
                        fillMode: Image.PreserveAspectFit

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                _itemConsole.visible = false
                                checkConsole.checked = false
                                checkAll.checked = false
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ConsoleOutput {
                            id: consoleOutput_
                            anchors.fill: parent
                        }
                    }
                }
            }
        }
    }

    Item {
        id: _itemFieldView
        x: parent.width - (_itemFieldView.width + 30)
        y: menuBar_rec.height + 20
        width: mainWindow.width < 1250 ? 320 : 360
        height: fieldViewCol.implicitHeight + 30
        visible: stressAnalysisController.hasResult || (hdf5ProjectController.isOpen && hdf5ProjectController.loadSteps.length > 0)

        property bool legendVertical: false
        onLegendVerticalChanged: exportController.legendVertical = legendVertical
        Component.onCompleted: exportController.legendVertical = legendVertical

        function levelVal(lvl) {
            var min = stressAnalysisController.fieldMin;
            var max = stressAnalysisController.fieldMax;
            if (min === undefined || isNaN(min)) min = 0;
            if (max === undefined || isNaN(max)) max = 0;
            return min + (max - min) * (lvl / 8.0);
        }

        function fieldFmt(v) {
            if (v === undefined || v === null || isNaN(v)) return "0";
            if (Math.abs(v) >= 10000 || (Math.abs(v) < 0.001 && v !== 0))
                return v.toExponential(3);
            return parseFloat(v.toPrecision(5)).toString();
        }

        Rectangle {
            id: fieldView_rec
            color: "#80282828"
            radius: 13
            anchors.fill: parent

            Column {
                id: fieldViewCol
                anchors.fill: parent
                anchors.margins: 15
                spacing: 12

                Text {
                    color: "#d9d9d9"
                    text: qsTr("Field view")
                    font.pixelSize: 14
                    font.family: montserrat.name
                }

                Row {
                    width: parent.width
                    spacing: 10
                    Text {
                        text: qsTr("Component:")
                        color: "#c6c6c6"
                        font.pixelSize: 14
                        font.family: inter.name
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    ComboBox {
                        id: fieldComboBox
                        width: 190
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter
                        model: stressAnalysisController.fieldComponents
                        currentIndex: stressAnalysisController.fieldComponentIndex
                        onActivated: (index) => stressAnalysisController.fieldComponentIndex = index
                    }
                }

                // ── Colormap Legend ───────────────────────────────────────────
                Item {
                    id: legendContainer
                    width: parent.width
                    height: legendCol.implicitHeight

                    property var paletteColors: {
                        glWidget.colorMapPalette;
                        return glWidget.getColorMap(9);
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: colorMapMenu.popup()
                    }

                    Column {
                        id: legendCol
                        width: parent.width
                        spacing: 8

                        // Header with title, Palette button, and View Mode toggle
                        Item {
                            width: parent.width
                            height: 22

                            Text {
                                id: legendTitleText
                                text: qsTr("Colormap Legend")
                                color: "#d9d9d9"
                                font.pixelSize: 13
                                font.family: inter.name
                                font.weight: Font.Medium
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Row {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Rectangle {
                                    id: paletteBtn
                                    width: paletteBtnText.implicitWidth + 12
                                    height: 20
                                    radius: 10
                                    color: palMouse.containsMouse ? "#3a3a3a" : "#2a2a2a"
                                    border.color: "#50ffffff"
                                    border.width: 1

                                    Text {
                                        id: paletteBtnText
                                        anchors.centerIn: parent
                                        text: qsTr("Palette...")
                                        color: "#d0d0d0"
                                        font.pixelSize: 10
                                        font.family: inter.name
                                    }

                                    MouseArea {
                                        id: palMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: colorMapMenu.popup()
                                    }
                                }

                                Rectangle {
                                    id: legendToggleBtn
                                    width: legendToggleText.implicitWidth + 12
                                    height: 20
                                    radius: 10
                                    color: toggleMouse.containsMouse ? "#3a3a3a" : "#2a2a2a"
                                    border.color: "#50ffffff"
                                    border.width: 1

                                    Text {
                                        id: legendToggleText
                                        anchors.centerIn: parent
                                        text: _itemFieldView.legendVertical ? qsTr("Vertical") : qsTr("Horizontal")
                                        color: "#d0d0d0"
                                        font.pixelSize: 10
                                        font.family: inter.name
                                    }

                                    MouseArea {
                                        id: toggleMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        ToolTip.visible: containsMouse
                                        ToolTip.text: _itemFieldView.legendVertical ? qsTr("Switch to horizontal legend") : qsTr("Switch to vertical (9-level) legend")
                                        onClicked: _itemFieldView.legendVertical = !_itemFieldView.legendVertical
                                    }
                                }
                            }
                        }

                        // Detailed Vertical FEA View (Levels 8 down to 0)
                        Column {
                            id: verticalLegend
                            width: parent.width
                            visible: _itemFieldView.legendVertical
                            height: visible ? implicitHeight : 0
                            spacing: 3

                            Repeater {
                                model: 9
                                Item {
                                    width: parent.width
                                    height: 15
                                    property int level: 8 - index
                                    property real val: _itemFieldView.levelVal(level)

                                    Row {
                                        anchors.fill: parent
                                        spacing: 8

                                        Rectangle {
                                            id: swatchRect
                                            width: 26
                                            height: 13
                                            radius: 2
                                            color: (legendContainer.paletteColors && legendContainer.paletteColors.length > level)
                                                   ? legendContainer.paletteColors[level]
                                                   : "transparent"
                                            border.color: "#40ffffff"
                                            border.width: 0.5
                                            anchors.verticalCenter: parent.verticalCenter

                                            MouseArea {
                                                id: swatchMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.NoButton
                                                ToolTip.visible: containsMouse
                                                ToolTip.text: (level === 8 ? "Max: " : (level === 0 ? "Min: " : "Level " + (level + 1) + ": ")) + _itemFieldView.fieldFmt(val)
                                            }
                                        }

                                        Text {
                                            width: 80
                                            text: _itemFieldView.fieldFmt(val)
                                            color: (level === 8 || level === 0) ? "#ffffff" : "#c6c6c6"
                                            font.pixelSize: 11
                                            font.family: inter.name
                                            font.weight: (level === 8 || level === 0) ? Font.DemiBold : Font.Normal
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: level === 8 ? qsTr("(Max)") : (level === 0 ? qsTr("(Min)") : "")
                                            color: "#7a7a7a"
                                            font.pixelSize: 10
                                            font.family: inter.name
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: text !== ""
                                        }
                                    }
                                }
                            }
                        }

                        // Compact Horizontal View
                        Column {
                            id: horizontalLegend
                            width: parent.width
                            visible: !_itemFieldView.legendVertical
                            height: visible ? implicitHeight : 0
                            spacing: 5

                            // Color bar on its own row
                            Rectangle {
                                id: hColorBar
                                width: parent.width
                                height: 14
                                radius: 3
                                clip: true
                                border.color: "#40ffffff"
                                border.width: 1

                                Row {
                                    anchors.fill: parent
                                    Repeater {
                                        model: 9
                                        Rectangle {
                                            width: hColorBar.width / 9
                                            height: parent.height
                                            color: (legendContainer.paletteColors && legendContainer.paletteColors.length > index)
                                                   ? legendContainer.paletteColors[index]
                                                   : "transparent"
                                        }
                                    }
                                }
                            }

                            // Values below the color bar
                            Item {
                                width: parent.width
                                height: 16

                                // Min (0%)
                                Text {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: _itemFieldView.fieldFmt(_itemFieldView.levelVal(0))
                                    color: "#ffffff"
                                    font.pixelSize: 10
                                    font.family: inter.name
                                    font.weight: Font.DemiBold
                                }

                                // 25% (Level 2)
                                Text {
                                    x: Math.round((parent.width * 0.25) - (width / 2))
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: _itemFieldView.fieldFmt(_itemFieldView.levelVal(2))
                                    color: "#9e9e9e"
                                    font.pixelSize: 10
                                    font.family: inter.name
                                    visible: parent.width >= 310
                                }

                                // Mid (50% - Level 4)
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: _itemFieldView.fieldFmt(_itemFieldView.levelVal(4))
                                    color: "#c6c6c6"
                                    font.pixelSize: 10
                                    font.family: inter.name
                                }

                                // 75% (Level 6)
                                Text {
                                    x: Math.round((parent.width * 0.75) - (width / 2))
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: _itemFieldView.fieldFmt(_itemFieldView.levelVal(6))
                                    color: "#9e9e9e"
                                    font.pixelSize: 10
                                    font.family: inter.name
                                    visible: parent.width >= 310
                                }

                                // Max (100%)
                                Text {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: _itemFieldView.fieldFmt(_itemFieldView.levelVal(8))
                                    color: "#ffffff"
                                    font.pixelSize: 10
                                    font.family: inter.name
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Text {
                            text: qsTr("Right-click legend to change colormap palette")
                            color: "#7a7a7a"
                            font.pixelSize: 10
                            font.family: inter.name
                        }
                    }

                    Menu {
                        id: colorMapMenu
                        title: qsTr("Colormap")

                        MenuItem {
                            text: qsTr("Rainbow")
                            checkable: true
                            checked: glWidget.colorMapPalette === 0
                            onTriggered: glWidget.colorMapPalette = 0
                        }
                        MenuItem {
                            text: qsTr("Cool-Warm (blue-white-red)")
                            checkable: true
                            checked: glWidget.colorMapPalette === 1
                            onTriggered: glWidget.colorMapPalette = 1
                        }
                        MenuItem {
                            text: qsTr("Red-Blue (red-white-blue)")
                            checkable: true
                            checked: glWidget.colorMapPalette === 2
                            onTriggered: glWidget.colorMapPalette = 2
                        }
                        MenuItem {
                            text: qsTr("Viridis")
                            checkable: true
                            checked: glWidget.colorMapPalette === 3
                            onTriggered: glWidget.colorMapPalette = 3
                        }
                        MenuItem {
                            text: qsTr("Grayscale")
                            checkable: true
                            checked: glWidget.colorMapPalette === 4
                            onTriggered: glWidget.colorMapPalette = 4
                        }
                    }
                }

                Row {
                    spacing: 10
                    Switch {
                        id: deformedSwitch
                        width: 50
                        height: 20
                        scale: 0.7
                        display: AbstractButton.IconOnly
                        checked: stressAnalysisController.showDeformed
                        onCheckedChanged: stressAnalysisController.showDeformed = checked
                    }
                    Text {
                        text: qsTr("Deformed shape")
                        color: "#c6c6c6"
                        font.pixelSize: 14
                        font.family: inter.name
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Row {
                    width: parent.width
                    visible: deformedSwitch.checked
                    spacing: 8
                    Slider {
                        id: deformedScaleSlider
                        width: parent.width - 60
                        from: 0
                        to: Math.max(stressAnalysisController.deformedScale * 4, 10)
                        value: stressAnalysisController.deformedScale
                        onMoved: stressAnalysisController.deformedScale = value
                    }
                    Text {
                        text: _itemFieldView.fieldFmt(stressAnalysisController.deformedScale) + "x"
                        color: "#9e9e9e"
                        font.pixelSize: 12
                        font.family: inter.name
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Row {
                    spacing: 10
                    Switch {
                        id: showFieldSwitch
                        width: 50
                        height: 20
                        scale: 0.7
                        display: AbstractButton.IconOnly
                        checked: stressAnalysisController.showField
                        onCheckedChanged: stressAnalysisController.showField = checked
                    }
                    Text {
                        text: showFieldSwitch.checked ? qsTr("Showing field (click to show grains)") : qsTr("Showing grains (click to show field)")
                        color: "#c6c6c6"
                        font.pixelSize: 12
                        font.family: inter.name
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    // Tensor overlays, stacked directly under the field-view card and shown
    // under the same condition, so the two read as one panel.
    TensorViewPanel {
        id: _itemTensorView
        x: _itemFieldView.x
        y: _itemFieldView.y + _itemFieldView.height + 12
        width: _itemFieldView.width
        visible: stressAnalysisController.hasResult

        target: glWidget
        titleFont: montserrat.name
        bodyFont: inter.name
        gridSize: Parameters.size
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Bottom Load Step Timeline Scrubber (Suggestion C)
    // ═══════════════════════════════════════════════════════════════════
    Rectangle {
        id: _itemLoadStepTimeline
        visible: checkLoadStep.checked && hdf5ProjectController.isOpen && (hdf5ProjectController.loadSteps.length > 1 || hdf5ProjectController.geomSets.length > 1)
        anchors {
            bottom: parent.bottom
            bottomMargin: (_itemConsole.visible ? _itemConsole.height : 0) + 14
            horizontalCenter: parent.horizontalCenter
        }
        height: 46
        width: Math.min(parent.width - 80, timelineRow.implicitWidth + 36)
        color: "#d9202020"
        radius: 12
        border.color: "#444444"
        border.width: 1

        RowLayout {
            id: timelineRow
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            Text {
                text: qsTr("Geom:")
                color: "#c6c6c6"
                font.pixelSize: 12
                font.bold: true
                font.family: montserrat.name
                visible: hdf5ProjectController.geomSets.length > 1
            }

            ComboBox {
                id: timelineGeomCombo
                Layout.preferredWidth: 65
                Layout.preferredHeight: 28
                model: hdf5ProjectController.geomSets
                currentIndex: hdf5ProjectController.currentGeomIndex
                onActivated: (index) => hdf5ProjectController.selectGeomSet(index)
                visible: hdf5ProjectController.geomSets.length > 1
            }

            Rectangle {
                width: 1
                height: 22
                color: "#555555"
                visible: hdf5ProjectController.geomSets.length > 1
            }

            Text {
                text: qsTr("Load Step:")
                color: "#c6c6c6"
                font.pixelSize: 12
                font.bold: true
                font.family: montserrat.name
            }

            // Vector Prev Button (<)
            Rectangle {
                id: timelinePrevBtn
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 6
                color: tlPrevMouse.pressed ? "#161616" : (tlPrevMouse.containsMouse ? "#3a3a3a" : "#262626")
                border.color: tlPrevMouse.containsMouse ? "#666666" : "#404040"
                border.width: 1
                opacity: hdf5ProjectController.currentLoadStepIndex > 0 ? 1.0 : 0.35

                Canvas {
                    anchors.centerIn: parent
                    width: 8; height: 12
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.beginPath();
                        ctx.moveTo(6.5, 1);
                        ctx.lineTo(1.5, 6);
                        ctx.lineTo(6.5, 11);
                        ctx.strokeStyle = "#e0e0e0";
                        ctx.lineWidth = 2.0;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";
                        ctx.stroke();
                    }
                }

                ToolTip.visible: tlPrevMouse.containsMouse
                ToolTip.text: qsTr("Previous load step")

                MouseArea {
                    id: tlPrevMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: parent.opacity > 0.5 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (hdf5ProjectController.currentLoadStepIndex > 0)
                            hdf5ProjectController.prevStep();
                    }
                }
            }

            // Timeline Slider
            Slider {
                id: timelineSlider
                Layout.preferredWidth: Math.min(320, mainWindow.width * 0.28)
                Layout.alignment: Qt.AlignVCenter
                from: 0
                to: Math.max(0, hdf5ProjectController.loadSteps.length - 1)
                stepSize: 1
                value: hdf5ProjectController.currentLoadStepIndex
                onMoved: hdf5ProjectController.selectLoadStep(Math.round(value))
            }

            // Vector Next Button (>)
            Rectangle {
                id: timelineNextBtn
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 6
                color: tlNextMouse.pressed ? "#161616" : (tlNextMouse.containsMouse ? "#3a3a3a" : "#262626")
                border.color: tlNextMouse.containsMouse ? "#666666" : "#404040"
                border.width: 1
                opacity: hdf5ProjectController.currentLoadStepIndex < hdf5ProjectController.loadSteps.length - 1 ? 1.0 : 0.35

                Canvas {
                    anchors.centerIn: parent
                    width: 8; height: 12
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.beginPath();
                        ctx.moveTo(1.5, 1);
                        ctx.lineTo(6.5, 6);
                        ctx.lineTo(1.5, 11);
                        ctx.strokeStyle = "#e0e0e0";
                        ctx.lineWidth = 2.0;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";
                        ctx.stroke();
                    }
                }

                ToolTip.visible: tlNextMouse.containsMouse
                ToolTip.text: qsTr("Next load step")

                MouseArea {
                    id: tlNextMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: parent.opacity > 0.5 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (hdf5ProjectController.currentLoadStepIndex < hdf5ProjectController.loadSteps.length - 1)
                            hdf5ProjectController.nextStep();
                    }
                }
            }

            Text {
                text: hdf5ProjectController.currentLoadStepName + " (" + (hdf5ProjectController.currentLoadStepIndex + 1) + " / " + hdf5ProjectController.loadSteps.length + ")"
                color: "#4fc3f7"
                font.pixelSize: 12
                font.bold: true
                font.family: "monospace"
            }

            Rectangle {
                width: 1
                height: 22
                color: "#555555"
            }

            // Macro von Mises stress readout
            Text {
                text: qsTr("σ_vm: ") + (hdf5ProjectController.macroVonMises > 0 ? (hdf5ProjectController.macroVonMises >= 1e5 || hdf5ProjectController.macroVonMises < 1e-3 ? hdf5ProjectController.macroVonMises.toExponential(2) : hdf5ProjectController.macroVonMises.toFixed(2)) + " Pa" : "0")
                color: "#ffb74d"
                font.pixelSize: 11
                font.family: inter.name
            }

            // Direct Statistics shortcut
            Rectangle {
                id: timelineStatsBtn
                Layout.preferredHeight: 28
                Layout.preferredWidth: tlStatsRow.implicitWidth + 20
                radius: 6
                color: tlStatsMouse.pressed ? "#18242c" : (tlStatsMouse.containsMouse ? "#273946" : "#1f2e38")
                border.color: tlStatsMouse.containsMouse ? "#4fc3f7" : "#37474f"
                border.width: 1

                Row {
                    id: tlStatsRow
                    anchors.centerIn: parent
                    spacing: 6

                    Canvas {
                        width: 13; height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();
                            ctx.fillStyle = "#4fc3f7";
                            ctx.fillRect(0, 5, 3, 7);
                            ctx.fillRect(5, 0, 3, 12);
                            ctx.fillRect(10, 3, 3, 9);
                        }
                    }

                    Text {
                        text: qsTr("Statistics")
                        font.family: inter.name
                        font.pixelSize: 12
                        font.bold: true
                        color: tlStatsMouse.containsMouse ? "#ffffff" : "#cbe4f2"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                ToolTip.visible: tlStatsMouse.containsMouse
                ToolTip.text: qsTr("Open Statistics window in Deformed State mode")

                MouseArea {
                    id: tlStatsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        statisticsLoader.active = true;
                        if (statisticsLoader.item) {
                            statisticsLoader.item.visible = true;
                            statisticsLoader.item.raise();
                        }
                        statisticsController.setMode("Deformed");
                    }
                }
            }

            // Close button (x) to hide loadstep control window
            Rectangle {
                id: tlCloseBtn
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: 10
                color: tlCloseMouse.pressed ? "#333333" : (tlCloseMouse.containsMouse ? "#2a2a2a" : "transparent")
                border.color: tlCloseMouse.containsMouse ? "#555555" : "transparent"
                border.width: 1

                Canvas {
                    anchors.centerIn: parent
                    width: 8; height: 8
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.beginPath();
                        ctx.moveTo(1.5, 1.5); ctx.lineTo(6.5, 6.5);
                        ctx.moveTo(6.5, 1.5); ctx.lineTo(1.5, 6.5);
                        ctx.strokeStyle = tlCloseMouse.containsMouse ? "#ffffff" : "#888888";
                        ctx.lineWidth = 1.6;
                        ctx.lineCap = "round";
                        ctx.stroke();
                    }
                }

                ToolTip.visible: tlCloseMouse.containsMouse
                ToolTip.text: qsTr("Hide load step control")

                MouseArea {
                    id: tlCloseMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        checkLoadStep.checked = false;
                        checkAll.checked = false;
                    }
                }
            }
        }
    }
}
