import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import parameters 1.0

Column {
    id: root
    width: parent ? parent.width : 224
    spacing: 12
    topPadding: 15

    // Показываем блок только когда схема непустая
    visible: schemaController.mainSchema.length > 0

    property var schemaModel: schemaController.mainSchema

    Repeater {
        model: root.schemaModel

        delegate: Item {
                    width: 224
                    height: col.implicitHeight
                    anchors.horizontalCenter: parent.horizontalCenter

                    property var f: modelData

                    Column {
                        id: col
                        width: parent.width
                        spacing: 6

                        Text {
                            text: f.label + ":"
                            color: "#c6c6c6"
                            font.pixelSize: 18
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        // Делегаты прямо здесь — f виден
                        Loader {
                            width: 224
                            sourceComponent: {
                                switch (f.type) {
                                    case 3: return enumDelegate
                                    case 2: return boolDelegate
                                    case 4: return pointsModeDelegate
                                    default: return numberDelegate
                                }
                            }

                            // Число (Int + Double)
                            Component {
                                id: numberDelegate
                                PlaceholderInput {
                                    initialValue: String(f.defValue)
                                    placeholderText: String(f.defValue)
                                    onTextChanged: (text) => {
                                        if (text !== "") {
                                            var val = (f.type === 0) ? parseInt(text, 10) : parseFloat(text)
                                            schemaController.applyValue(f.key, val, f.invokeMethod)
                                        }
                                    }
                                }
                            }

                            // Enum
                            Component {
                                id: enumDelegate
                                ComboBox {
                                    width: 224
                                    height: 28
                                    model: f.enumOptions
                                    currentIndex: Math.max(0, f.enumOptions.indexOf(String(f.defValue)))
                                    leftPadding: 10
                                    font.pointSize: 10
                                    font.family: montserrat.name
                                    background: Rectangle {
                                        color: "#282828"; radius: 11; border.color: "#969696"
                                    }
                                    onActivated: schemaController.applyValue(f.key, currentText, f.invokeMethod)
                                }
                            }

                            // Bool
                            Component {
                                id: boolDelegate
                                Switch {
                                    checked: f.defValue === true
                                    onCheckedChanged: schemaController.applyValue(f.key, checked, f.invokeMethod)
                                }
                            }

                            Component {
                                        id: pointsModeDelegate

                                        Column {
                                            width: 224
                                            spacing: 10

                                            // Переключатель режима
                                            Row {
                                                leftPadding: -2
                                                RadioButton {
                                                    id: sizeRadio
                                                    text: f.enumOptions[0]        // "Size"
                                                    font.pixelSize: 15
                                                    font.family: montserrat.name
                                                    checked: true
                                                    onClicked: {
                                                        Parameters.setPointsMode("count")
                                                        Parameters.processPointInput(pointsInput.text)
                                                    }
                                                }
                                                RadioButton {
                                                    text: f.enumOptions[1]        // "Concentration"
                                                    font.pixelSize: 15
                                                    font.family: montserrat.name
                                                    onClicked: {
                                                        Parameters.setPointsMode("density")
                                                        Parameters.processPointInput(pointsInput.text)
                                                    }
                                                }
                                            }

                                            // Поле числа
                                            PlaceholderInput {
                                                id: pointsInput
                                                initialValue: String(f.defValue)
                                                placeholderText: String(f.defValue)
                                                onTextChanged: (text) => {
                                                    if (text !== "")
                                                        Parameters.processPointInput(text)
                                                }
                                            }
                                        }
                                    }
                        }
                    }
                }
    }
}
