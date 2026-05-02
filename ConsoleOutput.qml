import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    // Called from outside to append messages
    function appendMessage(type, text) {
        var color;
        switch(type) {
            case "debug":    color = "#969696"; break;
            case "info":     color = "#4fc3f7"; break;
            case "warning":  color = "#ffb74d"; break;
            case "critical": color = "#ef5350"; break;
            case "fatal":    color = "#b71c1c"; break;
            default:         color = "#969696"; break;
        }

        var prefix;
        switch(type) {
            case "debug":    prefix = "[DBG] ";  break;
            case "info":     prefix = "[INF] ";  break;
            case "warning":  prefix = "[WRN] ";  break;
            case "critical": prefix = "[CRT] ";  break;
            case "fatal":    prefix = "[FTL] ";  break;
            default:         prefix = "[LOG] ";  break;
        }

        messagesModel.append({
            "msgType": type,
            "msgColor": color,
            "msgPrefix": prefix,
            "msgText": text
        })

        // Auto-scroll to bottom
        Qt.callLater(() => listView.positionViewAtEnd())
    }

    function clear() {
        messagesModel.clear()
    }

    // Highlight numbers and booleans in a message string → returns RichText
    function highlightValues(text) {
        // Highlight booleans
        text = text.replace(/\b(true|false)\b/g,
            "<font color='#ce93d8'>$1</font>")

        // Highlight numbers (int and float)
        text = text.replace(/\b(\d+\.?\d*)\b/g,
            "<font color='#80cbc4'>$1</font>")

        return text
    }

    Connections {
        target: ConsoleLogger
        function onNewMessage(type, text) {
            root.appendMessage(type, text)
        }
        function onClearRequested() {
                root.clear()
        }
    }

    ListModel {
        id: messagesModel
    }

    ListView {
        id: listView
        anchors.fill: parent
        model: messagesModel
        clip: true
        spacing: 0

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 4
                implicitHeight: 100
                radius: 2
                color: "#555555"
            }
        }

        delegate: Item {
            width: listView.width
            height: rowContent.implicitHeight

            Row {
                id: rowContent
                width: parent.width
                spacing: 4

                Text {
                    id: prefixText
                    text: msgPrefix
                    color: msgColor
                    font.pixelSize: 12
                    font.family: montserrat.name
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    id: messageText
                    width: parent.width - prefixText.width - 4
                    text: root.highlightValues(msgText)
                    textFormat: Text.RichText
                    color: msgColor
                    font.pixelSize: 12
                    font.family: montserrat.name
                    wrapMode: Text.WrapAnywhere
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}