import QtQuick 6.5
import QtQuick.Controls 6.5

import QtQuick.Layouts 1.15
import QtQuick.Controls.Basic 6.2
import QtQuick.Controls.Material 2.15

Window {
    id: materialDatabaseView
    // Wider by default so the anisotropy panel has room; the minimum stays
    // sized for the table alone, since the panel starts hidden.
    width: 1660
    height: 600
    color: theme.windowBg
    minimumHeight: 600
    minimumWidth: 1280
    title: qsTr("Material Data")

    Material.theme: materialDatabaseView.darkTheme ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    property int selectedRow: -1
    property int editingColumn: -1

    /// Light/dark switch for this window and its anisotropy panel.
    property bool darkTheme: true

    // One place every colour in this window comes from, so the theme toggle is
    // a single flip rather than a hunt through hardcoded hex. Kept as a plain
    // QtObject rather than a separate file: nothing else needs these.
    QtObject {
        id: theme
        readonly property bool dark: materialDatabaseView.darkTheme
        readonly property color windowBg:   dark ? "#282828" : "#f4f4f4"
        readonly property color panelBg:    dark ? "#282828" : "#ffffff"
        readonly property color headerBg:   dark ? "#303030" : "#e6e6e6"
        readonly property color viewportBg: dark ? "#1e1e1e" : "#fbfbfb"
        readonly property color border:     dark ? "#3a3a3a" : "#c8c8c8"
        readonly property color textStrong: dark ? "#CFCECE" : "#1c1c1c"
        readonly property color textBody:   dark ? "#c6c6c6" : "#333333"
        readonly property color textDim:    dark ? "#9e9e9e" : "#5c5c5c"
        readonly property color textFaint:  dark ? "#6a6a6a" : "#8a8a8a"
        readonly property color buttonBg:   dark ? "#303030" : "#ececec"
        readonly property color buttonHover:dark ? "#3a3a3a" : "#dcdcdc"
        readonly property color buttonEdge: dark ? "#969696" : "#b0b0b0"
        readonly property color accent:     "#4db6ac"
    }

    // Columns hidden while the anisotropy panel is open: everything the
    // database leaves at zero (most of c13..c66 for a table of cubic
    // materials), plus the surrogate key. Recomputed on every edit via
    // dbManager.revision -- a bare Q_INVOKABLE call is not a tracked binding
    // dependency, so the read below is what makes this refresh.
    property var collapsedColumns: {
        dbManager.revision;
        if (!compactColumns) return [];
        var list = dbManager.emptyElasticColumns();
        list.push(0);                       // id
        return list;
    }
    property bool compactColumns: anisoButton.checked

    onCollapsedColumnsChanged: {
        tableView.forceLayout();
        horizontalHeader.forceLayout();
    }

    function columnHidden(c) {
        return collapsedColumns.indexOf(c) !== -1;
    }
    function columnWidth(c) {
        if (columnHidden(c)) return 0;
        return (c === 1 || c === 2) ? 110 : 70;
    }

    FontLoader {
        id: inter
        source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf"
    }

    RowLayout {
        id: mainRowData
        anchors.fill: parent
        spacing: 0

    ColumnLayout {
        id: mainColumnData
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        Rectangle {
                    Layout.fillWidth: true
                    height: 40 // implicitHeight
                    color: theme.headerBg
                    z: 2

                    HorizontalHeaderView {
                        id: horizontalHeader
                        anchors.fill: parent
                        syncView: tableView
                        // Must mirror the table's provider exactly or the header
                        // labels drift out of step with the columns they name.
                        columnWidthProvider: function (c) { return materialDatabaseView.columnWidth(c) }

                        delegate: Rectangle {
                            implicitWidth: materialDatabaseView.columnWidth(column)
                            implicitHeight: 40
                            visible: !materialDatabaseView.columnHidden(column)
                            color: theme.headerBg
                            border.color: theme.border

                            Text {
                                anchors.centerIn: parent
                                text: display
                                color: theme.textStrong
                                font.family: inter.name
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }
                    }
                }

        TableView {
            id: tableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 5

            clip: true

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

            boundsBehavior: Flickable.StopAtBounds

            columnWidthProvider: function (c) { return materialDatabaseView.columnWidth(c) }

            delegate: Rectangle {
                implicitWidth: materialDatabaseView.columnWidth(column)
                implicitHeight: 40
                visible: !materialDatabaseView.columnHidden(column)
                color: "transparent"
                border.color: theme.border

                TextField {
                    id: textField
                    anchors.fill: parent
                    anchors.margins: 4
                    text: model.display
                    readOnly: !(selectedRow === row && editingColumn === column)

                    background: Rectangle { color: "transparent" }
                    color: theme.textStrong

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
            // 3 x 90 px buttons + the 140 px anisotropy toggle + the 44 px
            // theme toggle + 4 x 20 spacing
            width: (addButton.width * 3) + anisoButton.width + themeButton.width + 80
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
                    // Bound, not assigned in onEntered/onExited: an imperative
                    // assignment would survive a theme flip and leave the button
                    // painted in the old palette.
                    color: hoverArea1.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1

                    MouseArea {
                        id: hoverArea1
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: addButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: theme.textStrong
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
                    // Bound, not assigned in onEntered/onExited: an imperative
                    // assignment would survive a theme flip and leave the button
                    // painted in the old palette.
                    color: hoverArea2.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1

                    MouseArea {
                        id: hoverArea2
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: saveButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: theme.textStrong
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
                    // Bound, not assigned in onEntered/onExited: an imperative
                    // assignment would survive a theme flip and leave the button
                    // painted in the old palette.
                    color: hoverArea3.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1

                    MouseArea {
                        id: hoverArea3
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: deleteButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: theme.textStrong
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

            Button {
                id: anisoButton
                width: 140
                height: 45
                text: anisoPanel.visible ? qsTr("Hide anisotropy") : qsTr("Anisotropy ▸")
                checkable: true

                background: Rectangle {
                    id: buttonBackground4
                    width: 140
                    height: 40
                    radius: 10
                    color: anisoButton.checked ? theme.buttonHover : theme.buttonBg
                    border.color: anisoButton.checked ? theme.accent : theme.buttonEdge
                    border.width: 1

                    MouseArea {
                        id: hoverArea4
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: anisoButton.text
                    font.family: inter.name
                    font.pixelSize: 18
                    color: theme.textStrong
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.centerIn: parent
                }
            }

            Button {
                id: themeButton
                width: 44
                height: 45
                text: materialDatabaseView.darkTheme ? "☀" : "☾"

                ToolTip.visible: hovered
                ToolTip.text: materialDatabaseView.darkTheme ? qsTr("Switch to light theme")
                                                             : qsTr("Switch to dark theme")

                background: Rectangle {
                    width: 44
                    height: 40
                    radius: 10
                    color: hoverArea5.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1

                    MouseArea {
                        id: hoverArea5
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: themeButton.text
                    font.family: inter.name
                    font.pixelSize: 20
                    color: theme.textStrong
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.centerIn: parent
                }

                onClicked: materialDatabaseView.darkTheme = !materialDatabaseView.darkTheme
            }
        }
    }

        AnisotropySurfacePanel {
            id: anisoPanel
            // Roomier than the table needs to give up: the surface is the point
            // of opening this panel, and the collapsed columns free the space.
            Layout.preferredWidth: 620
            Layout.minimumWidth: 460
            Layout.fillHeight: true
            visible: anisoButton.checked
            selectedRow: materialDatabaseView.selectedRow

            darkTheme:  materialDatabaseView.darkTheme
            panelBg:    theme.panelBg
            viewportBg: theme.viewportBg
            borderColor: theme.border
            textStrong: theme.textStrong
            textBody:   theme.textBody
            textDim:    theme.textDim
            textFaint:  theme.textFaint
        }
    }
}
