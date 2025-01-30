import QtQuick 2.15
import QtQuick.Controls 2.15


Window {
    id: mainWindow
    width: 1280
    height: 832
    color: "#363636"
    property alias _item6_menuBar: _item6_menuBar
    property alias _item5_menuBar: _item5_menuBar
    property alias _item4_menuBar: _item4_menuBar
    property alias _item3_menuBar: _item3_menuBar
    property alias _item1_menuBar: _item1_menuBar
    minimumHeight: 832
    minimumWidth: 1280
    title: qsTr("MatViz3D")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    FontLoader {
        id: montserrat
        source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf"
    }

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

    Loader {
        id: statisticsLoader
        source: "StatisticsView.qml"
        active: false
    }

    Grid {
        id: grid
        anchors.fill: parent
        rows: 3
        columns: 1

        Rectangle {
            id: menuBar_rec
            width: mb_row.width
            height: mb_row.height
            color: "#282828"

            Row {
                id: mb_row
                width: grid.width
                height: 67
                bottomPadding: 9
                topPadding: 9
                rightPadding: 40
                leftPadding: 40

                Item {
                    id: _item1_menuBar
                    width: 88
                    height: 49

                    Image {
                        id: iconMenu_img
                        width: 48
                        height: 48
                        source: "qrc:/img/iconMenu.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }

                Item {
                    id: _item2_menuBar
                    width: 83
                    height: 49

                    MenuBar {
                        id: fileMenuBar
                        width: 43
                        height: 49
                        background: Rectangle {
                            color: "#00000000"
                            border.color: "#00000000"
                            border.width: 0
                            width: 43
                            height: 49
                        }

                        contentItem: Text {
                            id: menuFileText
                            text: qsTr("File")
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        Menu {
                            id: fileMenu
                            title: qsTr("   ")
                            font.pixelSize: 16
                            font.family: inter.name

                            Action {
                                text: qsTr("Save as image")
                                onTriggered: console.log("Save as image")
                            }
                            Action {
                                text: qsTr("Make screenshot")
                                onTriggered: console.log("Make screenshot")
                            }
                            MenuSeparator { }
                            Action {
                                text: qsTr("Export to wrl")
                                onTriggered: console.log("Export to wrl")
                            }
                            Action {
                                text: qsTr("Export to csv")
                                onTriggered: console.log("Export to csv")
                            }
                            Action {
                                text: qsTr("Save project as HDF5")
                                onTriggered: console.log("Save project as HDF5")
                            }
                            MenuSeparator { }
                            Action {
                                text: qsTr("Estimate stresses")
                                onTriggered: console.log("Estimate stresses")
                            }
                        }
                    }

                    MouseArea {
                        id: hoverMenuFileText
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: menuFileText.font.weight = Font.Bold;
                        onExited: menuFileText.font.weight = Font.Normal;

                        onClicked: {
                            fileMenu.open()
                        }
                    }
                }

                Item {
                    id: _item3_menuBar
                    width: 128
                    height: 49

                    MenuBar {
                        id: windowMenuBar
                        width: 88
                        height: 49
                        background: Rectangle {
                            color: "#00000000"
                            border.color: "#00000000"
                            border.width: 0
                            width: 88
                            height: 49
                        }

                        contentItem: Text {
                            id: menuWindowText
                            text: qsTr("Window")
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        Menu {
                            id: windowMenu
                            title: qsTr("   ")
                            font.pixelSize: 16
                            font.family: montserrat.name

                            MenuItem {
                                text: qsTr("All")
                                checkable: true
                                checked: true
                                onTriggered: {
                                    console.log("All selected: " + checked)
                                }
                            }
                            MenuItem {
                                text: qsTr("Animation")
                                checkable: true
                                onTriggered: {
                                    console.log("Animation selected: " + checked)
                                }
                            }
                            MenuItem {
                                text: qsTr("Console")
                                checkable: true
                                onTriggered: {
                                    console.log("Console selected: " + checked)
                                }
                            }
                            MenuItem {
                                text: qsTr("Data")
                                checkable: true
                                onTriggered: {
                                    console.log("Data selected: " + checked)
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: hoverMenuWindowText
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: menuWindowText.font.weight = Font.Bold;
                        onExited: menuWindowText.font.weight = Font.Normal;

                        onClicked: {
                            windowMenu.open()
                        }
                    }
                }

                Item {
                    id: _item4_menuBar
                    width: 149
                    height: 49

                    Button {
                        id: buttonStatistics
                        x: 0
                        y: 0
                        width: 109
                        height: _item4_menuBar.height
                        text: qsTr("Statistics")

                        background: Rectangle {
                            id: buttonStatisticsBackground
                            width: 109
                            height: 49
                            color: "transparent"
                            border.color: "transparent"
                            border.width: 1

                            MouseArea {
                                id: hoverButtonStatistics
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onEntered: buttonStatisticsText.font.weight = Font.Bold;
                                onExited: buttonStatisticsText.font.weight = Font.Normal;
                            }
                        }

                        contentItem: Text {
                            id: buttonStatisticsText
                            text: buttonStatistics.text
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        onClicked: {
                            statisticsLoader.active = true;
                            statisticsLoader.item.visible = true;
                        }
                    }
                }

                Item {
                    id: _item5_menuBar
                    width: 115
                    height: 49

                    Button {
                        id: buttonAbout
                        x: 0
                        y: 0
                        width: 75
                        height: _item5_menuBar.height
                        text: qsTr("About")

                        background: Rectangle {
                            id: buttonAboutBackground
                            width: 75
                            height: 49
                            color: "transparent"
                            border.color: "transparent"
                            border.width: 1

                            MouseArea {
                                id: hoverButtonAbout
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onEntered: buttonAboutText.font.weight = Font.Bold;
                                onExited: buttonAboutText.font.weight = Font.Normal;
                            }
                        }

                        contentItem: Text {
                            id: buttonAboutText
                            text: buttonAbout.text
                            font.pixelSize: 24
                            font.family: montserrat.name
                            color: "#CFCECE"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        onClicked: {
                            aboutLoader.active = true;
                            aboutLoader.item.visible = true;
                        }
                    }
                }

                Item {
                    id: _item6_menuBar
                    height: 49
                    width: parent.width - (_item1_menuBar.width + _item2_menuBar.width + _item3_menuBar.width + _item4_menuBar.width + _item5_menuBar.width) - 80

                    Text {
                        id: _text_menuBar
                        color: "#00897b"
                        textFormat: Text.RichText
                        text: qsTr("Material<span style='color: #00564D;'>Viz</span>")
                        anchors.fill: parent
                        font.pixelSize: 36
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        font.styleName: "Bold"
                        font.family: inter.name
                    }
                }
            }
        }
    }
}


