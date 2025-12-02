import QtQuick 2.15
import parameters 1.0
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import OpenGLUnderQML 1.0

Window {
    id: tensorVisualizationView
    objectName: "tensorVisualizationView"
    width: 800
    height: 768
    color: "#363636"
    minimumHeight: 768
    minimumWidth: 800
    title: qsTr("Tensor Visualization")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    Component.onCompleted: {
            Parameters.loadMaterialListFromDatabase()
        }

    function appendToConsole(text) {
        consoleOutput.text += "\n" + text;
    }

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    FontLoader {
        id: montserrat
        source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf"
    }

    Item {
        id: visualizationContainer
        width: parent.width
        height: parent.height - (_itemConsole.visible ? _itemConsole.height : 0)
        anchors.top: parent.top

        TensorWidgetQML {
            id: tensorVisualization
            anchors.fill: parent
        }
    }
    function updateTensorVisualization() {
        appendToConsole("Attempting visualization update...");
        var dep = Parameters.dependence // Young's Modulus E, Shear Modulus G, Poisson's ratio nu
        var values = []

        // Вибираємо правильний масив даних на основі обраної залежності
        if (dep.includes("Young")) {
            values = TensorController.EData
        } else if (dep.includes("Shear")) {
            values = TensorController.GData
        } else if (dep.includes("Poisson")) {
            values = TensorController.NuData
        } else {
            return
        }

        // Передаємо дані у C++ віджет
        if (values.length > 0 && TensorController.GridSize > 0) {
            tensorVisualization.setVisualizationData(
                values,
                TensorController.Thetas,
                TensorController.Phis,
                TensorController.GridSize
            )
            appendToConsole("Data successfully passed to TensorWidgetQML.");
        } else {
            appendToConsole("Error: Data array is empty or GridSize is 0.");
        }
    }

    // --- ЗВ'ЯЗОК: Оновлення при зміні залежності у ComboBox ---
    Connections {
        target: Parameters
        onDependenceChanged: {
            updateTensorVisualization()
        }
    }

    // --- ЗВ'ЯЗОК: Оновлення після розрахунку START ---
    Connections {
        target: TensorController
        onDataUpdated: {
            updateTensorVisualization()
        }
    }

    Item {
        id: _itemConsole
        width: parent.width
        height: parent.height < 600 ? 170 : 220
        anchors.bottom: parent.bottom
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
                        wrapMode: Text.Wrap
                        color: "#CFCECE"
                        font.family: inter.name
                        font.pixelSize: 12
                        text: "Tensor visualization console...\n> Ready"
                    }
                }
            }
        }
    }

    Item {
        id: _itemData
        x: 30
        y: 30
        width: parent.width < 850 ? 310 : 350
        height: 380
        visible: true

        Rectangle {
            id: data_rec
            color: "#80282828"
            radius: 13
            anchors.fill: parent

            Column {
                id: data_column
                anchors.fill: parent
                spacing: 10

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
                            text: qsTr("Tensor Data")
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
                                }
                            }
                        }
                    }
                }

                Item {
                    id: _item_column_alg
                    width: data_column.width
                    height: 200

                    Column {
                        id: column
                        width: 280
                        height: column.childrenRect.height
                        spacing: 25
                        anchors.centerIn: parent

                        // Перший комбобокс
                        Item {
                            id: _item_combo1
                            width: 280
                            height: 70

                            Column {
                                id: combo1_column
                                anchors.fill: parent
                                spacing: 10

                                Text {
                                    id: combo1_text
                                    width: 280
                                    height: 24
                                    color: "#c6c6c6"
                                    text: qsTr("Material Type:")
                                    font.pixelSize: 20
                                    font.styleName: "Bold"
                                    font.family: inter.name
                                    font.bold: true
                                }

                                ComboBox {
                                    id: comboBox1
                                    width: 280
                                    height: 32
                                    leftPadding: 15
                                    displayText: qsTr("Choose example matrix")
                                    font.pointSize: 10
                                    font.family: montserrat.name
                                    editable: true

                                    background: Rectangle {
                                        color: "#282828"
                                        radius: 11
                                        border.color: "#969696"
                                    }

                                    //  Модель із C++ Parameters
                                    model: Parameters.materialList

                                    //  Коли користувач вибрав матеріал
                                    onCurrentTextChanged: {
                                        Qt.callLater(() => {
                                            Parameters.setSelectedMaterial(currentText)
                                        })
                                    }
                                }
                            }
                        }
                        // Другий комбобокс
                        Item {
                            id: _item_combo2
                            width: 280
                            height: 70

                            Column {
                                id: combo2_column
                                anchors.fill: parent
                                spacing: 10

                                Text {
                                    id: combo2_text
                                    width: 280
                                    height: 24
                                    color: "#c6c6c6"
                                    text: qsTr("Dependence Type:")
                                    font.pixelSize: 20
                                    font.styleName: "Bold"
                                    font.family: inter.name
                                    font.bold: true
                                }

                                ComboBox {
                                    id: comboBox2
                                    width: 280
                                    height: 32
                                    leftPadding: 15
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
                                        id: model2
                                        ListElement { text: "Choose dependence" }
                                        ListElement { text: "Young's modulus" }
                                        ListElement { text: "Shear modulus" }
                                        ListElement { text: "Poisson's ratio" }
                                    }

                                    onActivated: (index) => {
                                        const selectedText = model.get(comboBox2.currentIndex).text;
                                        Parameters.setDependence(selectedText);

                                        updateTensorVisualization();
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    id: _item3
                    width: data_column.width
                    height: 40
                    visible: true

                    Item {
                        id: tensor_alg_item
                        anchors.fill: parent
                        visible: true
                    }
                }

                Item {
                    id: _item_start
                    width: data_column.width
                    height: 70

                    Button {
                            id: start_button
                            width: 160
                            height: 50
                            text: qsTr("Visualization")

                            font.pixelSize: 20
                            font.family: inter.name
                            font.bold: true
                            anchors.centerIn: parent

                            background: Rectangle {
                            id: buttonBackground2
                            width: 160
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
                            font.bold: true
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                            onClicked: {
                            appendToConsole("> Starting tensor visualization...");
                            appendToConsole("> Material: " + comboBox1.currentText);
                            appendToConsole("> Dependence: " + comboBox2.currentText);

                            TensorController.run();
                        }
                    }
                }
            }
        }
    }

    Button {
        id: showPanelsButton
        text: "Show Panels"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        visible: !_itemData.visible || !_itemConsole.visible
        font.bold: true

        background: Rectangle {
            color: "#282828"
            radius: 6
            border.color: "#969696"
        }

        contentItem: Text {
            text: showPanelsButton.text
            color: "#CFCECE"
            font.family: inter.name
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        onClicked: {
            _itemData.visible = true;
            _itemConsole.visible = true;
        }
    }
}
