import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import parameters 1.0

import OpenGLUnderQML 1.0

Window {
    id: mainWindow
    width: 1024
    height: 768
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
        id: probabilityAlgorithmLoader
        source: "ProbabilityAlgorithmView.qml"
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

    Grid {
        id: grid
        anchors.fill: parent
        rows: 3
        columns: 1

        Rectangle {
            id: menuBar_rec
            width: mb_row.width
            height: mb_row.height
            color: "#282828"

            Row {
                id: mb_row
                width: grid.width
                height: 67
                bottomPadding: 9
                topPadding: 9
                rightPadding: 40
                leftPadding: 40

                Item {
                    id: _item1_menuBar
                    width: 88
                    height: 49

                    Image {
                        id: iconMenu_img
                        width: 48
                        height: 48
                        source: "qrc:/img/iconMenu.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }

                Item {
                    id: _item2_menuBar
                    width: 83
                    height: 49

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
                            font.pixelSize: 16
                            font.family: inter.name

                            Action {
                                text: qsTr("Save as image")
                                onTriggered: console.log("Save as image")
                            }
                            Action {
                                text: qsTr("Make screenshot")
                                onTriggered: console.log("Make screenshot")
                            }
                            MenuSeparator { }
                            Action {
                                text: qsTr("Export to wrl")
                                onTriggered: console.log("Export to wrl")
                            }
                            Action {
                                text: qsTr("Export to csv")
                                onTriggered: console.log("Export to csv")
                            }
                            Action {
                                text: qsTr("Save project as HDF5")
                                onTriggered: console.log("Save project as HDF5")
                            }
                            Action {
                                text: qsTr("Open project")
                                onTriggered: console.log("Save project as HDF5")
                            }
                            MenuSeparator { }
                            Action {
                                text: qsTr("Estimate stresses")
                                onTriggered: console.log("Estimate stresses")
                            }
                            Action {
                                text: qsTr("Edit material data")
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
                    width: 140
                    height: 49

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
                                    checkAnimation.checked = checked;
                                    checkConsole.checked = checked;
                                    checkData.checked = checked;

                                    checkAnimation.triggered();
                                    checkConsole.triggered();
                                    checkData.triggered();
                                }
                            }
                            MenuItem {
                                id: checkAnimation
                                text: qsTr("Animation")
                                checkable: true
                                checked: true

                                onTriggered: {
                                    _itemAnimationWidget.visible = checked;

                                    if (!checked) {
                                        checkAll.checked = false;
                                    }
                                }
                            }
                            MenuItem {
                                id: checkConsole
                                text: qsTr("Console")
                                checkable: true
                                checked: true

                                onTriggered: {
                                    _itemConsole.visible = checked;

                                    if (!checked) {
                                        checkAll.checked = false;
                                    }
                                }
                            }
                            MenuItem {
                                id: checkData
                                text: qsTr("Data")
                                checkable: true
                                checked: true

                                onTriggered: {
                                    _itemData.visible = checked;

                                    if (!checked) {
                                        checkAll.checked = false;
                                    }
                                }
                            }

                            MenuItem {
                                id: checkToolBar
                                text: qsTr("Tool Bar")
                                checkable: true
                                checked: true

                                onTriggered: {
                                    _itemToolBar.visible = checked;

                                    if (!checked) {
                                        checkAll.checked = false;
                                    }
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
                    width: 146
                    height: 49

                    Button {
                        id: buttonStatistics
                        x: 0
                        y: 0
                        width: 95
                        height: _item4_menuBar.height
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
                            statisticsLoader.active = true;
                            statisticsLoader.item.visible = true;
                        }
                    }
                }

                Item {
                    id: _item5_menuBar
                    width: 115
                    height: 49

                    Button {
                        id: buttonAbout
                        x: 0
                        y: 0
                        width: 75
                        height: _item5_menuBar.height
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
                    id: _item6_menuBar
                    height: 49
                    width: parent.width - (_item1_menuBar.width + _item2_menuBar.width + _item3_menuBar.width + _item4_menuBar.width + _item5_menuBar.width) - 80

                    Text {
                        id: _text_menuBar
                        color: "#00897b"
                        textFormat: Text.RichText
                        text: qsTr("Material<span style='color: #00564D;'>Viz</span>")
                        anchors.fill: parent
                        font.pixelSize: 36
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        font.styleName: "Bold"
                        font.family: inter.name
                    }
                }
            }
        }

        Item {
            id: _item_GLWidget
            width: mainWindow.width
            height: mainWindow.height - menuBar_rec.height - (_itemConsole.visible ? _itemConsole.height : 0)

            // width: 300
            // height: 300

            OpenGLWidgetQML {
                width: _item_GLWidget.width
                height: _item_GLWidget.height
            }
        }

        Item {
            id: _itemConsole
            width: mainWindow.width
            height: mainWindow.height < 780 ? "170" : "220"
            visible: true

            Rectangle {
                id: recConsole
                anchors.fill: parent
                color: "#282828"
                border.color: "#00000000"

                Column {
                    id: columnConsole
                    anchors.fill: parent
                    bottomPadding: 15
                    topPadding: 15
                    rightPadding: 20
                    leftPadding: 20

                    Item {
                        id: _item8
                        height: 10
                        width: columnConsole.width - 40

                        Image {
                            id: image6
                            x: _item8.width - 10
                            width: 10
                            height: 10
                            source: "qrc:/img/closeData.png"
                            fillMode: Image.PreserveAspectFit

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor

                                onClicked: {
                                    _itemConsole.visible = false;
                                    checkConsole.checked = false;
                                    checkAll.checked = false;
                                }
                            }
                        }
                    }

                    Item {
                        id: _item24
                        width: columnConsole.width - 40
                        height: columnConsole.height - _item8.height - 30

                        Text {
                            id: consoleOutput
                            anchors.fill: parent
                            wrapMode: TextArea.Wrap
                        }
                    }
                }
            }
        }
    }

    Item {
        id: _itemData
        x: 30
        y: 97
        width: mainWindow.width < 1250 ? "310" : "350"
        height: mainWindow.height < 780 ? "440" : "455"
        visible: true

        Rectangle {
            id: data_rec
            color: "#80282828"
            radius: 13
            anchors.fill: parent

            Column {
                id: data_column
                anchors.fill: parent

                Row {
                    id: headerData_row
                    width: data_column.width
                    height: 32
                    rightPadding: 29
                    leftPadding: 29
                    bottomPadding: 5
                    topPadding: 10

                    Item {
                        id: _item1
                        width: headerData_row.width * 0.5 - 29
                        height: 17

                        Text {
                            id: _text
                            color: "#d9d9d9"
                            text: qsTr("Data")
                            font.pixelSize: 14
                            font.family: montserrat.name
                        }
                    }

                    Item {
                        id: _item2
                        width: headerData_row.width * 0.5 - 29
                        height: 17

                        Image {
                            id: image
                            width: 10
                            height: 10
                            source: "qrc:/img/closeData.png"
                            fillMode: Image.PreserveAspectFit

                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor

                                onClicked: {
                                    _itemData.visible = false;
                                    checkData.checked = false;
                                    checkAll.checked = false;
                                }
                            }
                        }
                    }
                }

                Item {
                    id: _item_column_alg
                    width: data_column.width
                    height: 70

                    Column {
                        id: column
                        x: 0
                        y: 0
                        width: column.childrenRect.width
                        height: column.childrenRect.height
                        spacing: 10
                        anchors.centerIn: parent

                        Text {
                            id: _text1
                            width: 224
                            height: 24
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
                            displayText: "---"
                            font.pointSize: 10
                            font.family: montserrat.name
                            editable: true
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

                            onActivated: {
                                if (comboBox.currentIndex === 0) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 1) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 2) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 3) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 4) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                    probabilityAlgorithmLoader.active = true;
                                    probabilityAlgorithmLoader.item.visible = true;
                                }

                                else if (comboBox.currentIndex === 5) {
                                    comp_alg_item.visible = true
                                    poly_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 6) {
                                    dlca_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    poly_alg_item.visible = false
                                }

                                else {
                                    empty_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    poly_alg_item.visible = false
                                    dlca_alg_item.visible = false
                                }

                                Qt.callLater(() => {
                                    console.log("Algorithm selected:", currentText)
                                    Parameters.setAlgorithm(currentText)
                                })
                            }

                            onAccepted: {
                                if (find(editText) === -1)
                                    model.append({text: editText})
                            }
                        }
                    }
                }

                Item {
                    id: _item3
                    width: data_column.width
                    height: parent.height - (headerData_row.height + _item_column_alg.height + _item_start.height)
                    visible: true

                    Item {
                        id: empty_alg_item
                        anchors.fill: parent
                        visible: true
                    }

                    Item {
                        id: poly_alg_item
                        anchors.fill: parent
                        visible: false

                        Column {
                            id: column1
                            anchors.fill: parent


                            Item {
                                id: _item
                                width: 224
                                height: 77
                                anchors.horizontalCenter: parent.horizontalCenter

                                Column {
                                    id: size_column
                                    anchors.fill: parent
                                    topPadding: 15
                                    spacing: 10

                                    Text {
                                        id: size_text
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Cube size:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    Rectangle {
                                        id: recInput1_pav
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        border.color: "#969696"
                                        border.width: 1
                                        radius: 11

                                        TextInput {
                                            id: size_textInput
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            color: "#969696"
                                            text: qsTr("0")
                                            font.pixelSize: 12
                                            font.family: montserrat.name
                                            verticalAlignment: Text.AlignTop
                                            font.bold: true
                                            bottomPadding: 1
                                            topPadding: 1
                                            rightPadding: 10
                                            leftPadding: 10
                                            horizontalAlignment: Text.AlignLeft
                                            padding: 3.5

                                            onTextChanged: {
                                                console.log("Change: ", text);
                                                Parameters.setSize(parseInt(text, 10))
                                            }
                                        }
                                    }
                                }
                            }

                            Item {
                                id: _item4
                                width: 224
                                height: 110
                                anchors.horizontalCenter: parent.horizontalCenter

                                Column {
                                    id: num_column
                                    anchors.fill: parent
                                    topPadding: 15
                                    spacing: 10

                                    Row {
                                        id: row
                                        width: parent.width
                                        height: size_radioButton.height
                                        leftPadding: -2

                                        RadioButton {
                                            id: size_radioButton
                                            text: qsTr("Size")
                                            font.pixelSize: 15
                                            font.family: montserrat.name

                                            checked: true
                                            onClicked: {
                                                Parameters.setPointsMode("count")
                                                Parameters.processPointInput(num_textInput.text)
                                            }
                                        }

                                        RadioButton {
                                            id: concentration_radioButton
                                            text: qsTr("Concentration")
                                            font.pixelSize: 15
                                            font.family: montserrat.name

                                            onClicked: {
                                                Parameters.setPointsMode("density")
                                                Parameters.processPointInput(num_textInput.text)
                                            }
                                        }
                                    }

                                    Rectangle {
                                        id: recInput2_pav
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        border.color: "#969696"
                                        border.width: 1
                                        radius: 11

                                        TextInput {
                                            id: num_textInput
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            color: "#969696"
                                            text: qsTr("0")
                                            font.pixelSize: 12
                                            font.family: montserrat.name
                                            verticalAlignment: Text.AlignTop
                                            font.bold: true
                                            rightPadding: 10
                                            leftPadding: 10
                                            bottomPadding: 1
                                            topPadding: 1
                                            horizontalAlignment: Text.AlignLeft

                                            onTextChanged: {
                                                console.log("Change: ", text);
                                                Parameters.processPointInput(text);
                                            }
                                        }
                                    }
                                }
                            }

                            Item {
                                id: _item5
                                width: 224
                                height: 77
                                Column {
                                    id: num_column1
                                    anchors.fill: parent
                                    topPadding: 15
                                    spacing: 10
                                    Text {
                                        id: num_text1
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Wave coefficient:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    Rectangle {
                                        id: recInput2_pav1
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                        border.width: 1
                                        TextInput {
                                            id: num_textInput1
                                            color: "#969696"
                                            text: qsTr("0")
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignTop
                                            topPadding: 1
                                            rightPadding: 10
                                            leftPadding: 10
                                            font.family: montserrat.name
                                            font.bold: true
                                            bottomPadding: 1
                                        }
                                    }
                                }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    Item {
                        id: comp_alg_item
                        anchors.fill: parent
                        visible: false

                        Column {
                            id: column2
                            x: 0
                            y: 0
                            anchors.fill: parent


                            Item {
                                id: _item23
                                width: 224
                                height: 85
                                Row {
                                    id: comp_row_material
                                    anchors.fill: parent
                                    spacing: 7
                                    x: 0
                                    y: 106
                                    Column {
                                        id: column9
                                        width: (comp_row_material.width / 2)
                                        height: 85
                                        topPadding: 20
                                        spacing: 10
                                        Text {
                                            id: _text5
                                            width: (comp_row_material.width / 2)
                                            height: 24
                                            color: "#c6c6c6"
                                            text: qsTr("Material 1:")
                                            font.pixelSize: 20
                                            font.styleName: "Bold"
                                            font.family: inter.name
                                        }

                                        ComboBox {
                                            id: comboBox3
                                            width: (comp_row_material.width / 2)
                                            height: 28
                                            onAccepted: {
                                                if (find(editText) === -1)
                                                    model3.append({text: editText})
                                            }
                                            model: ListModel {
                                                id: model3
                                                ListElement {
                                                    text: "fcc"
                                                }

                                                ListElement {
                                                    text: "bcc"
                                                }
                                            }
                                            leftPadding: 10
                                            font.pointSize: 10
                                            font.family: montserrat.name
                                            editable: true
                                            displayText: "---"
                                            background: Rectangle {
                                                color: "#282828"
                                                radius: 11
                                                border.color: "#969696"
                                            }
                                        }
                                        anchors.verticalCenter : _item23.verticalCenter
                                    }
                                    Column {
                                        id: column9_2
                                        width: (comp_row_material.width / 2)
                                        height: 85
                                        topPadding: 20
                                        spacing: 10
                                        Text {
                                            id: _text5_2
                                            width: (comp_row_material.width / 2)
                                            height: 24
                                            color: "#c6c6c6"
                                            text: qsTr("Material 2:")
                                            font.pixelSize: 20
                                            font.styleName: "Bold"
                                            font.family: inter.name
                                        }

                                        ComboBox {
                                            id: comboBox3_2
                                            width: (comp_row_material.width / 2)
                                            height: 28
                                            onAccepted: {
                                                if (find(editText) === -1)
                                                    model3.append({text: editText})
                                            }
                                            model: ListModel {
                                                id: model3_2
                                                ListElement {
                                                    text: "fcc"
                                                }

                                                ListElement {
                                                    text: "bcc"
                                                }
                                            }
                                            leftPadding: 10
                                            font.pointSize: 10
                                            font.family: montserrat.name
                                            editable: true
                                            displayText: "---"
                                            background: Rectangle {
                                                color: "#282828"
                                                radius: 11
                                                border.color: "#969696"
                                            }
                                        }
                                        anchors.verticalCenter : _item23.verticalCenter
                                    }
                                }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Item {
                                id: _item7
                                width: 224
                                height: 85
                                anchors.horizontalCenter: parent.horizontalCenter

                                Column {
                                    id: size_column1
                                    x: -63
                                    y: 0
                                    anchors.fill: parent
                                    topPadding: 20
                                    spacing: 10
                                    Text {
                                        id: size_text1
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Cube size:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    Rectangle {
                                        id: recInput1_pav1
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                        border.width: 1
                                        TextInput {
                                            id: size_textInput1
                                            color: "#969696"
                                            text: qsTr("0")
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignTop
                                            topPadding: 1
                                            rightPadding: 10
                                            padding: 3.5
                                            leftPadding: 10
                                            font.family: montserrat.name
                                            font.bold: true
                                            bottomPadding: 1
                                        }
                                    }
                                }
                            }

                            Item {
                                id: _item6
                                width: 224
                                height: 110
                                Column {
                                    id: num_column3
                                    anchors.fill: parent
                                    topPadding: 15
                                    spacing: 10
                                    Row {
                                        id: row5
                                        width: parent.width
                                        height: size_radioButton2.height
                                        leftPadding: -2
                                        RadioButton {
                                            id: size_radioButton2
                                            text: qsTr("Radius")
                                            font.pixelSize: 15
                                            font.family: montserrat.name
                                        }

                                        RadioButton {
                                            id: concentration_radioButton2
                                            text: qsTr("Concentration")
                                            font.pixelSize: 15
                                            font.family: montserrat.name
                                        }
                                    }

                                    Rectangle {
                                        id: recInput2_pav3
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                        border.width: 1
                                        TextInput {
                                            id: num_textInput3
                                            color: "#969696"
                                            text: qsTr("0")
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignTop
                                            topPadding: 1
                                            rightPadding: 10
                                            leftPadding: 10
                                            font.family: montserrat.name
                                            font.bold: true
                                            bottomPadding: 1
                                        }
                                    }
                                }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    Item {
                        id: dlca_alg_item
                        visible: false
                        anchors.fill: parent
                        Column {
                            id: column4
                            anchors.fill: parent


                            Item {
                                id: _item9
                                width: 224
                                height: 85
                                Column {
                                    id: size_column3
                                    anchors.fill: parent
                                    topPadding: 20
                                    spacing: 10
                                    Text {
                                        id: size_text3
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Cube size:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    Rectangle {
                                        id: recInput1_pav3
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                        border.width: 1
                                        TextInput {
                                            id: size_textInput3
                                            color: "#969696"
                                            text: qsTr("0")
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignTop
                                            topPadding: 1
                                            rightPadding: 10
                                            padding: 3.5
                                            leftPadding: 10
                                            font.family: montserrat.name
                                            font.bold: true
                                            bottomPadding: 1
                                        }
                                    }
                                }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Item {
                                id: _item10
                                width: 224
                                height: 100
                                Column {
                                    id: num_column2
                                    anchors.fill: parent
                                    topPadding: 15
                                    spacing: 10
                                    Row {
                                        id: row1
                                        width: parent.width
                                        height: size_radioButton1.height
                                        leftPadding: -2
                                        RadioButton {
                                            id: size_radioButton1
                                            text: qsTr("Size")
                                            font.pixelSize: 15
                                            font.family: montserrat.name
                                        }

                                        RadioButton {
                                            id: concentration_radioButton1
                                            text: qsTr("Concentration")
                                            font.pixelSize: 15
                                            font.family: montserrat.name
                                        }
                                    }

                                    Rectangle {
                                        id: recInput2_pav2
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                        border.width: 1
                                        TextInput {
                                            id: num_textInput2
                                            color: "#969696"
                                            text: qsTr("0")
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignTop
                                            topPadding: 1
                                            rightPadding: 10
                                            leftPadding: 10
                                            font.family: montserrat.name
                                            font.bold: true
                                            bottomPadding: 1
                                        }
                                    }
                                }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Item {
                                id: _item11
                                width: 224
                                height: 85
                                Column {
                                    id: column5
                                    x: 0
                                    y: 106
                                    anchors.fill: parent
                                    topPadding: 20
                                    spacing: 10
                                    Text {
                                        id: _text3
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Material:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    ComboBox {
                                        id: comboBox2
                                        width: 224
                                        height: 28
                                        onAccepted: {
                                            if (find(editText) === -1)
                                                model2.append({text: editText})
                                        }
                                        model: ListModel {
                                            id: model2
                                            ListElement {
                                                text: "fcc"
                                            }

                                            ListElement {
                                                text: "bcc"
                                            }
                                        }
                                        leftPadding: 10
                                        font.pointSize: 10
                                        font.family: montserrat.name
                                        editable: true
                                        displayText: "---"
                                        background: Rectangle {
                                            color: "#282828"
                                            radius: 11
                                            border.color: "#969696"
                                        }
                                    }
                                    anchors.centerIn: parent
                                }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                        }
                    }
                }

                Item {
                    id: _item_start
                    width: data_column.width
                    height: 80

                    Button {
                        id: start_button
                        width: 105
                        height: 50
                        text: qsTr("START")
                        font.pixelSize: 20
                        font.family: inter.name
                        font.bold: true
                        anchors.centerIn: parent

                        background: Rectangle {
                            id: buttonBackground2
                            width: 105
                            height: 50
                            radius: 12
                            color: "#282828"
                            border.color: "#969696"
                            border.width: 1

                            MouseArea {
                                id: hoverArea2
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
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        onClicked: mainWindowWrapper.onStartButton()
                    }
                }
            }
        }
    }

    Item {
        id: _itemAnimationWidget
        x: parent.width - (_itemAnimationWidget.width + 30)
        y: 97
        width: mainWindow.width < 1250 ? "310" : "350"
        height: 275

        Rectangle {
            id: aniWidget_rec
            color: "#80282828"
            radius: 13
            anchors.fill: parent

            Column {
                id: column6
                anchors.fill: parent

                Row {
                    id: headerAnimWidget_row
                    width: data_column.width
                    height: 32
                    topPadding: 10
                    rightPadding: 29
                    leftPadding: 29
                    Item {
                        id: _item12
                        width: headerAnimWidget_row.width * 0.5 - 29
                        height: 17
                        Text {
                            id: _text4
                            color: "#d9d9d9"
                            text: qsTr("Animation controller")
                            font.pixelSize: 14
                            font.family: montserrat.name
                        }
                    }

                    Item {
                        id: _item13
                        width: headerAnimWidget_row.width * 0.5 - 29
                        height: 17
                        Image {
                            id: image1
                            width: 10
                            height: 10
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            source: "qrc:/img/closeData.png"
                            fillMode: Image.PreserveAspectFit
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    _itemAnimationWidget.visible = false;
                                    checkAnimation.checked = false;
                                    checkAll.checked = false;
                                }
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                    bottomPadding: 5
                }

                Item {
                    id: _item14
                    width: parent.width
                    height: (parent.height - headerAnimWidget_row.height) / 4

                    Column {
                        id: column7
                        width: 228
                        height: parent.height
                        topPadding: 5
                        spacing: 15
                        anchors.centerIn: parent

                        Text {
                            id: exploded_text
                            width: 224
                            height: 24
                            color: "#c6c6c6"
                            text: qsTr("Exploded view")
                            font.pixelSize: 20
                            leftPadding: 12
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        Slider {
                            id: slider
                            value: 0
                            width: 228
                            height: 18
                        }
                    }
                }

                Item {
                    id: _item15
                    width: parent.width
                    height: (parent.height - headerAnimWidget_row.height) / 4
                    Column {
                        id: column8
                        width: 228
                        height: parent.height
                        topPadding: 15
                        spacing: 15
                        Text {
                            id: exploded_text1
                            width: 224
                            height: 24
                            color: "#c6c6c6"
                            text: qsTr("Animation speed")
                            font.pixelSize: 20
                            leftPadding: 12
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        Slider {
                            id: slider1
                            width: 228
                            height: 18
                            value: 0
                        }
                        anchors.centerIn: parent
                    }
                }

                Item {
                    id: _item16
                    width: parent.width
                    height: (parent.height - headerAnimWidget_row.height) / 4

                    Row {
                        id: row2
                        width: 235
                        height: parent.height
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: 15

                        Text {
                            id: exploded_text2
                            width: parent.width * 0.8
                            height: parent.height
                            color: "#c6c6c6"
                            text: qsTr("Animation on/off")
                            font.pixelSize: 20
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        Switch {
                            id: _switch_animation
                            display: AbstractButton.IconOnly
                            width: 50
                            height: 20
                            scale: 0.7
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Item {
                    id: _item17
                    width: parent.width
                    height: (parent.height - headerAnimWidget_row.height) / 4
                    Row {
                        id: row3
                        width: 235
                        height: parent.height
                        Text {
                            id: exploded_text3
                            width: parent.width * 0.8
                            height: parent.height
                            color: "#c6c6c6"
                            text: qsTr("Wireframe on/off")
                            font.pixelSize: 20
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        Switch {
                            id: _switch_animation1
                            width: 50
                            height: 20
                            anchors.verticalCenter: parent.verticalCenter
                            scale: 0.7
                            display: AbstractButton.IconOnly
                        }
                        anchors.centerIn: parent
                    }
                }
            }
        }
    }

    Item {
        id: _itemToolBar
        y: 97
        x: mainWindow.width - (mainWindow.width / 2) - (_itemToolBar.width / 2)
        width: 284
        height: 26

        Row {
            id: row4
            anchors.fill: parent

            Item {
                id: _item18
                width: parent.height + 17
                height: parent.height

                Image {
                    id: image2
                    width: 26
                    height: 26
                    property bool widgetsHidden: false
                    source: widgetsHidden ? "qrc:/img/toolBar/viewIconHidden.svg" : "qrc:/img/toolBar/viewIcon.svg"
                    fillMode: Image.PreserveAspectFit

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            image2.widgetsHidden = !image2.widgetsHidden;

                            _itemAnimationWidget.visible = !image2.widgetsHidden;
                            _itemData.visible = !image2.widgetsHidden;
                            _itemConsole.visible = !image2.widgetsHidden;
                            checkData.checked = !image2.widgetsHidden;
                            checkAnimation.checked = !image2.widgetsHidden;
                            checkConsole.checked = !image2.widgetsHidden;
                            checkAll.checked = !image2.widgetsHidden;
                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            Item {
                id: _item19
                width: parent.height + 17
                height: parent.height

                Image {
                    id: image3
                    x: 0
                    y: 0
                    width: 26
                    height: 26
                    source: "qrc:/img/toolBar/saveIcon.svg"
                    fillMode: Image.PreserveAspectFit

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {

                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            Item {
                id: _itemGif
                width: parent.height + 17
                height: parent.height

                Image {
                    id: imagegif
                    x: 0
                    y: 0
                    width: 26
                    height: 26
                    source: "qrc:/img/toolBar/gifIcon.svg"
                    fillMode: Image.PreserveAspectFit

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {

                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            Item {
                id: _itemScreen
                width: parent.height + 17
                height: parent.height

                Image {
                    id: imagescreen
                    x: 0
                    y: 0
                    width: 26
                    height: 26
                    source: "qrc:/img/toolBar/screenIcon.svg"
                    fillMode: Image.PreserveAspectFit

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {

                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            Item {
                id: _item20
                width: parent.height + 17
                height: parent.height

                Image {
                    id: image4
                    x: 0
                    y: 0
                    width: 26
                    height: 26
                    source: "qrc:/img/toolBar/zoom-inIcon.svg"
                    fillMode: Image.PreserveAspectFit

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {

                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            Item {
                id: _item21
                width: parent.height + 17
                height: parent.height

                Image {
                    id: image5
                    x: 0
                    y: 0
                    width: 26
                    height: 26
                    source: "qrc:/img/toolBar/zoom-outIcon.svg"
                    fillMode: Image.PreserveAspectFit

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {

                        }
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            Item {
                id: _item22
                width: parent.height
                height: parent.height

                Image {
                    id: cubeIcon
                    width: 26
                    height: 26
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
                    title: ""
                    width: 70

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/isometric_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.isometricViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/dimetric_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.dimetricViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/front_Cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.frontViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/back_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.backViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/top_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.topViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/bottom_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.bottomViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/left_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.leftViewButton()
                    }

                    MenuItem {
                        contentItem: Item {
                            width: 26
                            height: 26
                            Image {
                                source: "qrc:/img/toolBar/views3d/rigft_cube.svg"
                                width: parent.width
                                height: parent.height
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        onClicked: mainWindowWrapper.rightViewButton()
                    }
                }
            }
        }
    }
}
