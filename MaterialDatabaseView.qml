import QtQuick 6.5
import QtQuick.Controls 6.5

import QtQuick.Layouts 1.15
import QtQuick.Controls.Basic 6.2
import QtQuick.Controls.Material 2.15

Window {
    id: materialDatabaseView
    width: 1200
    height: 600
    color: "#282828"
    minimumHeight: 600
    minimumWidth: 1200
    title: qsTr("Material Data")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    property int selectedRow: -1
    property int editingColumn: -1

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    ColumnLayout {
        id: mainColumnData
        anchors.fill: parent
        spacing: 25

        TableView {
            id: tableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: dbManager.getModel()

            columnSpacing: 1
            rowSpacing: 1

            // Component.onCompleted: {
            //     var headerRow = tableView.createRow();
            //     headerRow.addItem({ text: "ID", width: 70 });
            //     headerRow.addItem({ text: "Назва", width: 150 });
            //     headerRow.addItem({ text: "Тип", width: 150 });
            //     tableView.insertRow(0, headerRow);
            // }

            delegate: Rectangle {
                implicitWidth: {
                    if (column === 1 || column === 2) return 150;
                    return 60;
                }
                implicitHeight: 40
                color: "transparent"
                border.color: "#3a3a3a"

                TextField {
                    id: textField
                    anchors.fill: parent
                    anchors.margins: 4
                    text: model.display
                    readOnly: !(selectedRow === row && editingColumn === column)

                    background: Rectangle { color: "transparent" }
                    color: "white"

                    onEditingFinished: {
                        if (selectedRow === row && editingColumn === column) {
                            dbManager.updateMaterial(row, column, text)
                        }
                        editingColumn = -1
                    }

                    onPressed: {
                        selectedRow = row
                        editingColumn = column
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}
        }

        Row {
            id: rowButton
            spacing: 20
            topPadding: 10
            bottomPadding: 15
            width: (addButton.width * 3) + 40
            height: 70
            Layout.alignment: Qt.AlignHCenter

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

                onClicked: {
                    dbManager.addMaterial("");
                    selectedRow = dbManager.getModel().rowCount - 1;
                    editingColumn = 0;
                }
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

                enabled: selectedRow >= 0
                onClicked: {
                    dbManager.getModel().submitAll();
                }
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

                enabled: selectedRow >= 0
                onClicked: {
                    dbManager.removeMaterial(selectedRow);
                    selectedRow = -1;
                    editingColumn = -1;
                }
            }
        }
    }
}
