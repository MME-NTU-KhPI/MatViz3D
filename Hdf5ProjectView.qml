import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Window {
    id: hdf5ProjectView

    Material.theme: chartTheme.dark ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    width: 1220
    height: 740
    minimumWidth: 1040
    minimumHeight: 600
    color: chartTheme.windowBackground
    title: qsTr("HDF5 Project View")

    property var ctrl: hdf5ProjectController
    property bool showStatsCard: true

    readonly property int xTicks: 8
    readonly property int yTicks: 6

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    ChartTheme { id: chartTheme }

    function fmt(v) {
        if (v === undefined || v === null) return "0";
        if (Math.abs(v) < 1e-9) return "0";
        if (Math.abs(v) >= 10000 || Math.abs(v) < 0.001)
            return v.toExponential(2);
        return parseFloat(v.toPrecision(4)).toString();
    }

    function exportBaseName() {
        var prop = ctrl.axisXLabel.length > 0 ? ctrl.axisXLabel : "deformed_stat";
        var geom = ctrl.currentGeomName.length > 0 ? ("geom_" + ctrl.currentGeomName) : "geom";
        var ls   = ctrl.currentLoadStepName.length > 0 ? ctrl.currentLoadStepName : "ls";
        return (geom + "_" + ls + "_" + prop).replace(/[^a-zA-Z0-9_-]/g, "_").toLowerCase();
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open MatViz3D HDF5 Project")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("HDF5 Files (*.h5 *.hdf5 *.hdf)"), qsTr("All Files (*.*)")]
        onAccepted: ctrl.openFile(ctrl.toLocalFile(selectedFile))
    }

    FileDialog {
        id: pngDialog
        title: qsTr("Save PDF chart as PNG")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            var path = ctrl.toLocalFile(selectedFile);
            chartArea.grabToImage(function(result) {
                if (!result.saveToFile(path))
                    console.warn("Hdf5ProjectView: failed to save PNG to", path);
            }, Qt.size(chartArea.width * 2, chartArea.height * 2));
        }
    }

    FileDialog {
        id: svgDialog
        title: qsTr("Save PDF chart as SVG")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("SVG image (*.svg)")]
        defaultSuffix: "svg"
        onAccepted: ctrl.exportSvg(selectedFile, chartTheme.dark, hdf5ProjectView.showStatsCard)
    }

    FileDialog {
        id: csvDialog
        title: qsTr("Export Load Step to CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV Data (*.csv)")]
        defaultSuffix: "csv"
        onAccepted: ctrl.exportCSV(ctrl.toLocalFile(selectedFile))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ═══════════════════════════════════════════════════════════════════
        //  Top Navigation & File Toolbar
        // ═══════════════════════════════════════════════════════════════════
        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: chartTheme.dark ? "#202020" : "#e8e8e8"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                Button {
                    text: qsTr("Open File...")
                    Layout.preferredHeight: 34
                    font.family: inter.name
                    font.pixelSize: 13
                    onClicked: openDialog.open()
                }

                Text {
                    text: ctrl.isOpen ? (qsTr("File: ") + ctrl.filePath.split(/[\\/]/).pop()) : qsTr("No project opened")
                    color: chartTheme.controlText
                    font.pixelSize: 13
                    font.family: montserrat.name
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 180
                }

                Rectangle {
                    width: 1
                    Layout.preferredHeight: 30
                    color: chartTheme.controlBorder
                }

                // Geometry Set Selector
                Text {
                    text: qsTr("Geom:")
                    color: chartTheme.controlText
                    font.pixelSize: 13
                    font.family: montserrat.name
                    visible: ctrl.isOpen && ctrl.geomSets.length > 0
                }

                ComboBox {
                    id: geomCombo
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 32
                    visible: ctrl.isOpen && ctrl.geomSets.length > 0
                    model: ctrl.geomSets
                    currentIndex: ctrl.currentGeomIndex
                    onActivated: (index) => ctrl.selectGeomSet(index)
                }

                Rectangle {
                    width: 1
                    Layout.preferredHeight: 30
                    color: chartTheme.controlBorder
                    visible: ctrl.isOpen
                }

                // Load Step Stepper & Scrubber
                RowLayout {
                    spacing: 6
                    visible: ctrl.isOpen && ctrl.loadSteps.length > 0

                    Button {
                        text: "◀"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        enabled: ctrl.currentLoadStepIndex > 0
                        onClicked: ctrl.prevStep()
                    }

                    ComboBox {
                        id: stepCombo
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 32
                        model: ctrl.loadSteps
                        currentIndex: ctrl.currentLoadStepIndex
                        onActivated: (index) => ctrl.selectLoadStep(index)
                    }

                    Button {
                        text: "▶"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        enabled: ctrl.currentLoadStepIndex < ctrl.loadSteps.length - 1
                        onClicked: ctrl.nextStep()
                    }

                    Slider {
                        id: stepSlider
                        Layout.preferredWidth: 140
                        Layout.alignment: Qt.AlignVCenter
                        from: 0
                        to: Math.max(0, ctrl.loadSteps.length - 1)
                        stepSize: 1
                        value: ctrl.currentLoadStepIndex
                        onMoved: ctrl.selectLoadStep(Math.round(value))
                    }

                    Text {
                        text: (ctrl.currentLoadStepIndex + 1) + " / " + ctrl.loadSteps.length
                        color: chartTheme.controlText
                        font.pixelSize: 12
                        font.family: montserrat.name
                        Layout.preferredWidth: 60
                    }
                }

                Item { Layout.fillWidth: true }

                // 3D Sync Button
                Button {
                    text: qsTr("Sync to 3D")
                    Layout.preferredHeight: 34
                    enabled: ctrl.isOpen && ctrl.hasVoxels
                    font.family: inter.name
                    font.pixelSize: 12
                    onClicked: ctrl.pushTo3DView()
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: ctrl.hasVoxels ? qsTr("Push the selected geometry and load step mesh, field, and deformations into the main 3D viewport") : qsTr("This geometry set has no 3D voxels to display")
                }

                CheckBox {
                    id: autoSyncCheck
                    text: qsTr("Auto 3D")
                    checked: ctrl.autoSync3D
                    onCheckedChanged: ctrl.autoSync3D = checked
                    font.family: montserrat.name
                    font.pixelSize: 12
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Automatically sync 3D viewport whenever you change geometry or scrub load steps")
                }

                Switch {
                    id: themeSwitch
                    checked: true
                    text: checked ? qsTr("Dark") : qsTr("Light")
                    font.pixelSize: 12
                    font.family: montserrat.name
                    onCheckedChanged: chartTheme.dark = checked
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        //  Main Body: Left Metadata Sidebar + Right Chart & Stats Area
        // ═══════════════════════════════════════════════════════════════════
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Left Metadata Sidebar ─────────────────────────────────────
            Rectangle {
                id: leftSidebar
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                color: chartTheme.dark ? "#242424" : "#f0f0f0"
                border.color: chartTheme.controlBorder
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        spacing: 14

                        Text {
                            text: qsTr("Dataset Info")
                            color: chartTheme.chartTitle
                            font.pixelSize: 15
                            font.bold: true
                            font.family: montserrat.name
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: metaCol.implicitHeight + 16
                            color: chartTheme.plotBackground
                            border.color: chartTheme.plotBorder
                            radius: 6

                            ColumnLayout {
                                id: metaCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Text { text: qsTr("Grid Size: ") + (ctrl.cubeSize > 0 ? (ctrl.cubeSize + "³") : qsTr("N/A")); color: chartTheme.controlText; font.pixelSize: 12; font.family: inter.name }
                                Text { text: qsTr("Grains / Points: ") + (ctrl.numPoints > 0 ? ctrl.numPoints : qsTr("N/A")); color: chartTheme.controlText; font.pixelSize: 12; font.family: inter.name }
                                Text { text: qsTr("RNG Seed: ") + (ctrl.seed !== 0 ? ctrl.seed : qsTr("N/A")); color: chartTheme.controlText; font.pixelSize: 12; font.family: inter.name }
                                Text { text: qsTr("Algorithm: ") + (ctrl.algorithm.length > 0 ? ctrl.algorithm : qsTr("Unknown")); color: chartTheme.controlText; font.pixelSize: 12; font.family: inter.name; visible: ctrl.algorithm.length > 0 }
                                Text { text: qsTr("Solver: ") + (ctrl.solver.length > 0 ? ctrl.solver.toUpperCase() : qsTr("Unknown")); color: chartTheme.controlText; font.pixelSize: 12; font.family: inter.name; visible: ctrl.solver.length > 0 }
                                Text { text: qsTr("Geometry: ") + (ctrl.hasVoxels ? qsTr("3D Voxels Available") : qsTr("None (Stiffness only)")); color: ctrl.hasVoxels ? "#81c784" : "#e57373"; font.pixelSize: 12; font.family: inter.name }
                                Text { text: qsTr("Load Steps: ") + ctrl.loadSteps.length; color: chartTheme.controlText; font.pixelSize: 12; font.family: inter.name }
                            }
                        }

                        Text {
                            text: qsTr("Generator Parameters")
                            color: chartTheme.chartTitle
                            font.pixelSize: 14
                            font.bold: true
                            font.family: montserrat.name
                            visible: ctrl.geomParamsSummary.length > 0 || ctrl.algorithm.length > 0
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: genCol.implicitHeight + 16
                            color: chartTheme.plotBackground
                            border.color: chartTheme.plotBorder
                            radius: 6
                            visible: ctrl.geomParamsSummary.length > 0 || ctrl.algorithm.length > 0

                            ColumnLayout {
                                id: genCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                Text {
                                    text: ctrl.geomParamsSummary.length > 0 ? ctrl.geomParamsSummary : (qsTr("Algorithm: ") + ctrl.algorithm)
                                    color: chartTheme.controlText
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: qsTr("Reproduce Geometry")
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    font.family: inter.name
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (ctrl.reproduceGeometry()) {
                                            reproduceNotice.text = qsTr("Parameters loaded into generator!");
                                            reproduceNotice.color = "#81c784";
                                            reproduceNotice.visible = true;
                                        } else {
                                            reproduceNotice.text = qsTr("Failed to load parameters.");
                                            reproduceNotice.color = "#e57373";
                                            reproduceNotice.visible = true;
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Load algorithm and parameters into generator settings to reproduce this microstructure")
                                }

                                Text {
                                    id: reproduceNotice
                                    visible: false
                                    color: "#81c784"
                                    font.pixelSize: 10
                                    font.family: inter.name
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Text {
                            text: qsTr("Applied Loading (Macro ε)")
                            color: chartTheme.chartTitle
                            font.pixelSize: 14
                            font.bold: true
                            font.family: montserrat.name
                            visible: ctrl.appliedStrain.length >= 6
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: epsCol.implicitHeight + 16
                            color: chartTheme.plotBackground
                            border.color: chartTheme.plotBorder
                            radius: 6
                            visible: ctrl.appliedStrain.length >= 6

                            ColumnLayout {
                                id: epsCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Text { text: qsTr("exx: ") + fmt(ctrl.appliedStrain[0]); color: "#4fc3f7"; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("eyy: ") + fmt(ctrl.appliedStrain[1]); color: "#4fc3f7"; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("ezz: ") + fmt(ctrl.appliedStrain[2]); color: "#4fc3f7"; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("exy: ") + fmt(ctrl.appliedStrain[3]); color: "#81c784"; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("eyz: ") + fmt(ctrl.appliedStrain[4]); color: "#81c784"; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("exz: ") + fmt(ctrl.appliedStrain[5]); color: "#81c784"; font.family: "monospace"; font.pixelSize: 11 }
                            }
                        }

                        Text {
                            text: qsTr("Macro Response")
                            color: chartTheme.chartTitle
                            font.pixelSize: 14
                            font.bold: true
                            font.family: montserrat.name
                            visible: ctrl.macroStress.length > 0
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: macroCol.implicitHeight + 16
                            color: chartTheme.plotBackground
                            border.color: chartTheme.plotBorder
                            radius: 6
                            visible: ctrl.macroStress.length > 0

                            ColumnLayout {
                                id: macroCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Text { text: qsTr("von Mises: ") + fmt(ctrl.macroVonMises) + " Pa"; color: "#ffb74d"; font.bold: true; font.family: inter.name; font.pixelSize: 12 }
                                Text { text: qsTr("σ_avg: ") + (ctrl.macroStress.length > 0 ? fmt(ctrl.macroStress[7]) : "0") + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                            }
                        }

                        Text {
                            text: qsTr("Effective Stiffness")
                            color: chartTheme.chartTitle
                            font.pixelSize: 14
                            font.bold: true
                            font.family: montserrat.name
                            visible: ctrl.hasStiffness
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: stiffCol.implicitHeight + 16
                            color: chartTheme.plotBackground
                            border.color: chartTheme.plotBorder
                            radius: 6
                            visible: ctrl.hasStiffness

                            ColumnLayout {
                                id: stiffCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Text { text: qsTr("Ex:  ") + fmt(ctrl.stiffnessModuli[0]) + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("Ey:  ") + fmt(ctrl.stiffnessModuli[1]) + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("Ez:  ") + fmt(ctrl.stiffnessModuli[2]) + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("Gxy: ") + fmt(ctrl.stiffnessModuli[3]) + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("Gyz: ") + fmt(ctrl.stiffnessModuli[4]) + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                                Text { text: qsTr("Gxz: ") + fmt(ctrl.stiffnessModuli[5]) + " Pa"; color: chartTheme.controlText; font.family: "monospace"; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }

            // ── Right Main Chart & Statistics Area ────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Control Bar for Property, Mode & Bins
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        // Aggregation Mode Radio Buttons
                        RadioButton {
                            id: fullVolRadio
                            text: qsTr("Full Volume")
                            checked: ctrl.statMode === "FullVolume"
                            font.pixelSize: 13
                            font.family: montserrat.name
                            onClicked: ctrl.setStatMode("FullVolume")
                        }

                        RadioButton {
                            id: perGrainRadio
                            text: qsTr("Per-Grain Mean")
                            checked: ctrl.statMode === "PerGrain"
                            font.pixelSize: 13
                            font.family: montserrat.name
                            onClicked: ctrl.setStatMode("PerGrain")
                        }

                        // Property Combo
                        ComboBox {
                            id: propertyCombo
                            Layout.preferredWidth: 260
                            Layout.preferredHeight: 32
                            model: ctrl.availableProperties
                            currentIndex: ctrl.selectedPropertyIndex
                            font.pixelSize: 12
                            font.family: montserrat.name
                            onActivated: ctrl.selectProperty(currentIndex)
                        }

                        Text {
                            text: qsTr("Bins:")
                            color: chartTheme.controlText
                            font.pixelSize: 13
                            font.family: montserrat.name
                            visible: ctrl.hasData
                        }

                        Slider {
                            id: binSlider
                            Layout.preferredWidth: 120
                            from: 5; to: 70; stepSize: 1; value: ctrl.binCount
                            visible: ctrl.hasData
                            onMoved: ctrl.setBinCount(Math.round(value))
                        }

                        Text {
                            text: ctrl.binCount
                            color: chartTheme.axisTitle
                            font.pixelSize: 13
                            font.bold: true
                            font.family: montserrat.name
                            visible: ctrl.hasData
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: qsTr("PNG")
                            Layout.preferredHeight: 32
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 4
                            bottomPadding: 4
                            font.family: inter.name
                            font.pixelSize: 12
                            enabled: ctrl.hasData
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Export PDF chart as high-resolution PNG image")
                            onClicked: {
                                pngDialog.selectedFile = pngDialog.currentFolder + "/" + hdf5ProjectView.exportBaseName() + ".png";
                                pngDialog.open();
                            }
                        }

                        Button {
                            text: qsTr("SVG")
                            Layout.preferredHeight: 32
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 4
                            bottomPadding: 4
                            font.family: inter.name
                            font.pixelSize: 12
                            enabled: ctrl.hasData
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Export PDF chart as scalable vector graphic (SVG)")
                            onClicked: {
                                svgDialog.selectedFile = svgDialog.currentFolder + "/" + hdf5ProjectView.exportBaseName() + ".svg";
                                svgDialog.open();
                            }
                        }

                        Button {
                            text: qsTr("CSV")
                            Layout.preferredHeight: 32
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 4
                            bottomPadding: 4
                            font.family: inter.name
                            font.pixelSize: 12
                            enabled: ctrl.hasData
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Export current load step node and tensor data to CSV")
                            onClicked: {
                                csvDialog.selectedFile = csvDialog.currentFolder + "/" + hdf5ProjectView.exportBaseName() + ".csv";
                                csvDialog.open();
                            }
                        }

                        Button {
                            text: qsTr("STATS")
                            Layout.preferredHeight: 32
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 4
                            bottomPadding: 4
                            font.family: inter.name
                            font.pixelSize: 12
                            enabled: ctrl.hasData
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Toggle statistics card overlay")
                            onClicked: showStatsCard = !showStatsCard
                        }
                    }
                }

                // Chart Title
                Text {
                    id: chartTitleText
                    Layout.alignment: Qt.AlignHCenter
                    text: ctrl.chartTitle
                    color: chartTheme.chartTitle
                    font.pixelSize: 17
                    font.bold: true
                    font.family: montserrat.name
                    visible: ctrl.hasData
                }

                // Canvas Plot Area
                Item {
                    id: chartArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 20

                    readonly property int marginLeft:   75
                    readonly property int marginBottom: 50

                    Text {
                        text: qsTr("Frequency")
                        color: chartTheme.axisTitle
                        font.pixelSize: 13
                        font.family: inter.name
                        rotation: -90
                        anchors.verticalCenter: plotArea.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: -16
                    }

                    Text {
                        text: ctrl.axisXLabel.length > 0 ? ctrl.axisXLabel : qsTr("Value")
                        color: chartTheme.axisTitle
                        font.pixelSize: 13
                        font.family: inter.name
                        anchors.horizontalCenter: plotArea.horizontalCenter
                        anchors.bottom: parent.bottom
                    }

                    Item {
                        id: plotArea
                        anchors {
                            left: parent.left; leftMargin: chartArea.marginLeft
                            right: parent.right; rightMargin: 20
                            top: parent.top; topMargin: 10
                            bottom: parent.bottom; bottomMargin: chartArea.marginBottom
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: chartTheme.plotBackground
                            border.color: chartTheme.plotBorder
                            border.width: 1
                        }

                        // Zero reference line
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

                        // X-axis Grid & Ticks
                        Repeater {
                            model: hdf5ProjectView.xTicks + 1
                            Item {
                                property real frac: index / hdf5ProjectView.xTicks
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
                                    y: plotArea.height + 8
                                    x: -width / 2
                                    text: hdf5ProjectView.fmt(parent.val)
                                    color: chartTheme.axisLabel
                                    font.pixelSize: 11
                                    font.family: montserrat.name
                                }
                            }
                        }

                        // Y-axis Grid & Ticks
                        Repeater {
                            model: hdf5ProjectView.yTicks + 1
                            Item {
                                property real frac: index / hdf5ProjectView.yTicks
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
                                    x: -width - 10
                                    y: -height / 2
                                    text: Math.round(parent.val)
                                    color: chartTheme.axisLabel
                                    font.pixelSize: 11
                                    font.family: montserrat.name
                                }
                            }
                        }

                        // Histogram Bars
                        Repeater {
                            model: ctrl.hasData ? Math.floor(ctrl.histogramPoints.length / 2) : 0

                            delegate: Rectangle {
                                readonly property var  binL: ctrl.histogramPoints[2 * index]
                                readonly property var  binR: ctrl.histogramPoints[2 * index + 1]
                                readonly property real xRange: ctrl.axisXMax - ctrl.axisXMin
                                readonly property real frac: ctrl.histogramPeak > 0 ? binL.y / ctrl.histogramPeak : 0

                                visible: xRange > 0 && ctrl.axisYMax > 0
                                x: xRange > 0 ? (binL.x - ctrl.axisXMin) / xRange * plotArea.width : 0
                                width: xRange > 0 ? Math.max(1, (binR.x - binL.x) / xRange * plotArea.width) : 0
                                height: ctrl.axisYMax > 0 ? (binL.y / ctrl.axisYMax) * plotArea.height : 0
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

                        // Smooth Gaussian KDE Curve Overlay via Canvas
                        Canvas {
                            id: kdeCanvas
                            anchors.fill: parent
                            z: 2

                            Connections {
                                target: ctrl
                                function onStatsChanged() { kdeCanvas.requestPaint(); }
                            }

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();

                                var pts = ctrl.kdePoints;
                                if (!pts || pts.length < 2 || ctrl.kdeMax <= 0) return;

                                var rx = ctrl.axisXMax - ctrl.axisXMin;
                                if (rx <= 0) return;

                                ctx.lineWidth = 2.5;
                                ctx.strokeStyle = "#4fc3f7";
                                ctx.beginPath();

                                for (var i = 0; i < pts.length; ++i) {
                                    var pt = pts[i];
                                    var px = (pt.x - ctrl.axisXMin) / rx * width;
                                    var py = (ctrl.axisYMax > 0) ? (height - (pt.y / ctrl.axisYMax) * height) : height;
                                    if (i === 0) ctx.moveTo(px, py);
                                    else ctx.lineTo(px, py);
                                }
                                ctx.stroke();
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !ctrl.hasData
                            text: qsTr("Open an HDF5 project or select a load step to display PDF statistics")
                            color: chartTheme.placeholder
                            font.pixelSize: 15
                            font.family: inter.name
                        }
                    }

                    // Floating Descriptive Statistics Card
                    Rectangle {
                        id: statsCard
                        visible: ctrl.hasData && hdf5ProjectView.showStatsCard
                        anchors {
                            right: parent.right; rightMargin: 30
                            top: parent.top; topMargin: 20
                        }
                        width: 220
                        height: statsCardCol.implicitHeight + 24
                        color: chartTheme.dark ? "#e61e1e1e" : "#f2ffffff"
                        border.color: chartTheme.plotBorder
                        border.width: 1
                        radius: 8
                        z: 10

                        Column {
                            id: statsCardCol
                            anchors {
                                left: parent.left; leftMargin: 12
                                right: parent.right; rightMargin: 12
                                top: parent.top; topMargin: 10
                            }
                            spacing: 5

                            Item {
                                width: parent.width
                                height: 20

                                Text {
                                    anchors.left: parent.left
                                    text: qsTr("Deformed Statistics")
                                    color: chartTheme.chartTitle
                                    font.pixelSize: 13
                                    font.bold: true
                                    font.family: montserrat.name
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
                                        onClicked: hdf5ProjectView.showStatsCard = false
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
                                    width: statsCardCol.width
                                    height: 17

                                    Text {
                                        anchors.left: parent.left
                                        text: modelData.label
                                        color: chartTheme.axisLabel
                                        font.pixelSize: 11
                                        font.family: montserrat.name
                                    }
                                    Text {
                                        anchors.right: parent.right
                                        text: modelData.value
                                        color: chartTheme.axisTitle
                                        font.pixelSize: 11
                                        font.bold: true
                                        font.family: montserrat.name
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
