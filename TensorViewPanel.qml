// ============================================================================
//  TensorViewPanel.qml
//
//  Controls for the tensor-field overlays drawn inside the main 3D view.
//
//  Kept out of MainWindow.qml (already ~1300 lines) but styled to match the
//  "Field view" panel it sits under, so the two read as one stack: the same
//  translucent card, the same Montserrat title / Inter body split, the same
//  scaled-down icon-only switches.
//
//  Everything here is off by default. The per-voxel tensor snapshot behind the
//  glyphs is built lazily on first use, so a user who never opens this panel
//  never pays for it.
// ============================================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

Item {
    id: panel

    /// The OpenGLWidgetQML instance to drive.
    property var target: null

    /// Font families, passed in so this file does not load its own copies of
    /// fonts MainWindow has already loaded.
    property string titleFont: ""
    property string bodyFont: ""

    /// Grid edge length, for the slice slider's range.
    property int gridSize: 1

    implicitHeight: card.implicitHeight

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    Rectangle {
        id: card
        anchors.fill: parent
        color: "#80282828"
        radius: 13
        implicitHeight: content.implicitHeight + 30

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 15
            spacing: 10

            // ── Header ──────────────────────────────────────────────────
            Row {
                width: parent.width
                spacing: 10

                Switch {
                    id: glyphSwitch
                    width: 50
                    height: 20
                    scale: 0.7
                    display: AbstractButton.IconOnly
                    checked: false
                    onCheckedChanged: if (panel.target) panel.target.setShowGlyphs(checked)
                }

                Text {
                    color: "#d9d9d9"
                    text: qsTr("Tensor glyphs")
                    font.pixelSize: 14
                    font.family: panel.titleFont
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: (glyphSwitch.checked || streamSwitch.checked) && panel.target && panel.target.tensorSourceMissing
                text: qsTr("This solve has no data for the selected tensor.")
                color: "#e0a04a"
                font.pixelSize: 11
                font.family: panel.bodyFont
            }

            // ── Source ──────────────────────────────────────────────────
            Row {
                width: parent.width
                visible: glyphSwitch.checked || streamSwitch.checked
                spacing: 10

                Text {
                    text: qsTr("Tensor:")
                    color: "#c6c6c6"
                    font.pixelSize: 14
                    font.family: panel.bodyFont
                    anchors.verticalCenter: parent.verticalCenter
                }

                ComboBox {
                    id: sourceCombo
                    width: 150
                    model: [ qsTr("Stress"), qsTr("Strain") ]
                    onActivated: if (panel.target) panel.target.setTensorSource(currentIndex)
                }
            }

            Row {
                visible: glyphSwitch.checked || streamSwitch.checked
                spacing: 10

                Switch {
                    id: deviatoricSwitch
                    width: 50
                    height: 20
                    scale: 0.7
                    display: AbstractButton.IconOnly
                    onCheckedChanged: if (panel.target) panel.target.setTensorDeviatoric(checked)
                }

                Text {
                    text: qsTr("Deviatoric part only")
                    color: "#c6c6c6"
                    font.pixelSize: 12
                    font.family: panel.bodyFont
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // ── Density ─────────────────────────────────────────────────
            // 0 on the slider means "pick a stride from the glyph budget",
            // which is what any grid big enough to matter wants.
            Column {
                width: parent.width
                visible: glyphSwitch.checked
                spacing: 2

                Text {
                    text: strideSlider.value < 1
                          ? qsTr("Density: automatic (%1 glyphs)").arg(panel.target ? panel.target.glyphCount : 0)
                          : qsTr("Density: every %1 voxels (%2 glyphs)")
                                .arg(Math.round(strideSlider.value))
                                .arg(panel.target ? panel.target.glyphCount : 0)
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: strideSlider
                    width: parent.width
                    from: 0
                    to: 12
                    stepSize: 1
                    value: 0
                    onMoved: if (panel.target) panel.target.setGlyphStride(Math.round(value))
                }
            }

            // ── Size ────────────────────────────────────────────────────
            Column {
                width: parent.width
                visible: glyphSwitch.checked
                spacing: 2

                Text {
                    text: qsTr("Size: %1").arg(sizeSlider.value.toFixed(2))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: sizeSlider
                    width: parent.width
                    from: 0.1
                    to: 2.0
                    value: 0.45
                    onMoved: if (panel.target) panel.target.setGlyphScale(value)
                }
            }

            // ── Sharpness ───────────────────────────────────────────────
            Column {
                width: parent.width
                visible: glyphSwitch.checked
                spacing: 2

                Text {
                    text: qsTr("Shape sharpness: %1").arg(gammaSlider.value.toFixed(1))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: gammaSlider
                    width: parent.width
                    from: 0.5
                    to: 6.0
                    value: 3.0
                    onMoved: if (panel.target) panel.target.setGlyphSharpness(value)
                }
            }

            // ── Colour ──────────────────────────────────────────────────
            Row {
                width: parent.width
                visible: glyphSwitch.checked || streamSwitch.checked
                spacing: 10

                Text {
                    text: qsTr("Colour:")
                    color: "#c6c6c6"
                    font.pixelSize: 14
                    font.family: panel.bodyFont
                    anchors.verticalCenter: parent.verticalCenter
                }

                ComboBox {
                    id: colorCombo
                    width: 180
                    // Order must match GlyphColorMode in tensorglyphbuilder.h.
                    model: [ qsTr("Principal value (signed)"),
                             qsTr("Sign pattern"),
                             qsTr("Equivalent"),
                             qsTr("Field component") ]
                    onActivated: if (panel.target) panel.target.setGlyphColorMode(currentIndex)
                }
            }

            // ── Slice ───────────────────────────────────────────────────
            Row {
                width: parent.width
                visible: glyphSwitch.checked || streamSwitch.checked
                spacing: 10

                Text {
                    text: qsTr("Slice:")
                    color: "#c6c6c6"
                    font.pixelSize: 14
                    font.family: panel.bodyFont
                    anchors.verticalCenter: parent.verticalCenter
                }

                ComboBox {
                    id: sliceCombo
                    width: 110
                    model: [ qsTr("None"), qsTr("X"), qsTr("Y"), qsTr("Z") ]
                    onActivated: if (panel.target)
                        panel.target.setGlyphSlice(currentIndex - 1, Math.round(sliceSlider.value))
                }
            }

            Column {
                width: parent.width
                visible: glyphSwitch.checked && sliceCombo.currentIndex > 0
                spacing: 2

                Text {
                    text: qsTr("Slice index: %1").arg(Math.round(sliceSlider.value))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: sliceSlider
                    width: parent.width
                    from: 0
                    to: Math.max(0, panel.gridSize - 1)
                    stepSize: 1
                    value: Math.floor(panel.gridSize / 2)
                    onMoved: if (panel.target)
                        panel.target.setGlyphSlice(sliceCombo.currentIndex - 1, Math.round(value))
                }
            }

            // ── Hyperstreamlines ────────────────────────────────────────
            // Integration is unbounded in the worst case, so this runs off the
            // main thread; the busy indicator is the only sign it is working.
            Rectangle {
                width: parent.width
                height: 1
                color: "#3a3a3a"
                visible: glyphSwitch.checked || streamSwitch.checked
            }

            Row {
                width: parent.width
                spacing: 10

                Switch {
                    id: streamSwitch
                    width: 50
                    height: 20
                    scale: 0.7
                    display: AbstractButton.IconOnly
                    checked: false
                    onCheckedChanged: if (panel.target) panel.target.setShowStreamlines(checked)
                }

                Text {
                    color: "#d9d9d9"
                    text: qsTr("Hyperstreamlines")
                    font.pixelSize: 14
                    font.family: panel.titleFont
                    anchors.verticalCenter: parent.verticalCenter
                }

                BusyIndicator {
                    running: panel.target ? panel.target.streamlinesBusy : false
                    visible: running
                    width: 18; height: 18
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                visible: streamSwitch.checked
                text: qsTr("%1 lines").arg(panel.target ? panel.target.streamlineCount : 0)
                color: "#9e9e9e"
                font.pixelSize: 11
                font.family: panel.bodyFont
            }

            Column {
                width: parent.width
                visible: streamSwitch.checked
                spacing: 2

                Text {
                    text: qsTr("Seed spacing: %1 voxels").arg(Math.round(seedSlider.value))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: seedSlider
                    width: parent.width
                    from: 2
                    to: 24
                    stepSize: 1
                    value: 8
                    onMoved: if (panel.target) panel.target.setStreamlineSeedStride(Math.round(value))
                }
            }

            Column {
                width: parent.width
                visible: streamSwitch.checked
                spacing: 2

                Text {
                    text: qsTr("Tube radius: %1").arg(tubeSlider.value.toFixed(2))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: tubeSlider
                    width: parent.width
                    from: 0.05
                    to: 1.2
                    value: 0.35
                    onMoved: if (panel.target) panel.target.setStreamlineTubeRadius(value)
                }
            }

            Column {
                width: parent.width
                visible: streamSwitch.checked
                spacing: 2

                // Below this the major principal direction is not determined by
                // the tensor, so the curve stops rather than drawing an
                // orientation the data does not contain.
                Text {
                    text: qsTr("Stop below linearity: %1").arg(clSlider.value.toFixed(2))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: clSlider
                    width: parent.width
                    from: 0.0
                    to: 0.8
                    value: 0.15
                    onMoved: if (panel.target) panel.target.setStreamlineMinLinearity(value)
                }
            }

            Column {
                width: parent.width
                visible: streamSwitch.checked
                spacing: 2

                Text {
                    text: qsTr("Integration step: %1 voxels").arg(stepSlider.value.toFixed(2))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: stepSlider
                    width: parent.width
                    from: 0.05
                    to: 1.0
                    value: 0.25
                    onMoved: if (panel.target) panel.target.setStreamlineStep(value)
                }
            }

            // ── Voxel opacity ───────────────────────────────────────────
            // Renderer-side only: fades the voxel block so overlay geometry
            // buried inside it is visible. Costs nothing to drag -- no geometry
            // is rebuilt, only a uniform changes.
            Column {
                width: parent.width
                visible: glyphSwitch.checked || streamSwitch.checked
                spacing: 2

                Text {
                    text: qsTr("Voxel opacity: %1").arg(opacitySlider.value.toFixed(2))
                    color: "#9e9e9e"
                    font.pixelSize: 11
                    font.family: panel.bodyFont
                }

                Slider {
                    id: opacitySlider
                    width: parent.width
                    from: 0.0
                    to: 1.0
                    value: 1.0
                    onMoved: if (panel.target) panel.target.setVoxelOpacity(value)
                }
            }
        }
    }
}
