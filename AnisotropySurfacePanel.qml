// ============================================================================
//  AnisotropySurfacePanel.qml
//
//  Side panel for MaterialDatabaseView: a live directional-elasticity surface
//  for the selected material, plus the numbers that go with it.
//
//  The matrix binding reads dbManager.revision even though it does not use the
//  value -- a plain Q_INVOKABLE call is not a tracked dependency, so without
//  that read the surface would not refresh when a cell is edited. Same trick as
//  the colormap legend in MainWindow.qml.
// ============================================================================
import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

import OpenGLUnderQML 1.0

Rectangle {
    id: panel

    /// Row in the material table to visualize.
    property int selectedRow: -1

    // Palette, supplied by the host window so the light/dark switch is decided
    // in one place. Defaults reproduce the original dark styling, so the panel
    // still looks right if instantiated without them.
    property bool  darkTheme:   true
    property color panelBg:     "#282828"
    property color viewportBg:  "#1e1e1e"
    property color borderColor: "#3a3a3a"
    property color textStrong:  "#CFCECE"
    property color textBody:    "#c6c6c6"
    property color textDim:     "#9e9e9e"
    property color textFaint:   "#6a6a6a"

    color: panelBg
    border.color: borderColor
    border.width: 1

    Material.theme: panel.darkTheme ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    FontLoader { id: interFont; source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }

    readonly property bool usingSolver: sourceCombo.currentIndex === 1
    readonly property bool componentMode:
        quantityCombo.currentIndex === 8 || quantityCombo.currentIndex === 9

    // A component that names a single axis (C11, C22, C33) reduces to the
    // n-n-n-n contraction, so the twist about the plotted direction cannot
    // change it. Showing a spin slider there would invite the user to drag a
    // control that does nothing. Mirrors mvt::componentIsSpinFree().
    readonly property bool componentSpinFree:
        iSpin.value === jSpin.value && iSpin.value <= 3

    function fmt(v, digits) {
        if (v === undefined || v === null || isNaN(v)) return "-"
        if (Math.abs(v) >= 100000 || (Math.abs(v) < 0.001 && v !== 0))
            return v.toExponential(3)
        return parseFloat(v.toPrecision(digits === undefined ? 5 : digits)).toString()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ── Header ────────────────────────────────────────────────────────
        Text {
            text: qsTr("Elastic anisotropy")
            color: panel.textStrong
            font.family: interFont.name
            font.pixelSize: 16
            font.weight: Font.Bold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Source:")
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            ComboBox {
                id: sourceCombo
                Layout.fillWidth: true
                font.pixelSize: 13
                model: [ qsTr("Database material"), qsTr("Homogenized C (last run)") ]
                delegate: ItemDelegate {
                    width: sourceCombo.width
                    text: modelData
                    font.pixelSize: 13
                    enabled: index === 0 || stressAnalysisController.hasStiffness
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: panel.usingSolver
                  ? qsTr("Effective stiffness of the last solved RVE")
                  : (dbManager.materialNameAt(panel.selectedRow) !== ""
                     ? dbManager.materialNameAt(panel.selectedRow)
                     : qsTr("Select a row in the table"))
            color: panel.textDim
            font.family: interFont.name
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        // ── The surface ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            color: panel.viewportBg
            border.color: panel.borderColor
            border.width: 1
            clip: true

            StiffnessSurfaceItem {
                id: surface
                anchors.fill: parent
                anchors.margins: 1

                // dbManager.revision is read purely to make this binding
                // re-evaluate after a cell edit; see the file header.
                matrix: {
                    dbManager.revision;
                    if (panel.usingSolver)
                        return stressAnalysisController.hasStiffness
                               ? stressAnalysisController.stiffnessC : []
                    return panel.selectedRow >= 0
                           ? dbManager.elasticMatrix(panel.selectedRow) : []
                }
                matrixBasis: panel.usingSolver ? 1 : 0

                quantity:   quantityCombo.currentIndex
                componentI: iSpin.value - 1
                componentJ: jSpin.value - 1
                spinDeg:    spinSlider.value
                twistMode:  twistCombo.currentIndex
                wireframe:  wireframeSwitch.checked

                // The GL viewport clears itself, so the Rectangle behind this
                // item never shows through -- the clear colour has to be told
                // about the theme separately.
                backgroundColor: panel.viewportBg
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 30
                visible: !surface.valid
                text: surface.errorMessage
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: 6
                visible: surface.valid
                text: qsTr("drag to rotate · wheel to zoom")
                color: panel.textFaint
                font.family: interFont.name
                font.pixelSize: 10
            }

            // Overlay tools, top-right of the viewport: reset view, save a PNG,
            // copy to the clipboard. ExportController takes any QQuickItem and
            // grabs it via QQuickItem::grabToImage(), which is the only correct
            // way to capture a threaded-render FBO item, so the surface needs no
            // capture code of its own.
            Row {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                spacing: 2

                Repeater {
                    model: [
                        { glyph: "⟲", tip: qsTr("Reset view"),        action: "reset" },
                        { glyph: "🖫", tip: qsTr("Save image as PNG"), action: "save"  },
                        { glyph: "⧉", tip: qsTr("Copy image to clipboard"), action: "copy" }
                    ]

                    MouseArea {
                        required property var modelData
                        width: 22; height: 22
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        enabled: modelData.action === "reset" || surface.valid
                        opacity: enabled ? 1.0 : 0.35
                        ToolTip.visible: containsMouse
                        ToolTip.text: modelData.tip
                        onClicked: {
                            if (modelData.action === "reset")     surface.resetView()
                            else if (modelData.action === "save") exportController.saveAsImage(surface)
                            else                                  exportController.copyToClipboard(surface)
                        }
                        Text {
                            anchors.centerIn: parent
                            text: parent.modelData.glyph
                            color: panel.textDim
                            font.pixelSize: 15
                        }
                    }
                }
            }
        }

        // ── Quantity ──────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Plot:")
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            ComboBox {
                id: quantityCombo
                Layout.fillWidth: true
                font.pixelSize: 13
                // Called on the instance, not the type: a static Q_INVOKABLE is
                // not reachable through the type name for a non-singleton.
                model: surface.quantityNames()
            }
        }

        // Component indices. Labelled with the axis pair on purpose: the
        // pipeline 6-vector used elsewhere in this codebase orders the shear
        // terms xy, yz, xz, so a bare "4" would be ambiguous.
        RowLayout {
            Layout.fillWidth: true
            visible: panel.componentMode
            spacing: 6
            Text {
                text: qsTr("Index i, j:")
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            SpinBox { id: iSpin; from: 1; to: 6; value: 1; font.pixelSize: 13 }
            SpinBox { id: jSpin; from: 1; to: 6; value: 1; font.pixelSize: 13 }
        }
        Text {
            Layout.fillWidth: true
            visible: panel.componentMode
            text: qsTr("1=xx  2=yy  3=zz  4=yz  5=xz  6=xy  (Voigt order, as in the table)")
            color: panel.textFaint
            font.family: interFont.name
            font.pixelSize: 10
        }

        // What the radius actually means. Without this the shear surfaces read
        // as arbitrary: it is not obvious that C44 is plotted about the normal
        // of its shear plane rather than along an axis of it.
        Text {
            Layout.fillWidth: true
            visible: panel.componentMode
            text: panel.componentSpinFree
                  ? qsTr("Radius = the component with its own axis along that direction, "
                       + "so C11, C22 and C33 give the same surface.")
                  : qsTr("Radius = the component with the crystal carried to that direction "
                       + "by a twist-free rotation, pivoted on the axis the component does "
                       + "not name. Along that pivot axis the value equals the table entry.")
            color: panel.textFaint
            font.family: interFont.name
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }

        // How the free rotation about the plotted direction is resolved. Only
        // meaningful for components naming two or more axes -- and for those it
        // is not cosmetic: a fixed twist violates the crystal's own symmetry by
        // as much as the whole range of the quantity, so "Fixed" is an
        // inspection tool, not a default.
        RowLayout {
            Layout.fillWidth: true
            visible: panel.componentMode && !panel.componentSpinFree
            spacing: 8
            Text {
                text: qsTr("Twist:")
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            ComboBox {
                id: twistCombo
                Layout.fillWidth: true
                font.pixelSize: 13
                // Order must match mvsurf::TwistMode.
                model: [ qsTr("Mean over twist"), qsTr("Min over twist"),
                         qsTr("Max over twist"),  qsTr("Fixed twist angle") ]
                currentIndex: 0
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panel.componentMode && !panel.componentSpinFree
                     && twistCombo.currentIndex === 3
            spacing: 6
            Text {
                text: qsTr("Angle:")
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            Slider {
                id: spinSlider
                Layout.fillWidth: true
                from: 0; to: 360; value: 0
            }
            Text {
                text: Math.round(spinSlider.value) + "°"
                color: panel.textDim
                font.family: interFont.name
                font.pixelSize: 12
            }
        }

        Text {
            Layout.fillWidth: true
            visible: panel.componentMode && !panel.componentSpinFree
            text: {
                if (twistCombo.currentIndex === 3)
                    return qsTr("Warning: a single twist angle cannot be chosen consistently over "
                              + "a whole sphere, so this surface carries a frame artefact — for a "
                              + "cubic crystal it breaks the crystal's own symmetry. Use it to "
                              + "inspect frame dependence, not to read off a shape.")
                if (twistCombo.currentIndex === 0)
                    return qsTr("Average over all twists about each direction. Depends on the "
                              + "direction alone, so it respects the crystal symmetry exactly.")
                return qsTr("Extreme value over all twists about each direction — the stiffness "
                          + "counterpart of the G min/max quantities. Depends on the direction "
                          + "alone, so C44, C55 and C66 coincide in this mode.")
            }
            color: twistCombo.currentIndex === 3 ? "#e0a04a" : panel.textFaint
            font.family: interFont.name
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }

        // ── Readouts ──────────────────────────────────────────────────────
        // Written out rather than factored into inline components: those
        // cannot reliably reach an outer-scope id such as interFont without
        // "pragma ComponentBehavior: Bound".
        GridLayout {
            id: readouts
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 3
            visible: surface.valid

            Text {
                text: qsTr("min")
                color: panel.textDim; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.minValue) + " " + surface.unit
                color: panel.textStrong; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("max")
                color: panel.textDim; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.maxValue) + " " + surface.unit
                color: panel.textStrong; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("anisotropy (max/min)")
                color: panel.textDim; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.anisotropyRatio, 4)
                color: panel.textStrong; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("Zener ratio")
                visible: surface.isCubic
                color: panel.textDim; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.zener, 4)
                visible: surface.isCubic
                color: panel.textStrong; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("bulk modulus (VRH)")
                color: panel.textDim; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.bulkModulus) + " GPa"
                color: panel.textStrong; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }
        }

        Text {
            Layout.fillWidth: true
            visible: surface.valid && !surface.isCubic
            text: qsTr("Matrix is not cubic — Zener ratio does not apply.")
            color: panel.textFaint
            font.family: interFont.name
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }

        // ── Display options ───────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Switch {
                id: wireframeSwitch
                scale: 0.7
                display: AbstractButton.IconOnly
                implicitWidth: 50; implicitHeight: 20
            }
            Text {
                text: qsTr("Wireframe")
                color: panel.textBody
                font.family: interFont.name
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: paletteCombo
                Layout.preferredWidth: 120
                font.pixelSize: 12
                model: [ qsTr("Rainbow"), qsTr("Cool-Warm"), qsTr("Red-Blue"),
                         qsTr("Viridis"), qsTr("Grayscale") ]
                currentIndex: 3
                onActivated: surface.palette = currentIndex
            }
        }
    }
}
