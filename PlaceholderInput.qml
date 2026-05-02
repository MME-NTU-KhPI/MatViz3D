import QtQuick 2.15

Item {
    id: root
    width: 224
    height: 28

    property string placeholderText: "0"
    property string value: ""
    property alias font: textInput.font

    property string initialValue: ""

    Component.onCompleted: {
        if (initialValue !== "") {
            textInput.text = initialValue
        }
    }

    signal textChanged(string text)

    Rectangle {
        anchors.fill: parent
        color: "#282828"
        border.color: textInput.activeFocus ? "#00897b" : "#969696"
        border.width: 1
        radius: 11

        Text {
            id: placeholder
            anchors.fill: parent
            leftPadding: 10
            verticalAlignment: Text.AlignVCenter
            text: root.placeholderText
            font: textInput.font
            color: "#4a4a4a"
            visible: textInput.text === "" && !textInput.activeFocus
        }

        TextInput {
            id: textInput
            anchors.fill: parent
            anchors.margins: 5
            leftPadding: 10
            rightPadding: 10
            verticalAlignment: Text.AlignVCenter
            color: "#969696"
            font.pixelSize: 12
            font.family: montserrat.name
            font.bold: true
            selectByMouse: true

            onActiveFocusChanged: {
                if (activeFocus) {
                    selectAll()
                }
            }

            onTextChanged: root.textChanged(text)
        }
    }
}