import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The right-hand panel of the main window's Dataset mode: the parameters of
// the cell selected in the Dataset tab, by section; Prev / Next step through
// the filtered rows. The cell itself is in the 3D view between the panels.
Item {
    id: panel
    clip: true

    property var ctrl: datasetBrowserController
    property string fontSans: "Inter"
    property string fontTitle: "Montserrat"

    readonly property color textColor: "#d9d9d9"
    readonly property color subText: "#969696"
    readonly property color lineColor: "#4a4a4a"
    readonly property color accent: "#22c3a6"
    readonly property color faintText: "#6a6a6a"
    readonly property color cellColor: "#333333"
    readonly property color diagColor: "#3d4d4a"     // the diagonal, as in the material matrix window

    ColumnLayout {
                anchors.fill: parent
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: panel.ctrl.currentTitle.length > 0 ? panel.ctrl.currentTitle : qsTr("Select a cell in the Dataset tab")
                        elide: Text.ElideRight
                        color: panel.textColor
                        font.family: panel.fontTitle
                        font.pixelSize: 14
                        font.bold: true
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                    }
                    ToolButton {
                        text: "◀"
                        enabled: panel.ctrl.currentIndex > 0
                        onClicked: panel.ctrl.prev()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Previous cell (PgUp)")
                    }
                    ToolButton {
                        text: "▶"
                        enabled: panel.ctrl.currentIndex >= 0 && panel.ctrl.currentIndex + 1 < panel.ctrl.shownRows
                        onClicked: panel.ctrl.next()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Next cell (PgDown)")
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: panel.ctrl.message.length > 0
                    text: panel.ctrl.message
                    wrapMode: Text.WordWrap
                    color: "#ffb74d"
                    font.family: panel.fontSans
                    font.pixelSize: 11
                }

                ScrollView {
                    id: detailScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    // Always drawn: a hidden bar made the panel look as if it ended at the fold.
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                    // A new cell starts at the top, on its Properties: the rebuilt
                    // sections otherwise kept the old offset and opened mid-list.
                    Connections {
                        target: panel.ctrl
                        function onCurrentChanged() { Qt.callLater(function() { detailScroll.contentItem.contentY = 0; }); }
                    }

                    Column {
                        id: detailCol
                        width: detailScroll.availableWidth - 8
                        spacing: 10

                        Repeater {
                            model: panel.ctrl.sections
                            delegate: Column {
                                id: sectionBlock
                                required property var modelData
                                width: detailCol.width
                                spacing: 3

                                Text {
                                    text: sectionBlock.modelData.title
                                    color: panel.accent
                                    font.family: panel.fontTitle
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Rectangle {
                                    width: sectionBlock.width
                                    height: 1
                                    color: panel.lineColor
                                }
                                Repeater {
                                    model: sectionBlock.modelData.items
                                    delegate: Row {
                                        id: kv
                                        required property var modelData
                                        width: sectionBlock.width
                                        spacing: 8
                                        Text {
                                            width: Math.round(kv.width * 0.42)
                                            text: kv.modelData.key
                                            color: panel.subText
                                            font.family: panel.fontSans
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            width: kv.width - Math.round(kv.width * 0.42) - kv.spacing
                                            text: kv.modelData.value
                                            color: panel.textColor
                                            font.family: panel.fontSans
                                            font.pixelSize: 11
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }

                                // The stiffness matrix section: a 6 x 6 grid (axis labels, the
                                // diagonal marked, entries that round to zero faint) and the
                                // button that opens it in the material matrix window.
                                Grid {
                                    id: cGrid
                                    readonly property var cells: sectionBlock.modelData.grid || []
                                    readonly property var faint: sectionBlock.modelData.gridFaint || []
                                    readonly property var labels: sectionBlock.modelData.gridLabels || []
                                    readonly property int labelW: 22
                                    readonly property int cellW: Math.floor((sectionBlock.width - labelW - 6 * columnSpacing) / 6)
                                    visible: cells.length === 36
                                    columns: 7
                                    columnSpacing: 2
                                    rowSpacing: 2
                                    topPadding: 4
                                    Repeater {
                                        model: cGrid.visible ? 49 : 0
                                        delegate: Rectangle {
                                            id: gridCell
                                            required property int index
                                            readonly property int r: Math.floor(index / 7) - 1     // -1: the label row
                                            readonly property int c: index % 7 - 1                 // -1: the label column
                                            readonly property bool isLabel: r < 0 || c < 0
                                            width: c < 0 ? cGrid.labelW : cGrid.cellW
                                            height: 20
                                            radius: 3
                                            color: gridCell.isLabel ? "transparent" : gridCell.r === gridCell.c ? panel.diagColor : panel.cellColor
                                            Text {
                                                anchors.fill: parent
                                                anchors.leftMargin: 2
                                                anchors.rightMargin: gridCell.isLabel ? 2 : 4
                                                horizontalAlignment: gridCell.isLabel ? Text.AlignHCenter : Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                                text: gridCell.r < 0 && gridCell.c < 0 ? "" : gridCell.r < 0 ? cGrid.labels[gridCell.c]
                                                    : gridCell.c < 0 ? cGrid.labels[gridCell.r] : cGrid.cells[gridCell.r * 6 + gridCell.c]
                                                color: gridCell.isLabel ? panel.subText
                                                     : cGrid.faint[gridCell.r * 6 + gridCell.c] ? panel.faintText : panel.textColor
                                                font.family: panel.fontSans
                                                font.pixelSize: 11
                                                fontSizeMode: Text.HorizontalFit    // a long entry in a narrow panel shrinks, never spills
                                                minimumPixelSize: 8
                                            }
                                        }
                                    }
                                }
                                Button {
                                    visible: cGrid.visible
                                    flat: true
                                    highlighted: true
                                    text: qsTr("Open as matrix…")
                                    font.family: panel.fontSans
                                    font.pixelSize: 11
                                    topPadding: 2
                                    bottomPadding: 2
                                    leftPadding: 6
                                    rightPadding: 6
                                    onClicked: panel.openMatrix(sectionBlock.modelData, true)
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 500
                                    ToolTip.text: qsTr("C and S as 6 × 6 matrices in Voigt order, with the Voigt / Reuss / Hill averages and the eigenvalues")
                                }
                            }
                        }
                    }
                }
            }

    // The material matrix window, read-only, for the selected cell's matrix; made on
    // first use, and it follows Prev / Next while it is open.
    Loader {
        id: matrixWindow
        active: false
        sourceComponent: Component { MaterialMatrixEditor { } }
    }

    function matrixOf(sections) {
        for (let k = 0; k < sections.length; ++k)
            if (sections[k].grid !== undefined) return sections[k];
        return null;
    }

    function openMatrix(sec, bringUp) {
        if (!sec) return;
        matrixWindow.active = true;
        const heading = ctrl.currentTitle.length > 0 ? ctrl.currentTitle : qsTr("Cell %1").arg(ctrl.currentId);
        matrixWindow.item.openForMatrix(heading, sec.unit, sec.voigtC, sec.voigtS, bringUp);
    }

    Connections {
        target: panel.ctrl
        function onCurrentChanged() {
            const w = matrixWindow.item;
            if (!w || !w.visible || !w.viewOnly) return;
            const sec = panel.matrixOf(panel.ctrl.sections);
            if (sec) panel.openMatrix(sec, false);
            else w.close();                     // the new row has no solved cell
        }
    }
}
