import QtQuick 2.15
import QtQuick.Controls 2.15


Window {
    id: mainWindow
    width: 1280
    height: 832
    color: "#363636"
    property alias size_column1: size_column1
    property alias _item7: _item7
    property alias num_text1: num_text1
    property alias row: row
    property alias size_radioButton: size_radioButton
    property alias _item4: _item4
    property alias size_textInput: size_textInput
    property alias _item: _item
    property alias comboBox: comboBox
    property alias _item6_menuBar: _item6_menuBar
    property alias _item5_menuBar: _item5_menuBar
    property alias _item4_menuBar: _item4_menuBar
    property alias _item3_menuBar: _item3_menuBar
    property alias _item1_menuBar: _item1_menuBar
    minimumHeight: 832
    minimumWidth: 1280
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
                    width: 128
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
                    width: 149
                    height: 49

                    Button {
                        id: buttonStatistics
                        x: 0
                        y: 0
                        width: 109
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
                            anchors.centerIn: parent
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
                            anchors.centerIn: parent
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
    }

    Item {
        id: _itemData
        x: 30
        y: 97
        width: 350
        height: 455
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
                                anchors.fill: parent  // MouseArea займає всю площу зображення
                                cursorShape: Qt.PointingHandCursor  // Змінює курсор на руку при наведенні

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
                                ListElement { text: "von Neumann" }
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
                                }

                                else if (comboBox.currentIndex === 1) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 2) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 3) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 4) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    probabilityAlgorithmLoader.active = true;
                                    probabilityAlgorithmLoader.item.visible = true;
                                }

                                else if (comboBox.currentIndex === 5) {
                                    comp_alg_item.visible = true
                                    poly_alg_item.visible = false
                                }

                                else if (comboBox.currentIndex === 6) {
                                    poly_alg_item.visible = true
                                    comp_alg_item.visible = false
                                }

                                else {
                                    empty_alg_item.visible = true
                                    comp_alg_item.visible = false
                                    poly_alg_item.visible = false
                                }
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
                                        }

                                        RadioButton {
                                            id: concentration_radioButton
                                            text: qsTr("Concentration")
                                            font.pixelSize: 15
                                            font.family: montserrat.name
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
                                id: _item6
                                width: 224
                                height: 85

                                Column {
                                    id: column3
                                    x: 0
                                    y: 106
                                    anchors.fill: parent
                                    topPadding: 20
                                    spacing: 10
                                    Text {
                                        id: _text2
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Algorithm:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    ComboBox {
                                        id: comboBox1
                                        width: 224
                                        height: 28
                                        onAccepted: {
                                            if (find(editText) === -1)
                                                model1.append({text: editText})
                                        }
                                        model: ListModel {
                                            id: model1
                                            ListElement {
                                                text: "compo 1"
                                            }

                                            ListElement {
                                                text: "compo 5"
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
                                id: _item8
                                width: 224
                                height: 85
                                anchors.horizontalCenter: parent.horizontalCenter

                                Column {
                                    id: size_column2
                                    x: -63
                                    y: -187
                                    anchors.fill: parent
                                    topPadding: 20
                                    spacing: 10
                                    Text {
                                        id: size_text2
                                        width: 224
                                        height: 24
                                        color: "#c6c6c6"
                                        text: qsTr("Radius:")
                                        font.pixelSize: 20
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                    }

                                    Rectangle {
                                        id: recInput1_pav2
                                        width: 224
                                        height: 28
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                        border.width: 1
                                        TextInput {
                                            id: size_textInput2
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
                    }
                }
            }
        }
    }
}



/*##^##
Designer {
    D{i:0}D{i:79;invisible:true}D{i:80;invisible:true}D{i:82;invisible:true}D{i:92}D{i:94}
D{i:101;invisible:true}D{i:102}D{i:107}
}
##^##*/
