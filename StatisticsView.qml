import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Shapes

Window {
    id: statisticsView

    Material.theme: chartTheme.dark ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    width: 900
    height: 620
    minimumWidth: 700
    minimumHeight: 500
    color: chartTheme.windowBackground
    title: qsTr("Statistics")

    property var ctrl: statisticsController

    readonly property int xTicks: 8
    readonly property int yTicks: 6

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    ChartTheme { id: chartTheme }

    function fmt(v) {
        if (Math.abs(v) >= 10000 || (Math.abs(v) < 0.001 && v !== 0))
            return v.toExponential(2)
        return parseFloat(v.toPrecision(4)).toString()
    }

    Rectangle {
        id: controlBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 60
        color: "#00000000"

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 25
            spacing: 20

            RadioButton {
                id: radio3D
                text: qsTr("3D")
                checked: true
                font.pixelSize: 15
                font.family: montserrat.name
                onClicked: {
                    ctrl.setMode("3D")
                    propertyBox.currentIndex = -1
                }

                contentItem: Text {
                    text: radio3D.text
                    color: chartTheme.controlText
                    font: radio3D.font
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: radio3D.indicator.width + radio3D.spacing
                }
            }

            RadioButton {
                id: radio2D
                text: qsTr("2D")
                font.pixelSize: 15
                font.family: montserrat.name
                onClicked: {
                    ctrl.setMode("2D")
                    propertyBox.currentIndex = -1
                }

                contentItem: Text {
                    text: radio2D.text
                    color: chartTheme.controlText
                    font: radio2D.font
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: radio2D.indicator.width + radio2D.spacing
                }
            }

            ComboBox {
                id: propertyBox
                width: 220
                height: 30
                anchors.verticalCenter: parent.verticalCenter
                model: ctrl.availableProperties        // data-driven список
                currentIndex: -1
                displayText: currentIndex === -1 ? "-----" : currentText
                leftPadding: 10
                font.pointSize: 10
                font.family: montserrat.name

                background: Rectangle {
                    color: chartTheme.controlBackground
                    radius: 11
                    border.color: chartTheme.controlBorder
                }

                contentItem: Text {
                    text: propertyBox.displayText
                    color: chartTheme.controlText
                    font: propertyBox.font
                    leftPadding: 12
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                delegate: ItemDelegate {
                    width: propertyBox.width
                    highlighted: propertyBox.highlightedIndex === index

                    contentItem: Text {
                        text: modelData
                        color: chartTheme.controlText
                        font: propertyBox.font
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: highlighted ? Qt.lighter(chartTheme.controlBackground, 1.4) : chartTheme.controlBackground
                    }
                }

                onActivated: ctrl.selectProperty(currentText)
            }
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 25
            spacing: 12

            Button {
                id: saveBtn
                width: 130
                height: 32
                text: qsTr("SAVE IMAGE")
                enabled: ctrl.hasData

                background: Rectangle {
                    radius: 10
                    border.color: chartTheme.controlBorder
                    border.width: 1

                    color: saveBtn.hovered
                           ? Qt.lighter(chartTheme.controlBackground, 1.4)
                           : chartTheme.controlBackground

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }

                contentItem: Text {
                    text: saveBtn.text
                    color: chartTheme.controlText
                    font.pixelSize: 13
                    font.family: inter.name
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: chartArea.grabToImage(function(result) {
                    result.saveToFile("histogram.png")
                    console.log("Chart saved to histogram.png")
                })
            }

            Button {
                id: csvBtn
                width: 130
                height: 32
                text: qsTr("EXPORT CSV")

                background: Rectangle {
                    radius: 10
                    border.color: chartTheme.controlBorder
                    border.width: 1

                    color: csvBtn.hovered
                           ? Qt.lighter(chartTheme.controlBackground, 1.4)
                           : chartTheme.controlBackground

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }

                contentItem: Text {
                    text: csvBtn.text
                    color: chartTheme.controlText
                    font.pixelSize: 13
                    font.family: inter.name
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: ctrl.exportCSV("grain_statistics.csv")
            }

            Switch {
                id: themeSwitch
                checked: true
                text: checked ? qsTr("Dark") : qsTr("Light")
                font.pixelSize: 13
                font.family: montserrat.name
                anchors.verticalCenter: parent.verticalCenter

                onCheckedChanged: {
                    chartTheme.dark = checked
                    console.log("Switch:", checked, "| theme.dark =", chartTheme.dark)
                    console.log("plotBackground =", chartTheme.plotBackground)
                }

                contentItem: Text {
                    text: themeSwitch.text
                    color: chartTheme.controlText
                    font: themeSwitch.font
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: themeSwitch.indicator.width + themeSwitch.spacing
                }
            }
        }
    }

    Text {
        id: chartTitle
        anchors { top: controlBar.bottom; horizontalCenter: parent.horizontalCenter }
        text: ctrl.chartTitle
        color: chartTheme.chartTitle
        font.pixelSize: 18
        font.styleName: "Bold"
        font.family: inter.name
    }

    Item {
        id: chartArea
        anchors {
            top: chartTitle.bottom;    topMargin: 15
            left: parent.left;         leftMargin: 25
            right: parent.right;       rightMargin: 25
            bottom: parent.bottom;     bottomMargin: 25
        }

        readonly property int marginLeft:   70
        readonly property int marginBottom: 55

        Text {
            text: qsTr("Frequency")
            color: chartTheme.axisTitle
            font.pixelSize: 14
            font.family: inter.name
            rotation: -90
            anchors.verticalCenter: plotArea.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: -18
        }

        Text {
            text: qsTr("Value")
            color: chartTheme.axisTitle
            font.pixelSize: 14
            font.family: inter.name
            anchors.horizontalCenter: plotArea.horizontalCenter
            anchors.bottom: parent.bottom
        }

        Item {
            id: plotArea
            anchors {
                left: parent.left;   leftMargin: chartArea.marginLeft
                right: parent.right
                top: parent.top
                bottom: parent.bottom; bottomMargin: chartArea.marginBottom
            }

            Rectangle {
                anchors.fill: parent
                color: chartTheme.plotBackground
                border.color: chartTheme.plotBorder
                border.width: 1
            }

            Repeater {
                model: statisticsView.xTicks + 1

                Item {
                    property real frac: index / statisticsView.xTicks
                    property real val:  ctrl.axisXMin + frac * (ctrl.axisXMax - ctrl.axisXMin)

                    x: frac * plotArea.width
                    y: 0
                    width: 1
                    height: plotArea.height

                    Rectangle {
                        width: 1
                        height: plotArea.height
                        color: chartTheme.gridLine
                    }

                    Rectangle {
                        y: plotArea.height
                        width: 1
                        height: 6
                        color: chartTheme.tickMark
                    }

                    Text {
                        y: plotArea.height + 10
                        x: -width / 2
                        text: statisticsView.fmt(parent.val)
                        color: chartTheme.axisLabel
                        font.pixelSize: 11
                        font.family: montserrat.name
                    }
                }
            }

            Repeater {
                model: statisticsView.yTicks + 1

                Item {
                    property real frac: index / statisticsView.yTicks
                    property real val:  frac * ctrl.axisYMax

                    x: 0
                    y: plotArea.height - frac * plotArea.height
                    width: plotArea.width
                    height: 1

                    Rectangle {
                        width: plotArea.width
                        height: 1
                        color: chartTheme.gridLine
                    }

                    Rectangle {
                        x: -6
                        width: 6
                        height: 1
                        color: chartTheme.tickMark
                    }

                    Text {
                        x: -width - 12
                        y: -height / 2
                        text: Math.round(parent.val)
                        color: chartTheme.axisLabel
                        font.pixelSize: 11
                        font.family: montserrat.name
                    }
                }
            }

            Shape {
                id: histShape
                anchors.fill: parent
                antialiasing: true
                visible: ctrl.hasData

                function buildPath(pts, w, h, xMin, xMax, yMax) {
                    var out = []
                    if (!pts || pts.length === 0) return out

                    var rx = xMax - xMin
                    if (rx <= 0 || yMax <= 0 || w <= 0 || h <= 0) return out

                    var x0 = (pts[0].x - xMin) / rx * w
                    out.push(Qt.point(x0, h))

                    for (var i = 0; i < pts.length; ++i) {
                        var px = (pts[i].x - xMin) / rx * w
                        var py = h - (pts[i].y / yMax) * h
                        out.push(Qt.point(px, py))
                    }

                    var xN = (pts[pts.length - 1].x - xMin) / rx * w
                    out.push(Qt.point(xN, h))

                    return out
                }

                property var pathPoints: buildPath(ctrl.histogramPoints,
                                                   plotArea.width,
                                                   plotArea.height,
                                                   ctrl.axisXMin,
                                                   ctrl.axisXMax,
                                                   ctrl.axisYMax)

                ShapePath {
                    strokeColor: chartTheme.seriesStroke
                    strokeWidth: 2
                    fillColor:   chartTheme.seriesFill
                    joinStyle:   ShapePath.RoundJoin

                    PathPolyline { path: histShape.pathPoints }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: !ctrl.hasData
                text: qsTr("Select a grain property to display")
                color: chartTheme.placeholder
                font.pixelSize: 16
                font.family: inter.name
            }
        }
    }
}
