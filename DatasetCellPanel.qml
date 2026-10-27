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
                            }
                        }
                    }
                }
            }
}
