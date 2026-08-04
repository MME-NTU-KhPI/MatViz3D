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
                                onTriggered: exportController.saveAsImage(glWidget)
                            }
                            Action
                            {
                                text: qsTr("Save as SVG");
                                icon.source: "qrc:/img/fileMenu/save_svg.svg"
                                onTriggered: exportController.saveAsSVG(glWidget)
                            }
                            Action
                            {
                                text: qsTr("Make screenshot");
                                icon.source: "qrc:/img/fileMenu/make_screenshot.svg"
                                onTriggered: exportController.copyToClipboard(glWidget)
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
                                onTriggered: exportController.openHDF5()
                            }
                            MenuSeparator { }
                            Action
                            {
                                text: qsTr("Estimate stresses");
                                icon.source: "qrc:/img/fileMenu/estimate_stresses.svg"
                                onTriggered: console.log("Estimate stresses")
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

                            MenuItem { id: checkAll; text: qsTr("All"); checkable: true; checked: true; onTriggered: { checkConsole.checked = checked; checkData.checked = checked; checkToolBar.checked = checked; checkConsole.triggered(); checkData.triggered(); checkToolBar.triggered(); } }
                            MenuItem { id: checkConsole; text: qsTr("Console"); checkable: true; checked: true; onTriggered: { _itemConsole.visible = checked; if (!checked) checkAll.checked = false; } }
                            MenuItem { id: checkData; text: qsTr("Data"); checkable: true; checked: true; onTriggered: { _itemData.visible = checked; if (!checked) checkAll.checked = false; } }
                            MenuItem { id: checkToolBar; text: qsTr("Tool Bar"); checkable: true; checked: true; onTriggered: { _itemToolBar.visible = checked; if (!checked) checkAll.checked = false; } }
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

                // Отрисовка осей
                Item {
                    id: axisLabelOverlay
                    anchors.fill: parent
                    anchors.margins: 10

                    Text {
                        text: "X"
                        color: "#FF4444"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 40
                    }
                    Text {
                        text: "Y"
                        color: "#44FF44"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.right: parent.right
                        anchors.rightMargin: 50
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 85
                    }
                    Text {
                        text: "Z"
                        color: "#4488FF"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.right: parent.right
                        anchors.rightMargin: 85
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 40
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
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 20
                width: mainWindow.width < 1250 ? 310 : 350
                height: mainWindow.height < 780 ? 440 : 455
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
                                    model: ListModel {
                                        id: model
                                        ListElement { text: "Neumann" }
                                        ListElement { text: "Moore" }
                                        ListElement { text: "Probability Ellipse" }
                                        ListElement { text: "Probability Circle" }
                                        ListElement { text: "Probability Algorithm" }
                                        ListElement { text: "Composite" }
                                        ListElement { text: "DLCA" }
                                    }
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
                                        if (find(editText) === -1) model.append({text: editText})
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
                width: 434
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
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
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
                            onClicked: {
                                gifToggle.isActive = !gifToggle.isActive;
                                Parameters.setIsGifRecording(gifToggle.isActive);
                            }
                        }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/screenIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: exportController.saveAsImage(glWidget) }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/zoom-inIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: glWidget.zoomIn(); }
                    }

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        source: "qrc:/img/toolBar/zoom-outIcon.svg"
                        fillMode: Image.PreserveAspectFit
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: glWidget.zoomOut(); }
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
                                onClicked: mainWindowWrapper.isometricViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/dimetric_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                onClicked: mainWindowWrapper.dimetricViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/front_Cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                onClicked: mainWindowWrapper.frontViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/back_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                onClicked: mainWindowWrapper.backViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/top_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                onClicked: mainWindowWrapper.topViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/bottom_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                onClicked: mainWindowWrapper.bottomViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/left_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
                                onClicked: mainWindowWrapper.leftViewButton()
                            }
                            MenuItem {
                                contentItem: Image {
                                    source: "qrc:/img/toolBar/views3d/rigft_cube.svg"
                                    fillMode: Image.PreserveAspectFit
                                }
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

                            Text {
                                anchors.centerIn: parent
                                text: (animationSpeedSlider.value / 250).toFixed(1) + "x"
                                font.family: montserrat.name
                                font.pixelSize: 13
                                font.bold: true
                                color: speedPopup.visible ? "#00897b" : "#CFCECE"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: speedPopup.visible ? speedPopup.close() : speedPopup.open()
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
}
