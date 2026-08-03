import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Window {
    id: stressAnalysisView
    width: 560
    height: 700
    minimumWidth: 480
    minimumHeight: 600
    color: "#282828"
    title: qsTr("Stress Analysis")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    property var ctrl: stressAnalysisController

    // name -> unit strain direction (pipeline order exx,eyy,ezz,exy,eyz,exz); null = "Custom"
    property var presets: [
        { name: qsTr("Custom"),     dir: null },
        { name: qsTr("Uniaxial X"), dir: [1,0,0,0,0,0] },
        { name: qsTr("Uniaxial Y"), dir: [0,1,0,0,0,0] },
        { name: qsTr("Uniaxial Z"), dir: [0,0,1,0,0,0] },
        { name: qsTr("Shear XY"),   dir: [0,0,0,1,0,0] },
        { name: qsTr("Shear YZ"),   dir: [0,0,0,0,1,0] },
        { name: qsTr("Shear XZ"),   dir: [0,0,0,0,0,1] }
    ]

    function applyPreset(idx) {
        var p = presets[idx];
        if (!p || !p.dir) return;
        var amp = parseFloat(strainAmpField.text) || 1e-4;
        exxField.text = String(p.dir[0] * amp);
        eyyField.text = String(p.dir[1] * amp);
        ezzField.text = String(p.dir[2] * amp);
        exyField.text = String(p.dir[3] * amp);
        eyzField.text = String(p.dir[4] * amp);
        exzField.text = String(p.dir[5] * amp);
    }

    function currentEps() {
        return [
            parseFloat(exxField.text) || 0,
            parseFloat(eyyField.text) || 0,
            parseFloat(ezzField.text) || 0,
            parseFloat(exyField.text) || 0,
            parseFloat(eyzField.text) || 0,
            parseFloat(exzField.text) || 0
        ];
    }

    function fmt(v) {
        if (v === undefined || v === null) return "0";
        if (Math.abs(v) >= 10000 || (Math.abs(v) < 0.001 && v !== 0))
            return v.toExponential(3);
        return parseFloat(v.toPrecision(5)).toString();
    }

    function currentSolver() { return ansysRadio.checked ? "ansys" : "fft"; }

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    Component.onCompleted: {
        presetBox.currentIndex = 1;
        applyPreset(1);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

            // ── Analysis type ────────────────────────────────────────────
            RowLayout {
                spacing: 16
                Label { text: qsTr("Analysis type:"); color: "#CFCECE"; font.family: montserrat.name; font.pixelSize: 15 }
                RadioButton { id: mechanicalRadio; text: qsTr("Mechanical"); checked: true; font.family: inter.name }
                RadioButton {
                    id: thermalRadio
                    text: qsTr("Thermal")
                    enabled: false
                    font.family: inter.name
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Not implemented yet")
                }
            }

            // ── Solver ───────────────────────────────────────────────────
            RowLayout {
                spacing: 16
                Label { text: qsTr("Solver:"); color: "#CFCECE"; font.family: montserrat.name; font.pixelSize: 15 }
                RadioButton { id: fftRadio;   text: qsTr("FFT");   checked: true; font.family: inter.name }
                RadioButton { id: ansysRadio; text: qsTr("ANSYS"); font.family: inter.name }
            }

            // ── Mode ─────────────────────────────────────────────────────
            RowLayout {
                spacing: 16
                Label { text: qsTr("Mode:"); color: "#CFCECE"; font.family: montserrat.name; font.pixelSize: 15 }
                RadioButton { id: singleShotRadio; text: qsTr("Single load case"); checked: true; font.family: inter.name }
                RadioButton { id: datasetRadio;    text: qsTr("Build dataset");    font.family: inter.name }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#3a3a3a" }

            // ── Single load case panel ──────────────────────────────────
            ColumnLayout {
                id: singleShotPanel
                visible: singleShotRadio.checked
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    spacing: 10
                    Label { text: qsTr("Preset:"); color: "#CFCECE"; font.family: inter.name }
                    ComboBox {
                        id: presetBox
                        Layout.preferredWidth: 180
                        model: presets.map(function(p) { return p.name })
                        onActivated: applyPreset(currentIndex)
                    }
                    Label { text: qsTr("Strain amplitude:"); color: "#CFCECE"; font.family: inter.name }
                    TextField {
                        id: strainAmpField
                        Layout.preferredWidth: 90
                        text: "1e-4"
                        validator: DoubleValidator {}
                        onEditingFinished: applyPreset(presetBox.currentIndex)
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 8
                    Layout.fillWidth: true

                    Label { text: "exx"; color: "#9e9e9e"; font.family: inter.name }
                    TextField { id: exxField; text: "0.0001"; Layout.fillWidth: true; validator: DoubleValidator {} }
                    Label { text: "exy"; color: "#9e9e9e"; font.family: inter.name }
                    TextField { id: exyField; text: "0"; Layout.fillWidth: true; validator: DoubleValidator {} }

                    Label { text: "eyy"; color: "#9e9e9e"; font.family: inter.name }
                    TextField { id: eyyField; text: "0"; Layout.fillWidth: true; validator: DoubleValidator {} }
                    Label { text: "eyz"; color: "#9e9e9e"; font.family: inter.name }
                    TextField { id: eyzField; text: "0"; Layout.fillWidth: true; validator: DoubleValidator {} }

                    Label { text: "ezz"; color: "#9e9e9e"; font.family: inter.name }
                    TextField { id: ezzField; text: "0"; Layout.fillWidth: true; validator: DoubleValidator {} }
                    Label { text: "exz"; color: "#9e9e9e"; font.family: inter.name }
                    TextField { id: exzField; text: "0"; Layout.fillWidth: true; validator: DoubleValidator {} }
                }

                RowLayout {
                    spacing: 10
                    Button {
                        text: qsTr("Run")
                        enabled: !ctrl.isRunning
                        onClicked: ctrl.runSingleShot(currentSolver(), currentEps())
                    }
                    Button {
                        text: qsTr("Save to HDF5")
                        enabled: ctrl.canSave && !ctrl.isRunning
                        onClicked: ctrl.saveSingleShotResult()
                    }
                    BusyIndicator {
                        running: ctrl.isRunning
                        visible: ctrl.isRunning
                        implicitWidth: 24
                        implicitHeight: 24
                    }
                }

                ColumnLayout {
                    visible: ctrl.hasResult
                    spacing: 4
                    Layout.topMargin: 6

                    Label { text: qsTr("Result"); font.bold: true; color: "#CFCECE"; font.family: montserrat.name }
                    Label {
                        text: qsTr("sx=%1  sy=%2  sz=%3 Pa")
                              .arg(fmt(ctrl.resultStress[0])).arg(fmt(ctrl.resultStress[1])).arg(fmt(ctrl.resultStress[2]))
                        color: "#CFCECE"; font.family: inter.name
                    }
                    Label {
                        text: qsTr("sxy=%1  syz=%2  sxz=%3 Pa")
                              .arg(fmt(ctrl.resultStress[3])).arg(fmt(ctrl.resultStress[4])).arg(fmt(ctrl.resultStress[5]))
                        color: "#CFCECE"; font.family: inter.name
                    }
                    Label {
                        text: qsTr("von Mises: %1 Pa").arg(fmt(ctrl.resultVonMises))
                        color: "#4fc3f7"; font.bold: true; font.family: inter.name
                    }
                    Label {
                        visible: ctrl.resultIterations > 0
                        text: qsTr("iterations: %1   equilibrium error: %2").arg(ctrl.resultIterations).arg(fmt(ctrl.resultError))
                        color: "#969696"; font.pixelSize: 12; font.family: inter.name
                    }
                    Label {
                        visible: ctrl.hasResult
                        text: qsTr("Component/deformed-shape controls for the 3D view are in the main window.")
                        color: "#7a7a7a"; font.pixelSize: 11; font.family: inter.name
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // ── Dataset build panel ──────────────────────────────────────
            ColumnLayout {
                id: datasetPanel
                visible: datasetRadio.checked
                Layout.fillWidth: true
                spacing: 12

                GridLayout {
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 8
                    Layout.fillWidth: true

                    Label { text: qsTr("Samples (phase 2.0):"); color: "#CFCECE"; font.family: inter.name }
                    TextField {
                        Layout.preferredWidth: 120
                        text: ctrl.numSamples
                        validator: IntValidator { bottom: 1 }
                        onEditingFinished: ctrl.numSamples = parseInt(text)
                    }

                    Label { text: qsTr("Calibration loads (phase 1.5):"); color: "#CFCECE"; font.family: inter.name }
                    TextField {
                        Layout.preferredWidth: 120
                        text: ctrl.numCalib
                        validator: IntValidator { bottom: 1 }
                        onEditingFinished: ctrl.numCalib = parseInt(text)
                    }

                    Label { text: qsTr("Strain amplitude:"); color: "#CFCECE"; font.family: inter.name }
                    TextField {
                        Layout.preferredWidth: 120
                        text: ctrl.strainVal
                        validator: DoubleValidator { bottom: 0 }
                        onEditingFinished: ctrl.strainVal = parseFloat(text)
                    }
                }

                RowLayout {
                    spacing: 10
                    Button {
                        text: qsTr("Run")
                        enabled: !ctrl.isRunning
                        onClicked: ctrl.runDataset(currentSolver())
                    }
                    BusyIndicator {
                        running: ctrl.isRunning
                        visible: ctrl.isRunning
                        implicitWidth: 24
                        implicitHeight: 24
                    }
                    Label {
                        text: qsTr("Progress is logged to the Console panel.")
                        color: "#969696"; font.pixelSize: 12; font.family: inter.name
                    }
                }
            }

            Label {
                visible: ctrl.lastErrorMessage.length > 0
                text: ctrl.lastErrorMessage
                color: "#ef5350"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.family: inter.name
            }

            Item { Layout.preferredHeight: 20 }
    }
}
