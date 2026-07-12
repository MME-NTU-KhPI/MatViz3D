import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: paramsPanel

    visible: schemaController.currentSchema.length > 0

    height: paramsColumn.implicitHeight + 20

    Rectangle {
        id: paramsRect
        color: "#80282828"
        radius: 13
        width: parent.width
        height: parent.height

        ScrollView {

        }

        Column {
            id: paramsColumn
            width: parent.width
            spacing: 0

            // ── Шапка панели (как у Data) ──
            Row {
                id: headerParams_row
                width: paramsColumn.width
                height: 32
                rightPadding: 29
                leftPadding: 29
                bottomPadding: 5
                topPadding: 10

                Item {
                    width: headerParams_row.width * 0.5 - 29
                    height: 17

                    Text {
                        color: "#d9d9d9"
                        text: qsTr("Algorithm parameters")
                        font.pixelSize: 14
                        font.family: montserrat.name
                    }
                }

                Item {
                    width: headerParams_row.width * 0.5 - 29
                    height: 17

                    Image {
                        width: 10
                        height: 10
                        source: "qrc:/img/closeData.png"
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: paramsPanel.visible = false
                        }
                    }
                }
            }

            // ── Динамические поля из схемы ──
            Repeater {
                model: schemaController.currentSchema

                delegate: Item {
                    width: 224
                    height: 62
                    anchors.horizontalCenter: parent.horizontalCenter

                    property var f: modelData

                    Column {
                        anchors.fill: parent
                        topPadding: 8
                        spacing: 6

                        Text {
                            width: 224
                            color: "#c6c6c6"
                            text: f.label + ":"
                            font.pixelSize: 15
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        PlaceholderInput {
                            initialValue: String(f.defValue)
                            placeholderText: String(f.defValue)
                            onTextChanged: (text) => {
                                if (text !== "")
                                    schemaController.applyValue(f.key, parseFloat(text))
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: 12 }
        }
    }
}
