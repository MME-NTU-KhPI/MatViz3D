import QtQuick 2.15
import QtQuick.Controls 2.15
import parameters 1.0


Window {
    id: probabilityAlgorithmView
    width: 534
    height: 522
    color: "#282828"
    property alias _item_b1_pav: _item_b1_pav
    minimumHeight: 522
    minimumWidth: 534
    title: qsTr("Probability Alghorithm Settings")

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    Column {
        id: column_probabilityAlgorithmView
        anchors.fill: parent

        Grid {
            id: grid_probabilityAlgorithmView
            width: column_probabilityAlgorithmView.width
            height: column_probabilityAlgorithmView.height * 0.7
            rows: 3
            columns: 2

            Item {
                id: item1_pav
                width: grid_probabilityAlgorithmView.width * 0.5
                height: grid_probabilityAlgorithmView.height / 3

                Column {
                    id: column1_pav
                    width: column1_pav.childrenRect.width
                    height: column1_pav.childrenRect.height
                    spacing: 15
                    anchors.centerIn: parent

                    Text {
                        id: _text1_pav
                        color: "#969696"
                        text: qsTr("Half-axis a")
                        font.pixelSize: 20
                        font.styleName: "Bold"
                        font.family: inter.name
                    }

                    Rectangle {
                        id: recInput1_pav
                        width: 228
                        height: 26
                        color: "#00000000"
                        border.color: "#969696"
                        border.width: 1
                        radius: 5

                        TextInput {
                            id: textInput1_pav
                            anchors.fill: parent
                            anchors.margins: 5
                            color: "#969696"
                            text: qsTr("1.5")
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignTop
                            horizontalAlignment: Text.AlignLeft
                            onTextChanged: {
                                console.log("Change: ", text);
                                Parameters.setHalfAxisA(parseInt(text, 10));
                            }
                        }
                    }
                }
            }

            Item {
                id: item2_pav
                width: grid_probabilityAlgorithmView.width * 0.5
                height: grid_probabilityAlgorithmView.height / 3

                Column {
                    id: column2_pav
                    width: column2_pav.childrenRect.width
                    height: column2_pav.childrenRect.height
                    spacing: 15
                    anchors.centerIn: parent

                    Text {
                        id: _text2_pav
                        color: "#969696"
                        text: qsTr("Half-axis b")
                        font.pixelSize: 20
                        font.styleName: "Bold"
                        font.family: inter.name
                    }

                    Rectangle {
                        id: recInput2_pav
                        width: 228
                        height: 26
                        color: "#00000000"
                        border.color: "#969696"
                        border.width: 1
                        radius: 5

                        TextInput {
                            id: textInput2_pav
                            anchors.fill: parent
                            anchors.margins: 5
                            color: "#969696"
                            text: qsTr("1.5")
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignTop
                            horizontalAlignment: Text.AlignLeft
                            onTextChanged: {
                                console.log("Change: ", text);
                                Parameters.setHalfAxisB(parseInt(text, 10));
                            }
                        }
                    }
                }
            }

            Item {
                id: item3_pav
                width: grid_probabilityAlgorithmView.width * 0.5
                height: grid_probabilityAlgorithmView.height / 3

                Column {
                    id: column3_pav
                    width: column3_pav.childrenRect.width
                    height: column3_pav.childrenRect.height
                    spacing: 15
                    anchors.centerIn: parent

                    Text {
                        id: _text3_pav
                        color: "#969696"
                        text: qsTr("Half-axis c")
                        font.pixelSize: 20
                        font.styleName: "Bold"
                        font.family: inter.name
                    }

                    Rectangle {
                        id: recInput3_pav
                        width: 228
                        height: 26
                        color: "#00000000"
                        border.color: "#969696"
                        border.width: 1
                        radius: 5

                        TextInput {
                            id: textInput3_pav
                            anchors.fill: parent
                            anchors.margins: 5
                            color: "#969696"
                            text: qsTr("1.5")
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignTop
                            horizontalAlignment: Text.AlignLeft
                            onTextChanged: {
                                console.log("Change: ", text);
                                Parameters.setHalfAxisC(parseInt(text, 10));
                            }
                        }
                    }
                }
            }

            Item {
                id: item4_pav
                width: grid_probabilityAlgorithmView.width * 0.5
                height: grid_probabilityAlgorithmView.height / 3

                Column {
                    id: column4_pav
                    width: column4_pav.childrenRect.width
                    height: column4_pav.childrenRect.height
                    spacing: 15
                    anchors.centerIn: parent

                    Text {
                        id: _text4_pav
                        color: "#969696"
                        text: qsTr("Orintation angle (A)")
                        font.pixelSize: 20
                        font.styleName: "Bold"
                        font.family: inter.name
                    }

                    Rectangle {
                        id: recInput4_pav
                        width: 228
                        height: 26
                        color: "#00000000"
                        border.color: "#969696"
                        border.width: 1
                        radius: 5

                        TextInput {
                            id: textInput4_pav
                            anchors.fill: parent
                            anchors.margins: 5
                            color: "#969696"
                            text: qsTr("0")
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignTop
                            horizontalAlignment: Text.AlignLeft
                            onTextChanged: {
                                console.log("Change: ", text);
                                Parameters.setOrientationAngleA(parseInt(text, 10));
                            }
                        }
                    }
                }
            }

            Item {
                id: item5_pav
                width: grid_probabilityAlgorithmView.width * 0.5
                height: grid_probabilityAlgorithmView.height / 3

                Column {
                    id: column5_pav
                    width: column5_pav.childrenRect.width
                    height: column5_pav.childrenRect.height
                    spacing: 15
                    anchors.centerIn: parent

                    Text {
                        id: _text5_pav
                        color: "#969696"
                        text: qsTr("Orintation angle (B)")
                        font.pixelSize: 20
                        font.styleName: "Bold"
                        font.family: inter.name
                    }

                    Rectangle {
                        id: recInput5_pav
                        width: 228
                        height: 26
                        color: "#00000000"
                        border.color: "#969696"
                        border.width: 1
                        radius: 5

                        TextInput {
                            id: textInput5_pav
                            anchors.fill: parent
                            anchors.margins: 5
                            color: "#969696"
                            text: qsTr("0")
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignTop
                            horizontalAlignment: Text.AlignLeft
                            onTextChanged: {
                                console.log("Change: ", text);
                                Parameters.setOrientationAngleB(parseInt(text, 10));
                            }
                        }
                    }
                }
            }

            Item {
                id: item6_pav
                width: grid_probabilityAlgorithmView.width * 0.5
                height: grid_probabilityAlgorithmView.height / 3

                Column {
                    id: column6_pav
                    width: column6_pav.childrenRect.width
                    height: column6_pav.childrenRect.height
                    spacing: 15
                    anchors.centerIn: parent

                    Text {
                        id: _text6_pav
                        color: "#969696"
                        text: qsTr("Orintation angle (C)")
                        font.pixelSize: 20
                        font.styleName: "Bold"
                        font.family: inter.name
                    }

                    Rectangle {
                        id: recInput6_pav
                        width: 228
                        height: 26
                        color: "#00000000"
                        border.color: "#969696"
                        border.width: 1
                        radius: 5

                        TextInput {
                            id: textInput6_pav
                            anchors.fill: parent
                            anchors.margins: 5
                            color: "#969696"
                            text: qsTr("0")
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignTop
                            horizontalAlignment: Text.AlignLeft
                            onTextChanged: {
                                console.log("Change: ", text);
                                Parameters.setOrientationAngleC(parseInt(text, 10));
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: rectangle_probabilityAlgorithmView
            width: column_probabilityAlgorithmView.width
            height: column_probabilityAlgorithmView.height * 0.05
            color: "#00000000"
            border.color: "#00000000"

            Text {
                id: _text_pav
                color: "#969696"
                text: qsTr("If a sphere is used for construction - all half-axes must be equal")
                font.pixelSize: 13
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                font.styleName: "Bold"
                anchors.centerIn: parent
                font.family: inter.name
            }
        }

        Row {
            id: row_probabilityAlgorithmView
            width: column_probabilityAlgorithmView.width
            height: column_probabilityAlgorithmView.height * 0.25

            Item {
                id: _item_b1_pav
                width: row_probabilityAlgorithmView.width * 0.5
                height: row_probabilityAlgorithmView.height

                Button {
                    id: button_cancel_pav
                    width: 228
                    height: 56
                    text: qsTr("CANCEL")
                    anchors.centerIn: parent
                    background: Rectangle {
                        id: buttonBackground1
                        width: 228
                        height: 56
                        radius: 15
                        color: "#282828"
                        border.color: "#969696"
                        border.width: 1

                        MouseArea {
                            id: hoverArea1
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: buttonBackground1.color = "#3a3a3a"
                            onExited: buttonBackground1.color = "#282828"
                        }
                    }
                    contentItem: Text {
                        text: button_cancel_pav.text
                        font.pixelSize: 20
                        font.family: inter.name
                        color: Qt.rgba(150 / 255, 150 / 255, 150 / 255, 0.5)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.centerIn: parent
                    }
                }
            }


            Item {
                id: _item_b2_pav
                width: row_probabilityAlgorithmView.width * 0.5
                height: row_probabilityAlgorithmView.height

                Button {
                    id: button_apply_pav
                    width: 228
                    height: 56
                    text: qsTr("APPLY")
                    anchors.centerIn: parent
                    background: Rectangle {
                        id: buttonBackground2
                        width: 228
                        height: 56
                        radius: 15
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
                        text: button_apply_pav.text
                        font.pixelSize: 20
                        font.family: inter.name
                        color: Qt.rgba(150 / 255, 150 / 255, 150 / 255, 0.5)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.centerIn: parent
                    }
                }
            }
        }
    }
}
