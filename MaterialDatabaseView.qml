import QtQuick 2.15
import QtQuick.Controls 6.2
import QtQuick.Layouts 1.15
import QtQuick.Controls.Basic 6.2
import QtQuick.Controls.Material 2.15

Window {
    id: materialDatabaseView
    width: 800
    height: 600
    color: "#282828"
    minimumHeight: 600
    minimumWidth: 800
    title: qsTr("Material Data")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    Column {
        id: mainColumnData
        anchors.fill: parent
        // spacing: 20

        Row {
            id: rowButton
            spacing: 20
            topPadding: 10
            x: (materialDatabaseView.width / 2) - (rowButton.width / 2)
            width: rowButton.childrenRect.width

            Button {
                id: addButton
                width: 90
                height: 45
                text: qsTr("Add")

                background: Rectangle {
                    id: buttonBackground1
                    width: 90
                    height: 40
                    radius: 10
                    color: "#303030"
                    border.color: "#969696"
                    border.width: 1

                    MouseArea {
                        id: hoverArea1
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: buttonBackground1.color = "#3a3a3a"
                        onExited: buttonBackground1.color = "#303030"
                    }
                }
                contentItem: Text {
                    text: addButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: "#CFCECE"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.centerIn: parent
                }

                // onClicked: {
                //     dbManager.addUser("", 0);
                //     selectedRow = dbManager.getModel().rowCount - 1;
                //     editingColumn = 0;
                // }
            }

            Button {
                id: saveButton
                width: 90
                height: 45
                text: qsTr("Save")

                background: Rectangle {
                    id: buttonBackground2
                    width: 90
                    height: 40
                    radius: 10
                    color: "#303030"
                    border.color: "#969696"
                    border.width: 1

                    MouseArea {
                        id: hoverArea2
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: buttonBackground2.color = "#3a3a3a"
                        onExited: buttonBackground2.color = "#303030"
                    }
                }
                contentItem: Text {
                    text: saveButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: "#CFCECE"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.centerIn: parent
                }

                // enabled: selectedRow >= 0
                // onClicked: {
                //     dbManager.getModel().submitAll();
                // }
            }

            Button {
                id: deleteButton
                width: 90
                height: 45
                text: qsTr("Delete")

                background: Rectangle {
                    id: buttonBackground3
                    width: 90
                    height: 40
                    radius: 10
                    color: "#303030"
                    border.color: "#969696"
                    border.width: 1

                    MouseArea {
                        id: hoverArea3
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: buttonBackground3.color = "#3a3a3a"
                        onExited: buttonBackground3.color = "#303030"
                    }
                }
                contentItem: Text {
                    text: deleteButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: "#CFCECE"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.centerIn: parent
                }

                // enabled: selectedRow >= 0
                // onClicked: {
                //     dbManager.removeUser(selectedRow);
                //     selectedRow = -1;
                //     editingColumn = -1;
                // }
            }
        }

        Row {
            id: headerDataRow
            width: parent.width
            height: 10

        }

        // TableView {
        //     id: tableView
        //     Layout.fillWidth: true
        //     Layout.fillHeight: true
        //     model: dbManager.getModel()

        //     columnSpacing: 1
        //     rowSpacing: 1

        //     delegate: Rectangle {
        //         implicitWidth: 150
        //         implicitHeight: 50
        //         color: "transparent"  // Прозорий фон або можна "#282828"
        //         border.color: "#3a3a3a"

        //         TextField {
        //             id: textField
        //             anchors.fill: parent
        //             anchors.margins: 5
        //             text: model.display
        //             readOnly: !(selectedRow === row && editingColumn === column)

        //             background: Rectangle { color: "transparent" } // Фон текстового поля прозорий
        //             color: "white"  // Білий текст для кращої видимості в темній темі

        //             onEditingFinished: {
        //                 if (selectedRow === row && editingColumn === column) {
        //                     dbManager.updateUser(row, column, text)
        //                 }
        //                 editingColumn = -1
        //             }

        //             onPressed: {
        //                 selectedRow = row
        //                 editingColumn = column
        //             }
        //         }
        //     }

        //     ScrollBar.vertical: ScrollBar {}
        // }
    }
}
