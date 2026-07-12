import QtQuick 2.15
import QtQuick.Controls 2.15
import parameters 1.0

Window {
    id: probabilityAlgorithmView

    // Число полей в текущей схеме
    property int fieldCount: schemaController.currentSchema.length

    // Число строк в сетке: по 2 поля в ряд
    property int gridRows: Math.max(1, Math.ceil(fieldCount / 2))

    width: 534
    height: 140 + gridRows * 110          // шапка/кнопки + строки полей
    minimumWidth: 534
    minimumHeight: 250
    color: "#282828"
    title: qsTr("Algorithm Settings")

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    Column {
        id: column_pav
        anchors.fill: parent

        // ── СЕТКА ПОЛЕЙ (строится из схемы) ──
        Grid {
            id: grid_pav
            width: column_pav.width
            height: probabilityAlgorithmView.gridRows * 110
            columns: 2
            rows: probabilityAlgorithmView.gridRows

            Repeater {
                model: schemaController.currentSchema

                delegate: Item {
                    width: grid_pav.width * 0.5      // половина ширины — 2 колонки
                    height: 110

                    property var f: modelData         // текущее поле схемы

                    Column {
                        spacing: 12
                        anchors.centerIn: parent

                        // Подпись поля — из схемы
                        Text {
                            color: "#969696"
                            text: f.label
                            font.pixelSize: 18
                            font.styleName: "Bold"
                            font.family: inter.name
                        }

                        // Поле ввода — стиль как был
                        Rectangle {
                            width: 228
                            height: 26
                            color: "#00000000"
                            border.color: "#969696"
                            border.width: 1
                            radius: 5

                            TextInput {
                                id: input
                                anchors.fill: parent
                                anchors.margins: 5
                                color: "#969696"
                                text: String(f.defValue)      // значение по умолчанию из схемы
                                font.pixelSize: 15
                                verticalAlignment: Text.AlignTop
                                horizontalAlignment: Text.AlignLeft
                                selectByMouse: true

                                onTextChanged: {
                                    if (text === "") return
                                    var v = parseFloat(text)   // parseFloat, не parseInt
                                    if (!isNaN(v))
                                        schemaController.applyValue(f.key, v)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── ПОДСКАЗКА ──
        Rectangle {
            width: column_pav.width
            height: 30
            color: "#00000000"

            Text {
                anchors.centerIn: parent
                color: "#969696"
                text: qsTr("If a sphere is used for construction - all half-axes must be equal")
                font.pixelSize: 13
                font.styleName: "Bold"
                font.family: inter.name
            }
        }

        // ── КНОПКИ ──
        Row {
            width: column_pav.width
            height: 90

            Item {
                width: parent.width * 0.5
                height: parent.height

                Button {
                    id: button_cancel_pav
                    width: 228
                    height: 56
                    text: qsTr("CANCEL")
                    anchors.centerIn: parent

                    background: Rectangle {
                        id: bgCancel
                        radius: 15
                        color: "#282828"
                        border.color: "#969696"
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: bgCancel.color = "#3a3a3a"
                            onExited: bgCancel.color = "#282828"
                        }
                    }

                    contentItem: Text {
                        text: button_cancel_pav.text
                        font.pixelSize: 20
                        font.family: inter.name
                        color: Qt.rgba(150/255, 150/255, 150/255, 0.5)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: probabilityAlgorithmView.visible = false
                }
            }

            Item {
                width: parent.width * 0.5
                height: parent.height

                Button {
                    id: button_apply_pav
                    width: 228
                    height: 56
                    text: qsTr("APPLY")
                    anchors.centerIn: parent

                    background: Rectangle {
                        id: bgApply
                        radius: 15
                        color: "#282828"
                        border.color: "#969696"
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: bgApply.color = "#3a3a3a"
                            onExited: bgApply.color = "#282828"
                        }
                    }

                    contentItem: Text {
                        text: button_apply_pav.text
                        font.pixelSize: 20
                        font.family: inter.name
                        color: Qt.rgba(150/255, 150/255, 150/255, 0.5)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: probabilityAlgorithmView.visible = false
                }
            }
        }
    }
}
