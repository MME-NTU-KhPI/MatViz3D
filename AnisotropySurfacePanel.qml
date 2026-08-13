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

    color: "#282828"
    border.color: "#3a3a3a"
    border.width: 1

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    FontLoader { id: interFont; source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }

    readonly property bool usingSolver: sourceCombo.currentIndex === 1
    readonly property bool componentMode:
        quantityCombo.currentIndex === 8 || quantityCombo.currentIndex === 9

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
            color: "#CFCECE"
            font.family: interFont.name
            font.pixelSize: 16
            font.weight: Font.Bold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Source:")
                color: "#9e9e9e"
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
            color: "#7a7a7a"
            font.family: interFont.name
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        // ── The surface ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            color: "#1e1e1e"
            border.color: "#3a3a3a"
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
                extremumOverSpin: spinExtremumSwitch.checked
                wireframe:  wireframeSwitch.checked
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 30
                visible: !surface.valid
                text: surface.errorMessage
                color: "#9e9e9e"
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
                color: "#6a6a6a"
                font.family: interFont.name
                font.pixelSize: 10
            }

            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                width: 22; height: 22
                cursorShape: Qt.PointingHandCursor
                onClicked: surface.resetView()
                ToolTip.visible: containsMouse
                ToolTip.text: qsTr("Reset view")
                hoverEnabled: true
                Text {
                    anchors.centerIn: parent
                    text: "⟲"
                    color: "#9e9e9e"
                    font.pixelSize: 15
                }
            }
        }

        // ── Quantity ──────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Plot:")
                color: "#9e9e9e"
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
                color: "#9e9e9e"
                font.family: interFont.name
                font.pixelSize: 13
            }
            SpinBox { id: iSpin; from: 1; to: 6; value: 1; font.pixelSize: 13 }
            SpinBox { id: jSpin; from: 1; to: 6; value: 1; font.pixelSize: 13 }
        }
        Text {
            Layout.fillWidth: true
            visible: panel.componentMode
            text: qsTr("1=xx  2=yy  3=zz  4=yz  5=xz  6=xy  (Voigt/Mandel order)")
            color: "#6a6a6a"
            font.family: interFont.name
            font.pixelSize: 10
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panel.componentMode && !spinExtremumSwitch.checked
            spacing: 6
            Text {
                text: qsTr("Spin:")
                color: "#9e9e9e"
                font.family: interFont.name
                font.pixelSize: 13
            }
            Slider {
                id: spinSlider
                Layout.fillWidth: true
                from: 0; to: 90; value: 0
            }
            Text {
                text: Math.round(spinSlider.value) + "°"
                color: "#9e9e9e"
                font.family: interFont.name
                font.pixelSize: 12
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panel.componentMode
            spacing: 6
            Switch {
                id: spinExtremumSwitch
                scale: 0.7
                display: AbstractButton.IconOnly
                implicitWidth: 50; implicitHeight: 20
            }
            Text {
                text: qsTr("Extremum over spin")
                color: "#c6c6c6"
                font.family: interFont.name
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter
            }
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
                color: "#7a7a7a"; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.minValue) + " " + surface.unit
                color: "#CFCECE"; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("max")
                color: "#7a7a7a"; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.maxValue) + " " + surface.unit
                color: "#CFCECE"; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("anisotropy (max/min)")
                color: "#7a7a7a"; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.anisotropyRatio, 4)
                color: "#CFCECE"; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("Zener ratio")
                visible: surface.isCubic
                color: "#7a7a7a"; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.zener, 4)
                visible: surface.isCubic
                color: "#CFCECE"; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }

            Text {
                text: qsTr("bulk modulus (VRH)")
                color: "#7a7a7a"; font.family: interFont.name; font.pixelSize: 12
            }
            Text {
                text: panel.fmt(surface.bulkModulus) + " GPa"
                color: "#CFCECE"; font.family: interFont.name; font.pixelSize: 12
                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
            }
        }

        Text {
            Layout.fillWidth: true
            visible: surface.valid && !surface.isCubic
            text: qsTr("Matrix is not cubic — Zener ratio does not apply.")
            color: "#6a6a6a"
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
                color: "#c6c6c6"
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
