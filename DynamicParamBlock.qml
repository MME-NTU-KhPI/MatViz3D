import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import parameters 1.0

Column {
    id: root
    width: parent ? parent.width : 224
    spacing: 12
    topPadding: 15

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
                            // Bool fields carry their own inline label so the
                            // switch sits on the same line as its text.
                            visible: f.type !== 2
                            text: f.label + ":"
                            color: "#c6c6c6"
                            font.pixelSize: 18
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        Loader {
                            width: 224
                            sourceComponent: {
                                switch (f.type) {
                                    case 3: return enumDelegate
                                    case 2: return boolDelegate
                                    case 4: return pointsModeDelegate
                                    case 5: return actionDelegate
                                    default: return numberDelegate
                                }
                            }

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
                                    onActivated: {
                                        schemaController.applyValue(f.key, currentText, f.invokeMethod)
                                        // An option such as "Custom (editor)"
                                        // both selects a value and opens the
                                        // tool that produces it.
                                        if (f.action !== "" && currentText === f.actionOnValue)
                                            schemaController.triggerAction(f.action)
                                    }
                                }
                            }

                            // Bool — label and switch on one line
                            Component {
                                id: boolDelegate
                                Row {
                                    width: 224
                                    spacing: 6

                                    Text {
                                        width: 224 - boolSwitch.width - parent.spacing
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: f.label + ":"
                                        color: "#c6c6c6"
                                        font.pixelSize: 18
                                        font.styleName: "Bold"
                                        font.family: inter.name
                                        elide: Text.ElideRight
                                    }

                                    Switch {
                                        id: boolSwitch
                                        anchors.verticalCenter: parent.verticalCenter
                                        checked: f.defValue === true
                                        onCheckedChanged: schemaController.applyValue(f.key, checked, f.invokeMethod)
                                    }
                                }
                            }

                            // Action — a button that hands an id back to QML,
                            // which decides what window to open.
                            Component {
                                id: actionDelegate
                                Button {
                                    id: actionButton
                                    width: 224
                                    height: 30
                                    text: String(f.defValue)
                                    hoverEnabled: true
                                    onClicked: schemaController.triggerAction(f.action)
                                    background: Rectangle {
                                        radius: 11
                                        color: actionButton.hovered ? "#3a3a3a" : "#282828"
                                        border.color: "#969696"
                                    }
                                    contentItem: Text {
                                        text: actionButton.text
                                        color: "#CFCECE"
                                        font.pointSize: 10
                                        font.family: montserrat.name
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            Component {
                                        id: pointsModeDelegate

                                        Column {
                                            width: 224
                                            spacing: 10

                                            // Switch mode. Changing the unit
                                            // must not change the structure:
                                            // set the mode, then rewrite the
                                            // field with the same point count
                                            // expressed in the new unit.
                                            Row {
                                                leftPadding: -2
                                                RadioButton {
                                                    id: sizeRadio
                                                    text: f.enumOptions[0]        // "Size"
                                                    font.pixelSize: 15
                                                    font.family: montserrat.name
                                                    checked: Parameters.pointsMode !== "density"
                                                    onClicked: {
                                                        Parameters.setPointsMode("count")
                                                        pointsInput.setText(Parameters.pointsDisplayValue())
                                                    }
                                                }
                                                RadioButton {
                                                    text: f.enumOptions[1]        // "Concentration"
                                                    font.pixelSize: 15
                                                    font.family: montserrat.name
                                                    checked: Parameters.pointsMode === "density"
                                                    onClicked: {
                                                        Parameters.setPointsMode("density")
                                                        pointsInput.setText(Parameters.pointsDisplayValue())
                                                    }
                                                }
                                            }

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
