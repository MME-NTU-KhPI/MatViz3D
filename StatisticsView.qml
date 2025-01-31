import QtQuick 2.15
import QtQuick.Controls 2.15


Window {
    id: statisticsView
    width: 640
    height: 480
    color: "#393939"
    title: qsTr("Material Statistics")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    Switch {
        id: _switch
        x: 50
        y: 32
        text: qsTr("Switch")
    }

    Slider {
        id: slider
        x: 108
        y: 154
        value: 0.5
    }

    RangeSlider {
        id: rangeSlider
        x: 43
        y: 218
        width: 221
        height: 60
        second.value: 0.75
        first.value: 0.25
    }

    RadioButton {
        id: radioButton
        x: 44
        y: 88
        width: 105
        height: 41
        text: qsTr("Radio Button")
    }

    ListView {
        id: listView
        x: 364
        y: 172
        width: 160
        height: 80
        model: ListModel {
            ListElement {
                name: "Red"
                colorCode: "red"
            }

            ListElement {
                name: "Green"
                colorCode: "green"
            }

            ListElement {
                name: "Blue"
                colorCode: "blue"
            }

            ListElement {
                name: "White"
                colorCode: "white"
            }
        }
        delegate: Row {
            spacing: 5
            Rectangle {
                width: 100
                height: 20
                color: colorCode
            }

            Text {
                width: 100
                text: name
            }
        }
    }
}
