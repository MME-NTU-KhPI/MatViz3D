import QtQuick 2.15
import QtQuick.Controls 2.15


Window {
    id: mainWindow
    width: 640
    height: 480
    title: qsTr("MatViz3D")

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

    Button {
        id: button1
        x: 77
        y: 179
        width: 186
        height: 123
        text: qsTr("About")

        onClicked: {
            aboutLoader.active = true;
            aboutLoader.item.visible = true;
        }
    }

    Button {
        id: button2
        x: 339
        y: 179
        width: 186
        height: 123
        text: qsTr("Algorithm")

        onClicked: {
            probabilityAlgorithmLoader.active = true;
            probabilityAlgorithmLoader.item.visible = true;
        }
    }
}
