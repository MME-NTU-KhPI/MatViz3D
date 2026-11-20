import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

// The "Dataset" tab of the main window's Data panel: the rows of a dataset
// folder written by tools/dataset/make_dataset.py (manifest.csv + cells/*.hdf5),
// filtered; the selected cell's parameters are in DatasetCellPanel on the right
// and the cell itself in the 3D view between them.
Item {
    id: panel
    clip: true

    property var ctrl: datasetBrowserController
    property string fontSans: "Inter"
    property string fontTitle: "Montserrat"

    readonly property color textColor: "#d9d9d9"
    readonly property color subText: "#969696"
    readonly property color fieldBg: "#282828"
    readonly property color lineColor: "#4a4a4a"
    readonly property color accent: "#22c3a6"

    function statusColor(s) {
        if (s === "done") return "#81c784";
        if (s === "unprintable" || s === "failed" || s === "unresolved" || s === "out_of_band"
            || s === "no_load_path") return "#e57373";
        if (s === "deferred" || s === "pending" || s === "retry") return "#ffb74d";
        return panel.subText;
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Open a dataset folder (manifest.csv + cells/)")
        onAccepted: panel.ctrl.openUrl(selectedFolder)
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open a cell file or a manifest")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("HDF5 cell (*.hdf5 *.h5)"), qsTr("Manifest (manifest.csv)"), qsTr("All files (*)")]
        onAccepted: panel.ctrl.openUrl(selectedFile)
    }

    Shortcut { enabled: panel.visible; sequence: "PgDown"; onActivated: panel.ctrl.next() }
    Shortcut { enabled: panel.visible; sequence: "PgUp";   onActivated: panel.ctrl.prev() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // ── Source ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Button {
                text: qsTr("Open folder…")
                Layout.preferredHeight: 30
                topInset: 0
                bottomInset: 0
                leftPadding: 10
                rightPadding: 10
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: folderDialog.open()
            }
            Button {
                text: qsTr("File…")
                Layout.preferredHeight: 30
                topInset: 0
                bottomInset: 0
                leftPadding: 10
                rightPadding: 10
                flat: true
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: fileDialog.open()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Reload")
                Layout.preferredHeight: 30
                topInset: 0
                bottomInset: 0
                leftPadding: 10
                rightPadding: 10
                flat: true
                enabled: panel.ctrl.isOpen
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: panel.ctrl.reload()
            }
        }
        Text {
            Layout.fillWidth: true
            text: panel.ctrl.isOpen ? panel.ctrl.folder.split(/[\\/]/).slice(-2).join("/")
                                      + "  ·  " + panel.ctrl.shownRows + " / " + panel.ctrl.totalRows + qsTr(" rows")
                                    : qsTr("Open a dataset folder: the one with manifest.csv and cells/")
            elide: Text.ElideMiddle
            color: panel.subText
            font.family: panel.fontSans
            font.pixelSize: 11
        }

        // ── Filters ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ComboBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                topInset: 0
                bottomInset: 0
                font.pixelSize: 12
                model: panel.ctrl.groups
                currentIndex: Math.max(0, panel.ctrl.groups.indexOf(panel.ctrl.groupFilter))
                onActivated: panel.ctrl.groupFilter = currentText
            }
            ComboBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                topInset: 0
                bottomInset: 0
                font.pixelSize: 12
                model: panel.ctrl.statuses
                currentIndex: Math.max(0, panel.ctrl.statuses.indexOf(panel.ctrl.statusFilter))
                onActivated: panel.ctrl.statusFilter = currentText
            }
        }
        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            topInset: 0
            bottomInset: 0
            placeholderText: qsTr("Search by id or family")
            font.family: panel.fontSans
            font.pixelSize: 12
            onTextEdited: panel.ctrl.search = text
            // The Selector tab clears the search when it has to show a hidden row.
            Connections {
                target: panel.ctrl
                function onFilterChanged() { if (searchField.text !== panel.ctrl.search) searchField.text = panel.ctrl.search; }
            }
        }

        // ── Rows ──
        Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#33000000"
                radius: 6

                ListView {
                    id: rowList
                    anchors.fill: parent
                    anchors.margins: 2
                    clip: true
                    focus: true
                    model: panel.ctrl.rows
                    currentIndex: panel.ctrl.currentIndex
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }
                    onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                    Keys.onDownPressed: panel.ctrl.next()
                    Keys.onUpPressed: panel.ctrl.prev()

                    delegate: ItemDelegate {
                        id: rowDelegate
                        required property int index
                        required property string rowId
                        required property string group
                        required property string family
                        required property string status
                        required property real density
                        required property real cellMm
                        required property int grid
                        required property bool hasFile
                        width: ListView.view.width
                        height: 40
                        highlighted: index === panel.ctrl.currentIndex
                        onClicked: { rowList.forceActiveFocus(); panel.ctrl.select(index); }

                        contentItem: RowLayout {
                            spacing: 8
                            Text {
                                text: rowDelegate.rowId
                                font.family: "Menlo"
                                font.pixelSize: 11
                                color: rowDelegate.hasFile ? panel.textColor : panel.subText
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    Layout.fillWidth: true
                                    text: rowDelegate.group === rowDelegate.family ? rowDelegate.family
                                                                                   : rowDelegate.group + " · " + rowDelegate.family
                                    elide: Text.ElideRight
                                    color: panel.textColor
                                    font.family: panel.fontSans
                                    font.pixelSize: 11
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "ρ " + rowDelegate.density.toFixed(2) + " · " + rowDelegate.cellMm.toFixed(1) + " mm"
                                          + (rowDelegate.grid > 0 ? " · " + rowDelegate.grid + "³" : "")
                                    elide: Text.ElideRight
                                    color: panel.subText
                                    font.family: panel.fontSans
                                    font.pixelSize: 10
                                }
                            }
                            Text {
                                text: rowDelegate.status
                                color: panel.statusColor(rowDelegate.status)
                                font.family: panel.fontSans
                                font.pixelSize: 10
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: rowList.count === 0
                        width: parent.width - 30
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: panel.ctrl.isOpen ? qsTr("No rows match the filters") : qsTr("No dataset opened")
                        color: panel.subText
                        font.family: panel.fontSans
                        font.pixelSize: 12
                    }
                }
            }
    }
}
