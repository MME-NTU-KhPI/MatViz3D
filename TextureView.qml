import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 1100
    minimumHeight: 600
    visible: true
    color: chartTheme.appBackground
    title: qsTr("MatViz3D — Texture Editor")

    Material.theme: chartTheme.dark ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    property var ctrl: textureController

    // 0 = pole figures, 1 = ODF sections, 2 = Euler section
    property int currentView: 0

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    // Shared with StatisticsView -- one palette definition, one Dark/Light switch.
    ChartTheme { id: chartTheme }

    readonly property color colBg:     chartTheme.appBackground
    readonly property color colPanel:  chartTheme.panelBackground
    readonly property color colPanel2: chartTheme.panelAlt
    readonly property color colSel:    chartTheme.selection
    readonly property color colBorder: chartTheme.borderColor
    readonly property color colText:   chartTheme.textColor
    readonly property color colSub:    chartTheme.subTextColor
    readonly property color colAccent: chartTheme.accentColor

    // Plot-surface roles (used from the Canvases, which repaint on dark change).
    readonly property color colPlot:   chartTheme.plotBackground
    readonly property color colGrid:   chartTheme.gridLine
    readonly property color colMarker: chartTheme.markerColor
    readonly property color colPoint:  chartTheme.pointColor
    readonly property real  pointAlpha: chartTheme.pointAlpha
    readonly property color colDisc:   chartTheme.dark ? "#5a5a5a" : "#8a8a8a"

    // The item the raster/vector export should capture for the active view.
    function currentPage() {
        if (currentView === 0) return poleFigurePage
        if (currentView === 1) return odfPage
        return eulerPage
    }
    function defaultBaseName() {
        if (currentView === 0) return "pole_figure_" + ctrl.poleFamilyName.replace(/[{}]/g, "")
        if (currentView === 1) return "odf_sections"
        return "euler_section"
    }

    // Contour colour ramp for the ODF sections, in "× random" units.
    // 1× is the random baseline, so it is drawn dim; everything from 2× up is
    // real texture and gets progressively hotter.
    function levelColor(lv) {
        if (lv >= 32) return "#ff5252"
        if (lv >= 16) return "#ff9f45"
        if (lv >= 8)  return "#e8b835"
        if (lv >= 4)  return "#6fd66f"
        if (lv >= 2)  return "#22c3a6"
        return "#4a6b7a"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: colBg
            border.color: colBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 14

                Row {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 6
                    Label {
                        text: qsTr("Method:") + " " + leftPanel.processList[ctrl.process].name
                        color: colSub
                        font.pixelSize: 12; font.family: montserrat.name
                    }
                    RoundButton {
                        text: "\u2212"; implicitWidth: 34; implicitHeight: 34
                        onClicked: ctrl.grainCount = Math.max(10, ctrl.grainCount - 100)
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 54; horizontalAlignment: Text.AlignHCenter
                        text: ctrl.grainCount; color: colText
                        font.pixelSize: 14; font.bold: true; font.family: montserrat.name
                    }
                    RoundButton {
                        text: "+"; implicitWidth: 34; implicitHeight: 34
                        onClicked: ctrl.grainCount = ctrl.grainCount + 100
                    }
                }

                Button {
                    text: qsTr("Generate")
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignVCenter
                    highlighted: true
                    font.pixelSize: 14; font.family: inter.name
                    // reseed(), not regenerate(): draws a NEW global seed so each
                    // press gives a new realization (and the same one the
                    // structure/stress pipelines will reproduce from that seed).
                    onClicked: ctrl.reseed()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("New random seed → new grain orientations")
                }

                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("seed") + " " + ctrl.seed
                    color: colSub
                    font.pixelSize: 12; font.family: montserrat.name
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Save PNG")
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Raster capture of the current plot (2× resolution)")
                    onClicked: {
                        pngDialog.selectedFile = pngDialog.currentFolder + "/" + root.defaultBaseName() + ".png"
                        pngDialog.open()
                    }
                }

                Button {
                    text: qsTr("Save SVG")
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Vector export, written from the plot data")
                    onClicked: {
                        svgDialog.selectedFile = svgDialog.currentFolder + "/" + root.defaultBaseName() + ".svg"
                        svgDialog.open()
                    }
                }

                Switch {
                    id: themeSwitch
                    Layout.alignment: Qt.AlignVCenter
                    checked: true
                    text: checked ? qsTr("Dark") : qsTr("Light")
                    font.pixelSize: 13
                    font.family: montserrat.name
                    // Style-laid-out indicator + label (a custom contentItem
                    // does not report the label width and gets clipped).
                    Material.foreground: root.colText
                    onCheckedChanged: chartTheme.dark = checked
                }

                Button {
                    text: qsTr("Apply to stress")
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: ctrl.applyToStress()
                }
            }
        }

        FileDialog {
            id: pngDialog
            title: qsTr("Save plot as PNG")
            fileMode: FileDialog.SaveFile
            nameFilters: [qsTr("PNG image (*.png)")]
            defaultSuffix: "png"
            onAccepted: {
                // Grab at 2x so the raster is usable in a document.
                var item = root.currentPage()
                var path = ctrl.toLocalFile(selectedFile)
                item.grabToImage(function(result) {
                    if (!result.saveToFile(path))
                        console.warn("TextureView: failed to save PNG to", path)
                }, Qt.size(item.width * 2, item.height * 2))
            }
        }

        FileDialog {
            id: svgDialog
            title: qsTr("Save plot as SVG")
            fileMode: FileDialog.SaveFile
            nameFilters: [qsTr("SVG image (*.svg)")]
            defaultSuffix: "svg"
            onAccepted: ctrl.exportSvg(root.currentView, selectedFile, chartTheme.dark)
        }

        RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    id: leftPanel
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    color: colPanel

                    clip: true

                    // Order must match TextureLibrary::Process -- the index is
                    // assigned straight to ctrl.process.
                    readonly property var processList: [
                        { name: "Random", desc: "Isotropic background", icon: "🎲" },
                        { name: "Extrusion", desc: "Fiber textures (<111>+<100> / <110>)", icon: "⭱" },
                        { name: "Rolling", desc: "Copper, Brass, S / Alpha, Gamma", icon: "⇌" },
                        { name: "Recrystallization", desc: "Cube & Goss components", icon: "❄" },
                        { name: "Shear", desc: "Torsion/Shear components", icon: "⇋" },
                        { name: "Scattered Cube", desc: "Cube-aligned, tilted by ≤ σ", icon: "⊹" }
                    ]

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            Layout.margins: 16
                            text: qsTr("Texture Type")
                            color: colText
                            font.pixelSize: 15; font.bold: true; font.family: inter.name
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Repeater {
                                model: leftPanel.processList
                                delegate: Rectangle {
                                    id: procDelegate
                                    Layout.fillWidth: true
                                    // Every row is laid out from the panel width, never from its
                                    // own text width: without this the longest description grew
                                    // the row past the panel and each row ended up a different
                                    // width. Labels elide instead of pushing the row wider.
                                    Layout.preferredWidth: leftPanel.width
                                    Layout.maximumWidth: leftPanel.width
                                    Layout.preferredHeight: 64
                                    clip: true

                                    property bool active: index === ctrl.process
                                    color: active ? colSel : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        spacing: 12

                                        // Fixed-width icon cell -- the glyphs have wildly
                                        // different advance widths (🎲 vs ⇌ vs ❄), so an
                                        // auto-sized Label made the text start at a different
                                        // x on every row.
                                        Label {
                                            Layout.preferredWidth: 24
                                            Layout.alignment: Qt.AlignVCenter
                                            horizontalAlignment: Text.AlignHCenter
                                            text: modelData.icon
                                            color: procDelegate.active ? colAccent : colSub
                                            font.pixelSize: 18
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            // Never report an implicit width larger than what
                                            // the row can give us.
                                            Layout.minimumWidth: 0
                                            spacing: 2
                                            Label {
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                                text: modelData.name
                                                color: procDelegate.active ? colAccent : colText
                                                font.pixelSize: 14; font.bold: true; font.family: inter.name
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                                text: modelData.desc
                                                color: colSub
                                                font.pixelSize: 11; font.family: montserrat.name
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: 3; height: parent.height
                                        anchors.left: parent.left
                                        color: procDelegate.active ? colAccent : "transparent"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: ctrl.process = index
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: colBorder; Layout.topMargin: 16 }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 16
                            spacing: 12

                            Label { text: "Lattice:"; color: colSub; font.pixelSize: 12; font.family: montserrat.name }

                            RowLayout {
                                spacing: 8
                                Repeater {
                                    model: ["FCC (Cu, Al)", "BCC (Fe, W)"]
                                    delegate: Rectangle {
                                        width: 100; height: 32; radius: 16
                                        color: index === ctrl.lattice ? colSel : "transparent"
                                        border.color: colBorder

                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: index === ctrl.lattice ? colAccent : colText
                                            font.pixelSize: 12; font.family: montserrat.name
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: ctrl.lattice = index
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: colBorder }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.margins: 16
                            spacing: 4
                            Label {
                                text: qsTr("TEXTURE SHARPNESS")
                                color: colSub; font.pixelSize: 11; font.family: montserrat.name
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("Scatter σ (°)"); color: colText; font.pixelSize: 12; font.family: montserrat.name }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: ctrl.scatterDeg.toFixed(1) + "°"
                                    color: colAccent; font.pixelSize: 12; font.bold: true; font.family: montserrat.name
                                }
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 1; to: 40; value: ctrl.scatterDeg
                                onMoved: ctrl.scatterDeg = value
                                Material.accent: colAccent
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: colBorder }
                        Item { Layout.fillHeight: true }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    // ── View selector ───────────────────────────────────────
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        color: colBg
                        border.color: colBorder

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 24
                            anchors.rightMargin: 24
                            spacing: 16

                            // Chip-style view switcher (same pattern as the Lattice
                            // picker) rather than a TabBar: no implicit-width
                            // surprises inside a RowLayout.
                            RowLayout {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 6
                                Repeater {
                                    model: [qsTr("Pole figures"), qsTr("ODF sections"), qsTr("Euler section")]
                                    delegate: Rectangle {
                                        Layout.preferredWidth: 116
                                        Layout.preferredHeight: 30
                                        radius: 15
                                        color: index === root.currentView ? colSel : "transparent"
                                        border.color: index === root.currentView ? colAccent : colBorder

                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: index === root.currentView ? colAccent : colText
                                            font.pixelSize: 12; font.family: montserrat.name
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.currentView = index
                                        }
                                    }
                                }
                            }

                            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: colBorder }

                            Label {
                                text: qsTr("Method:") + " " + leftPanel.processList[ctrl.process].name
                                color: colSub
                                font.pixelSize: 12; font.family: montserrat.name
                            }

                            Item { Layout.fillWidth: true }

                            // Pole family picker — pole figure tab only.
                            RowLayout {
                                visible: root.currentView === 0
                                spacing: 8
                                Label {
                                    text: qsTr("Poles:"); color: colSub
                                    font.pixelSize: 12; font.family: montserrat.name
                                }
                                Repeater {
                                    model: ["{100}", "{110}", "{111}"]
                                    delegate: Rectangle {
                                        width: 58; height: 28; radius: 14
                                        color: index === ctrl.poleFamily ? colSel : "transparent"
                                        border.color: colBorder
                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: index === ctrl.poleFamily ? colAccent : colText
                                            font.pixelSize: 12; font.family: montserrat.name
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: ctrl.poleFamily = index
                                        }
                                    }
                                }
                            }

                            // Peak readout — ODF tab only.
                            Label {
                                visible: root.currentView === 1
                                text: qsTr("peak ") + ctrl.odfMax.toFixed(1) + qsTr("× random  ·  ")
                                      + (ctrl.odfBins > 0 ? (90 / ctrl.odfBins).toFixed(1) : "—") + "° bins"
                                color: colSub
                                font.pixelSize: 12; font.family: montserrat.name
                            }

                            // φ2 slider — Euler tab only.
                            Label {
                                visible: root.currentView === 2
                                text: "φ₂ = " + ctrl.sectionPhi2.toFixed(0) + "°"
                                color: colText
                                font.pixelSize: 13; font.family: montserrat.name
                            }
                            Slider {
                                visible: root.currentView === 2
                                Layout.preferredWidth: 160
                                from: 0; to: 90
                                value: ctrl.sectionPhi2
                                onMoved: ctrl.sectionPhi2 = value
                                Material.accent: colAccent
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: root.currentView

                        // ═══ 0. Pole figures ════════════════════════════════
                        Item {
                            id: poleFigurePage
                            Item {
                                id: pfBox
                                anchors.fill: parent
                                anchors.margins: 24

                                readonly property real radius: Math.min(width, height) / 2 - 26
                                readonly property real cx: width / 2
                                readonly property real cy: height / 2

                                Canvas {
                                    id: poleCanvas
                                    anchors.fill: parent
                                    property var pts: ctrl.polePoints
                                    property bool darkMode: chartTheme.dark
                                    onPtsChanged: requestPaint()
                                    onDarkModeChanged: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()

                                    onPaint: {
                                        var g = getContext("2d"); g.reset()
                                        var R = pfBox.radius, cx = pfBox.cx, cy = pfBox.cy
                                        if (R <= 0) return

                                        // projection disc
                                        g.fillStyle = root.colPlot
                                        g.beginPath(); g.arc(cx, cy, R, 0, 2 * Math.PI); g.fill()
                                        g.strokeStyle = root.colDisc; g.lineWidth = 1.5; g.stroke()

                                        // RD / TD crosshair + 45° ring
                                        g.strokeStyle = root.colBorder; g.lineWidth = 1
                                        g.beginPath()
                                        g.moveTo(cx - R, cy); g.lineTo(cx + R, cy)
                                        g.moveTo(cx, cy - R); g.lineTo(cx, cy + R)
                                        g.stroke()
                                        g.beginPath()
                                        // 45° from ND projects to tan(22.5°) = 0.4142 R
                                        g.arc(cx, cy, R * 0.41421, 0, 2 * Math.PI)
                                        g.stroke()

                                        if (!pts) return
                                        g.fillStyle = Qt.rgba(root.colPoint.r, root.colPoint.g,
                                                              root.colPoint.b, root.pointAlpha)
                                        for (var i = 0; i + 1 < pts.length; i += 2) {
                                            g.beginPath()
                                            g.arc(cx + pts[i] * R, cy + pts[i + 1] * R, 2.0, 0, 2 * Math.PI)
                                            g.fill()
                                        }
                                    }
                                }

                                Label {
                                    x: pfBox.cx - width / 2
                                    y: pfBox.cy - pfBox.radius - 20
                                    text: "RD"; color: colText
                                    font.pixelSize: 12; font.bold: true; font.family: montserrat.name
                                }
                                Label {
                                    x: pfBox.cx + pfBox.radius + 8
                                    y: pfBox.cy - height / 2
                                    text: "TD"; color: colText
                                    font.pixelSize: 12; font.bold: true; font.family: montserrat.name
                                }
                                Label {
                                    x: pfBox.cx + 6
                                    y: pfBox.cy + 4
                                    text: "ND"; color: colSub
                                    font.pixelSize: 10; font.family: montserrat.name
                                }
                                Label {
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    text: ctrl.poleFamilyName + qsTr(" stereographic projection · ")
                                          + (ctrl.polePoints.length / 2) + qsTr(" poles from ")
                                          + ctrl.grainCount + qsTr(" grains")
                                    color: colSub
                                    font.pixelSize: 11; font.family: montserrat.name
                                }
                            }
                        }

                        // ═══ 1. ODF sections ════════════════════════════════
                        Item {
                            id: odfPage
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 16

                                    Repeater {
                                        model: ctrl.odfSections
                                        delegate: ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            spacing: 6

                                            Label {
                                                Layout.alignment: Qt.AlignHCenter
                                                text: "φ₂ = " + modelData.phi2.toFixed(0) + "°   ("
                                                      + modelData.max.toFixed(1) + "×)"
                                                color: colText
                                                font.pixelSize: 12; font.bold: true
                                                font.family: montserrat.name
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                color: root.colPlot
                                                border.color: colBorder

                                                Canvas {
                                                    anchors.fill: parent
                                                    anchors.margins: 1
                                                    property var contours: modelData.contours
                                                    property bool darkMode: chartTheme.dark
                                                    onContoursChanged: requestPaint()
                                                    onDarkModeChanged: requestPaint()
                                                    onWidthChanged: requestPaint()
                                                    onHeightChanged: requestPaint()

                                                    onPaint: {
                                                        var g = getContext("2d"); g.reset()

                                                        // 15° grid
                                                        g.strokeStyle = root.colGrid; g.lineWidth = 1
                                                        g.beginPath()
                                                        for (var t = 1; t < 6; ++t) {
                                                            var f = t / 6
                                                            g.moveTo(f * width, 0);  g.lineTo(f * width, height)
                                                            g.moveTo(0, f * height); g.lineTo(width, f * height)
                                                        }
                                                        g.stroke()

                                                        if (!contours) return
                                                        for (var c = 0; c < contours.length; ++c) {
                                                            var lv = contours[c].level
                                                            var segs = contours[c].segs
                                                            g.strokeStyle = root.levelColor(lv)
                                                            g.lineWidth = lv >= 4 ? 1.8 : 1.0
                                                            g.beginPath()
                                                            for (var i = 0; i + 3 < segs.length; i += 4) {
                                                                g.moveTo(segs[i] * width, segs[i + 1] * height)
                                                                g.lineTo(segs[i + 2] * width, segs[i + 3] * height)
                                                            }
                                                            g.stroke()
                                                        }
                                                    }
                                                }
                                            }

                                            Label {
                                                Layout.alignment: Qt.AlignHCenter
                                                text: "φ₁ →   ↓ Φ   (0–90°)"
                                                color: colSub
                                                font.pixelSize: 10; font.family: montserrat.name
                                            }
                                        }
                                    }
                                }

                                // Contour legend
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 14
                                    Label {
                                        text: qsTr("Contours (× random):")
                                        color: colSub
                                        font.pixelSize: 11; font.family: montserrat.name
                                    }
                                    Repeater {
                                        model: [1, 2, 4, 8, 16, 32]
                                        delegate: RowLayout {
                                            spacing: 5
                                            Rectangle {
                                                Layout.preferredWidth: 18
                                                Layout.preferredHeight: 3
                                                color: root.levelColor(modelData)
                                            }
                                            Label {
                                                text: modelData + "×"
                                                color: colSub
                                                font.pixelSize: 11; font.family: montserrat.name
                                            }
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: qsTr("histogram estimate · noise floor ≈1.5×")
                                        color: colSub
                                        font.pixelSize: 10; font.family: montserrat.name
                                    }
                                }
                            }
                        }

                        // ═══ 2. Euler section (kept as reference view) ══════
                        Item {
                            id: eulerPage
                            Item {
                            id: plot
                            anchors.fill: parent
                            // Room for the Φ tick labels + rotated axis title on the
                            // left, and the φ1 tick labels + title underneath.
                            anchors.leftMargin: 72
                            anchors.rightMargin: 32
                            anchors.topMargin: 20
                            anchors.bottomMargin: 52

                            Rectangle {
                                anchors.fill: parent
                                color: root.colPlot
                                border.color: colBorder
                            }

                            // Owned by the controller so the plot and the SVG
                            // export cannot disagree about the slab thickness.
                            readonly property real phi2Tol: ctrl.sectionTol

                            // ── Φ (vertical) axis: gridlines + ticks, 0..90° downwards ──
                            Repeater {
                                model: 7   // 0, 15, ..., 90
                                Item {
                                    property real phiVal: index * 15
                                    width: plot.width
                                    y: (phiVal / 90.0) * plot.height
                                    height: 1

                                    Rectangle {
                                        width: plot.width; height: 1
                                        color: root.colGrid
                                    }
                                    Label {
                                        x: -40
                                        y: -7
                                        text: parent.phiVal + "°"
                                        color: colSub
                                        font.pixelSize: 11
                                        font.family: montserrat.name
                                    }
                                }
                            }

                            // ── φ1 (horizontal) axis: gridlines + ticks, 0..90° to the right ──
                            Repeater {
                                model: 7   // 0, 15, ..., 90
                                Item {
                                    property real phi1Val: index * 15
                                    x: (phi1Val / 90.0) * plot.width
                                    width: 1
                                    height: plot.height

                                    Rectangle {
                                        width: 1; height: plot.height
                                        color: root.colGrid
                                    }
                                    Label {
                                        x: -width / 2
                                        y: plot.height + 6
                                        text: parent.phi1Val + "°"
                                        color: colSub
                                        font.pixelSize: 11
                                        font.family: montserrat.name
                                    }
                                }
                            }

                            // ── Axis titles ──
                            Label {
                                x: (plot.width - width) / 2
                                y: plot.height + 24
                                text: "φ₁  (°)"
                                color: colText
                                font.pixelSize: 12; font.bold: true
                                font.family: montserrat.name
                            }
                            Label {
                                // Rotated so it reads bottom-to-top alongside the Φ ticks.
                                text: "Φ  (°)"
                                color: colText
                                font.pixelSize: 12; font.bold: true
                                font.family: montserrat.name
                                rotation: -90
                                x: -56 - width / 2 + height / 2
                                y: (plot.height - height) / 2
                            }

                            Canvas {
                                id: eulerCanvas
                                anchors.fill: parent
                                property var pts: ctrl.eulerPoints
                                property real sect: ctrl.sectionPhi2
                                property real tol: plot.phi2Tol
                                property bool darkMode: chartTheme.dark
                                onPtsChanged: requestPaint()
                                onSectChanged: requestPaint()
                                onDarkModeChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()

                                onPaint: {
                                    var g = getContext("2d"); g.reset()
                                    if (!pts) return
                                    g.fillStyle = Qt.rgba(root.colPoint.r, root.colPoint.g,
                                                          root.colPoint.b, root.pointAlpha)
                                    for (var i = 0; i < pts.length; ++i) {
                                        var dp = Math.abs(pts[i].phi2 - sect)
                                        if (dp > 180) dp = 360 - dp
                                        if (dp > tol) continue
                                        g.beginPath()
                                        g.arc(pts[i].x * width, pts[i].y * height, 3.5, 0, 2 * Math.PI)
                                        g.fill()
                                    }
                                }
                            }

                            Repeater {
                                model: ctrl.componentLabels
                                delegate: Item {
                                    property real dp: {
                                        var d = Math.abs(modelData.phi2 - ctrl.sectionPhi2)
                                        return d > 180 ? 360 - d : d
                                    }
                                    visible: dp <= plot.phi2Tol + 5
                                    x: modelData.x * plot.width
                                    y: modelData.y * plot.height

                                    Rectangle {
                                        width: 8; height: 8; radius: 4
                                        color: root.colMarker
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        x: 12; anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.name
                                        color: root.colMarker
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
