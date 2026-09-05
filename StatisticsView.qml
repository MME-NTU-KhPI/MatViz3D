import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Window {
    id: statisticsView

    Material.theme: chartTheme.dark ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    // Wide enough for the whole control bar: at 900 the STATS button and the
    // theme switch fell off the right edge.
    width: 1120
    height: 640
    minimumWidth: 1040
    minimumHeight: 500
    color: chartTheme.windowBackground
    title: qsTr("Statistics")

    property var ctrl: statisticsController
    property bool showSummary: true

    readonly property int xTicks: 8
    readonly property int yTicks: 6

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    ChartTheme { id: chartTheme }

    function fmt(v) {
        if (Math.abs(v) < 1e-9)
            return "0"
        if (Math.abs(v) >= 10000 || Math.abs(v) < 0.001)
            return v.toExponential(2)
        return parseFloat(v.toPrecision(4)).toString()
    }

    // Land on a populated chart instead of the "select a property" placeholder:
    // pick the first entry of the property list whenever the window is shown or
    // the 2D/3D mode changes the list underneath it.
    function selectFirstProperty() {
        if (!ctrl.availableProperties || ctrl.availableProperties.length === 0)
            return
        propertyBox.currentIndex = 0
        ctrl.selectProperty(propertyBox.textAt(0))
    }

    function exportBaseName() {
        var prop = ctrl.axisXLabel.length > 0 ? ctrl.axisXLabel : "histogram"
        var prefix = (ctrl.mode === "Deformed") ? ("deformed_" + ctrl.deformStatMode) : ctrl.mode
        return (prefix + "_" + prop).replace(/[^a-zA-Z0-9_-]/g, "_").toLowerCase()
    }

    Component.onCompleted: selectFirstProperty()
    onVisibleChanged: if (visible) selectFirstProperty()

    FileDialog {
        id: pngDialog
        title: qsTr("Save histogram as PNG")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            var path = ctrl.toLocalFile(selectedFile)
            chartArea.grabToImage(function(result) {
                if (!result.saveToFile(path))
                    console.warn("StatisticsView: failed to save PNG to", path)
            }, Qt.size(chartArea.width * 2, chartArea.height * 2))
        }
    }

    FileDialog {
        id: svgDialog
        title: qsTr("Save histogram as SVG")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("SVG image (*.svg)")]
        defaultSuffix: "svg"
        onAccepted: ctrl.exportSvg(selectedFile, chartTheme.dark, statisticsView.showSummary)
    }

    FileDialog {
        id: csvDialog
        title: qsTr("Export statistics to CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV Files (*.csv)")]
        defaultSuffix: "csv"
        onAccepted: ctrl.exportCSV(ctrl.toLocalFile(selectedFile))
    }

    Rectangle {
        id: controlBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 60
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 25
            anchors.rightMargin: 25
            // Tight: the bar's items cannot shrink below their implicit width,
            // so any excess overflows off the right edge instead of compressing.
            spacing: 10

            RadioButton {
                id: radio3D
                text: qsTr("3D")
                checked: ctrl.mode === "3D"
                Layout.alignment: Qt.AlignVCenter
                font.pixelSize: 15
                font.family: montserrat.name
                onClicked: { ctrl.setMode("3D"); statisticsView.selectFirstProperty() }
            }

            RadioButton {
                id: radio2D
                text: qsTr("2D")
                checked: ctrl.mode === "2D"
                Layout.alignment: Qt.AlignVCenter
                font.pixelSize: 15
                font.family: montserrat.name
                onClicked: { ctrl.setMode("2D"); statisticsView.selectFirstProperty() }
            }

            RadioButton {
                id: radioDeformed
                text: qsTr("Deformed")
                checked: ctrl.mode === "Deformed"
                visible: ctrl.hasDeformedData
                Layout.alignment: Qt.AlignVCenter
                font.pixelSize: 15
                font.family: montserrat.name
                onClicked: { ctrl.setMode("Deformed"); statisticsView.selectFirstProperty() }
            }

            Row {
                visible: ctrl.mode === "Deformed"
                spacing: 2
                Layout.alignment: Qt.AlignVCenter

                RadioButton {
                    text: qsTr("Full")
                    checked: ctrl.deformStatMode === "FullVolume"
                    font.pixelSize: 12
                    font.family: montserrat.name
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Full Volume: voxel-level field distribution")
                    onClicked: { ctrl.setDeformStatMode("FullVolume"); statisticsView.selectFirstProperty() }
                }
                RadioButton {
                    text: qsTr("Grain")
                    checked: ctrl.deformStatMode === "PerGrain"
                    font.pixelSize: 12
                    font.family: montserrat.name
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Per-Grain Mean: grain-averaged distribution")
                    onClicked: { ctrl.setDeformStatMode("PerGrain"); statisticsView.selectFirstProperty() }
                }
            }

            ComboBox {
                id: propertyBox
                Layout.preferredWidth: ctrl.mode === "Deformed" ? 255 : 220
                Layout.alignment: Qt.AlignVCenter
                model: ctrl.availableProperties
                currentIndex: 0
                displayText: currentIndex === -1 ? "-----" : currentText
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
                popup: Popup {
                    y: propertyBox.height + 4
                    width: Math.max(propertyBox.width, 250)
                    implicitHeight: contentItem.implicitHeight
                    padding: 4
                    contentItem: ListView {
                        clip: true
                        implicitHeight: Math.min(contentHeight, 350)
                        model: propertyBox.popup.visible ? propertyBox.delegateModel : null
                        currentIndex: propertyBox.highlightedIndex
                        ScrollIndicator.vertical: ScrollIndicator { }
                    }
                    background: Rectangle {
                        color: chartTheme.controlBackground
                        radius: 8
                        border.color: chartTheme.controlBorder
                        border.width: 1
                    }
                }
                onActivated: ctrl.selectProperty(currentText)
            }

            Text {
                text: qsTr("Bins:")
                Layout.alignment: Qt.AlignVCenter
                color: chartTheme.controlText
                font.pixelSize: 13
                font.family: montserrat.name
                visible: ctrl.hasData
            }

            Slider {
                id: binSlider
                Layout.preferredWidth: 130
                Layout.alignment: Qt.AlignVCenter
                from: 5; to: 60; stepSize: 1; value: 20
                visible: ctrl.hasData
                onMoved: ctrl.setBinCount(Math.round(value))
            }

            Text {
                text: Math.round(binSlider.value)
                Layout.preferredWidth: 24
                Layout.alignment: Qt.AlignVCenter
                visible: ctrl.hasData
                color: chartTheme.axisTitle
                font.pixelSize: 13
                font.bold: true
                font.family: montserrat.name
            }

            Item { Layout.fillWidth: true }

            Button {
                id: saveBtn
                Layout.preferredWidth: 74
                Layout.preferredHeight: 32
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("PNG")
                enabled: ctrl.hasData
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Save the chart as a raster image (2× resolution)")

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
                    text: saveBtn.text; color: chartTheme.controlText
                    font.pixelSize: 12; font.family: inter.name
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    pngDialog.selectedFile = pngDialog.currentFolder + "/"
                                             + statisticsView.exportBaseName() + ".png"
                    pngDialog.open()
                }
            }

            Button {
                id: svgBtn
                Layout.preferredWidth: 74
                Layout.preferredHeight: 32
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("SVG")
                enabled: ctrl.hasData
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Save the chart as vector art, written from the bin data")

                background: Rectangle {
                    radius: 10
                    border.color: chartTheme.controlBorder
                    border.width: 1
                    color: svgBtn.hovered
                           ? Qt.lighter(chartTheme.controlBackground, 1.4)
                           : chartTheme.controlBackground
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
                contentItem: Text {
                    text: svgBtn.text; color: chartTheme.controlText
                    font.pixelSize: 12; font.family: inter.name
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    svgDialog.selectedFile = svgDialog.currentFolder + "/"
                                             + statisticsView.exportBaseName() + ".svg"
                    svgDialog.open()
                }
            }

            Button {
                id: csvBtn
                Layout.preferredWidth: 74
                Layout.preferredHeight: 32
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("CSV")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Export statistics to CSV file")

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
                    text: csvBtn.text; color: chartTheme.controlText
                    font.pixelSize: 12; font.family: inter.name
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    csvDialog.selectedFile = csvDialog.currentFolder + "/"
                                             + statisticsView.exportBaseName() + ".csv"
                    csvDialog.open()
                }
            }

            Button {
                id: statsBtn
                Layout.preferredWidth: 90
                Layout.preferredHeight: 32
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("STATS")
                enabled: ctrl.hasData

                background: Rectangle {
                    radius: 10
                    border.color: chartTheme.controlBorder
                    border.width: 1
                    color: statsBtn.hovered
                           ? Qt.lighter(chartTheme.controlBackground, 1.4)
                           : chartTheme.controlBackground
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
                contentItem: Text {
                    text: statsBtn.text; color: chartTheme.controlText
                    font.pixelSize: 12; font.family: inter.name
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: showSummary = !showSummary
            }

            Switch {
                id: themeSwitch
                Layout.alignment: Qt.AlignVCenter
                checked: true
                text: checked ? qsTr("Dark") : qsTr("Light")
                font.pixelSize: 13
                font.family: montserrat.name
                // Let the style lay out indicator + label: the custom
                // contentItem this used to carry did not report the label's
                // width, so the text ran past the window edge.
                Material.foreground: chartTheme.controlText
                onCheckedChanged: chartTheme.dark = checked
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
            text: ctrl.chartTitle.length > 0 ? ctrl.axisXLabel : qsTr("Value")
            color: chartTheme.axisTitle
            font.pixelSize: 14
            font.family: inter.name
            anchors.horizontalCenter: plotArea.horizontalCenter
            anchors.bottom: parent.bottom
        }

        Item {
            id: plotArea
            anchors {
                left: parent.left
                leftMargin: chartArea.marginLeft
                right: parent.right
                top: parent.top
                bottom: parent.bottom
                bottomMargin: chartArea.marginBottom
            }

            Rectangle {
                anchors.fill: parent
                color: chartTheme.plotBackground
                border.color: chartTheme.plotBorder
                border.width: 1
            }

            // Highlighted zero reference line when the range spans 0
            Rectangle {
                visible: ctrl.axisXMin < 0 && ctrl.axisXMax > 0
                readonly property real xRange: ctrl.axisXMax - ctrl.axisXMin
                x: xRange > 0 ? (-ctrl.axisXMin / xRange) * plotArea.width : 0
                y: 0
                width: 1.5
                height: plotArea.height
                color: chartTheme.tickMark
                z: 1
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

            // Bars, one per bin. Each bin contributes two points to
            // histogramPoints (left and right edge at the same count), so bin i
            // is points[2i] .. points[2i+1]. Fill opacity is proportional to the
            // bin's height relative to the tallest bin, so the mode of the
            // distribution reads at a glance.
            Repeater {
                model: ctrl.hasData ? Math.floor(ctrl.histogramPoints.length / 2) : 0

                delegate: Rectangle {
                    readonly property var  binL: ctrl.histogramPoints[2 * index]
                    readonly property var  binR: ctrl.histogramPoints[2 * index + 1]
                    readonly property real xRange: ctrl.axisXMax - ctrl.axisXMin
                    readonly property real frac: ctrl.histogramPeak > 0
                                                 ? binL.y / ctrl.histogramPeak : 0

                    visible: xRange > 0 && ctrl.axisYMax > 0
                    x: xRange > 0 ? (binL.x - ctrl.axisXMin) / xRange * plotArea.width : 0
                    width: xRange > 0
                           ? Math.max(1, (binR.x - binL.x) / xRange * plotArea.width) : 0
                    height: ctrl.axisYMax > 0
                            ? (binL.y / ctrl.axisYMax) * plotArea.height : 0
                    y: plotArea.height - height

                    color: Qt.rgba(chartTheme.seriesStroke.r,
                                   chartTheme.seriesStroke.g,
                                   chartTheme.seriesStroke.b,
                                   0.22 + 0.78 * frac)
                    border.color: chartTheme.seriesStroke
                    border.width: 1
                    antialiasing: true
                }
            }

            // Smooth Gaussian KDE Curve Overlay for Deformed Mode
            Canvas {
                id: kdeCanvas
                anchors.fill: parent
                z: 2
                visible: ctrl.mode === "Deformed" && ctrl.kdePoints.length > 1

                Connections {
                    target: ctrl
                    function onHistogramChanged() { kdeCanvas.requestPaint(); }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var pts = ctrl.kdePoints;
                    if (!pts || pts.length < 2 || ctrl.axisYMax <= 0) return;

                    var rx = ctrl.axisXMax - ctrl.axisXMin;
                    if (rx <= 0) return;

                    ctx.lineWidth = 2.5;
                    ctx.strokeStyle = chartTheme.dark ? "#4fc3f7" : "#0288d1";
                    ctx.beginPath();

                    for (var i = 0; i < pts.length; ++i) {
                        var pt = pts[i];
                        var px = (pt.x - ctrl.axisXMin) / rx * width;
                        var py = height - (pt.y / ctrl.axisYMax) * height;
                        if (i === 0) ctx.moveTo(px, py);
                        else ctx.lineTo(px, py);
                    }
                    ctx.stroke();
                }
            }

            Text {
                anchors.centerIn: parent
                visible: !ctrl.hasData
                text: ctrl.mode === "Deformed"
                      ? qsTr("No deformed state data available for this load step")
                      : qsTr("Select a grain property to display")
                color: chartTheme.placeholder
                font.pixelSize: 16
                font.family: inter.name
            }
        }
    }

    Rectangle {
        id: summaryPanel
        visible: ctrl.hasData && showSummary

        anchors {
            right: parent.right;  rightMargin: 40
            top: chartTitle.bottom; topMargin: 25
        }
        width: 190
        height: summaryColumn.implicitHeight + 24

        color: chartTheme.dark ? "#e61e1e1e" : "#f2ffffff"
        border.color: chartTheme.plotBorder
        border.width: 1
        radius: 8

        Column {
            id: summaryColumn
            anchors {
                left: parent.left;   leftMargin: 14
                right: parent.right; rightMargin: 14
                top: parent.top;     topMargin: 12
            }
            spacing: 6

            Item {
                width: parent.width
                height: 18

                Text {
                    anchors.left: parent.left
                    text: qsTr("Statistics")
                    color: chartTheme.chartTitle
                    font.pixelSize: 13
                    font.bold: true
                    font.family: inter.name
                }

                Text {
                    anchors.right: parent.right
                    text: "×"
                    color: chartTheme.axisLabel
                    font.pixelSize: 16
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: showSummary = false
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: chartTheme.plotBorder
            }

            Repeater {
                model: ctrl.descriptiveStats

                delegate: Item {
                    width: summaryColumn.width
                    height: 18

                    Text {
                        anchors.left: parent.left
                        text: modelData.label
                        color: chartTheme.axisLabel
                        font.pixelSize: 12
                        font.family: montserrat.name
                    }
                    Text {
                        anchors.right: parent.right
                        text: modelData.value
                        color: chartTheme.axisTitle
                        font.pixelSize: 12
                        font.bold: true
                        font.family: montserrat.name
                    }
                }
            }
        }
    }
}
