import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Window {
    id: stressAnalysisView
    width: 620
    height: 760
    minimumWidth: 520
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

    // Reads cell [i][j] from whichever of S/C/P the matrixTab currently selects.
    function currentMatrixCell(i, j) {
        var m = matrixTab.currentIndex === 0 ? ctrl.stiffnessC
              : matrixTab.currentIndex === 1 ? ctrl.stiffnessS
              : ctrl.stiffnessP;
        return (m && m[i]) ? m[i][j] : 0;
    }

    readonly property var seriesColors: ["#4fc3f7", "#81c784", "#ffb74d", "#ba68c8", "#f06292", "#a1887f"]
    readonly property var loadLabels: ["exx", "eyy", "ezz", "exy", "eyz", "exz"]

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    Component.onCompleted: {
        presetBox.currentIndex = 1;
        applyPreset(1);
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: mainColumn.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: mainColumn
        x: 20
        y: 20
        width: parent.width - 40
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
                RadioButton { id: matrixRadio;     text: qsTr("Stiffness matrix (S/C/P)"); font.family: inter.name }
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

            // ── Stiffness matrix panel ──────────────────────────────────
            ColumnLayout {
                id: matrixPanel
                visible: matrixRadio.checked
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    spacing: 10
                    Button {
                        text: qsTr("Compute")
                        enabled: !ctrl.isRunning
                        onClicked: ctrl.runStiffnessMatrix(currentSolver())
                    }
                    BusyIndicator {
                        running: ctrl.isRunning
                        visible: ctrl.isRunning
                        implicitWidth: 24
                        implicitHeight: 24
                    }
                    Label {
                        text: qsTr("6 canonical unit-strain solves → C, S = C⁻¹, P[i][j] = -S[i][j]/S[j][j]")
                        color: "#969696"; font.pixelSize: 12; font.family: inter.name
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                ColumnLayout {
                    visible: ctrl.hasStiffness
                    spacing: 8
                    Layout.topMargin: 4

                    RowLayout {
                        spacing: 6
                        Label { text: qsTr("Matrix:"); color: "#CFCECE"; font.family: inter.name }
                        TabBar {
                            id: matrixTab
                            TabButton { text: "C [Pa]" }
                            TabButton { text: "S [1/Pa]" }
                            TabButton { text: "P" }
                        }
                    }

                    GridLayout {
                        columns: 6
                        columnSpacing: 8
                        rowSpacing: 4
                        Layout.fillWidth: true

                        Repeater {
                            model: 36
                            delegate: Text {
                                text: fmt(currentMatrixCell(Math.floor(index / 6), index % 6))
                                color: (index % 6) === Math.floor(index / 6) ? "#4fc3f7" : "#CFCECE"
                                font.family: "monospace"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 84
                            }
                        }
                    }

                    RowLayout {
                        spacing: 14
                        Layout.topMargin: 6
                        Label { text: qsTr("Ex=%1").arg(fmt(ctrl.stiffnessModuli[0]));  color: "#4fc3f7"; font.pixelSize: 12; font.family: inter.name }
                        Label { text: qsTr("Ey=%1").arg(fmt(ctrl.stiffnessModuli[1]));  color: "#4fc3f7"; font.pixelSize: 12; font.family: inter.name }
                        Label { text: qsTr("Ez=%1").arg(fmt(ctrl.stiffnessModuli[2]));  color: "#4fc3f7"; font.pixelSize: 12; font.family: inter.name }
                        Label { text: qsTr("Gxy=%1").arg(fmt(ctrl.stiffnessModuli[3])); color: "#4fc3f7"; font.pixelSize: 12; font.family: inter.name }
                        Label { text: qsTr("Gyz=%1").arg(fmt(ctrl.stiffnessModuli[4])); color: "#4fc3f7"; font.pixelSize: 12; font.family: inter.name }
                        Label { text: qsTr("Gxz=%1").arg(fmt(ctrl.stiffnessModuli[5])); color: "#4fc3f7"; font.pixelSize: 12; font.family: inter.name }
                    }

                    Label {
                        visible: ctrl.stiffnessIsFFT
                        text: qsTr("FFT: %1 total iterations across the 6 loads. Per-load iterations/error/stress are logged to the Console panel. Live convergence below.")
                              .arg(ctrl.stiffnessTotalIterations)
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

            // ── Live FFT convergence ────────────────────────────────────
            // Shared by the single-shot solve (1 series) and the stiffness-
            // matrix solve (up to 6 series, one per canonical load) -- both
            // feed ctrl.convergencePoints via the same live callback. Hidden
            // for ANSYS: it's a direct FE solve, no per-iteration error.
            ColumnLayout {
                visible: ctrl.convergenceIsFFT && ctrl.convergencePoints.length > 0
                Layout.fillWidth: true
                spacing: 4
                Layout.topMargin: 6

                RowLayout {
                    spacing: 12
                    Label {
                        text: qsTr("FFT convergence (log scale)")
                        font.bold: true; color: "#CFCECE"; font.family: montserrat.name
                    }
                    Label {
                        visible: ctrl.convergenceLoadCount > 1
                        text: {
                            var labels = [];
                            for (var k = 0; k < Math.min(ctrl.convergenceLoadCount, loadLabels.length); ++k)
                                labels.push("<font color='" + seriesColors[k % seriesColors.length] + "'>■</font> " + loadLabels[k]);
                            return labels.join("  ");
                        }
                        textFormat: Text.RichText
                        font.pixelSize: 11; font.family: inter.name
                    }
                }

                Canvas {
                    id: convCanvas
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220

                    Connections {
                        target: ctrl
                        function onConvergenceChanged() { convCanvas.requestPaint(); }
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.fillStyle = "#1e1e1e";
                        ctx.fillRect(0, 0, width, height);

                        var pts = ctrl.convergencePoints;
                        if (!pts || pts.length === 0) return;

                        var margin = { l: 60, r: 10, t: 10, b: 24 };
                        var plotW = Math.max(1, width - margin.l - margin.r);
                        var plotH = Math.max(1, height - margin.t - margin.b);

                        var maxIter = 1;
                        var minLog = 1e300, maxLog = -1e300;
                        var k;
                        for (k = 0; k < pts.length; ++k) {
                            var it = pts[k][1], err = pts[k][2];
                            if (it > maxIter) maxIter = it;
                            var lg = err > 0 ? Math.log(err) / Math.LN10 : -300;
                            if (lg < minLog) minLog = lg;
                            if (lg > maxLog) maxLog = lg;
                        }
                        var tolLog = ctrl.convergenceTol > 0 ? Math.log(ctrl.convergenceTol) / Math.LN10 : minLog;
                        minLog = Math.min(minLog, tolLog) - 0.5;
                        maxLog = Math.max(maxLog, tolLog) + 0.5;
                        if (maxLog <= minLog) maxLog = minLog + 1;

                        function xPix(it) { return margin.l + (maxIter > 0 ? it / maxIter : 0) * plotW; }
                        function yPix(lg)  { return margin.t + (1 - (lg - minLog) / (maxLog - minLog)) * plotH; }

                        ctx.strokeStyle = "#555";
                        ctx.lineWidth = 1;
                        ctx.strokeRect(margin.l, margin.t, plotW, plotH);

                        // horizontal gridlines + log-scale y labels
                        ctx.fillStyle = "#9e9e9e";
                        ctx.font = "10px sans-serif";
                        var ticks = 5, t;
                        for (t = 0; t <= ticks; ++t) {
                            var lgv = minLog + (maxLog - minLog) * t / ticks;
                            var yy = yPix(lgv);
                            ctx.strokeStyle = "#333";
                            ctx.beginPath(); ctx.moveTo(margin.l, yy); ctx.lineTo(margin.l + plotW, yy); ctx.stroke();
                            ctx.fillText("1e" + lgv.toFixed(0), 2, yy + 3);
                        }
                        ctx.fillText(qsTr("iteration"), margin.l + plotW / 2 - 20, height - 6);

                        // tolerance line
                        if (ctrl.convergenceTol > 0) {
                            ctx.strokeStyle = "#ef5350";
                            ctx.setLineDash([4, 3]);
                            var ty = yPix(tolLog);
                            ctx.beginPath();
                            ctx.moveTo(margin.l, ty);
                            ctx.lineTo(margin.l + plotW, ty);
                            ctx.stroke();
                            ctx.setLineDash([]);
                        }

                        // one polyline per load index
                        var series = {};
                        for (k = 0; k < pts.length; ++k) {
                            var li = pts[k][0];
                            if (!series[li]) series[li] = [];
                            series[li].push(pts[k]);
                        }
                        for (var key in series) {
                            var s = series[key];
                            ctx.strokeStyle = seriesColors[key % seriesColors.length];
                            ctx.lineWidth = 2;
                            ctx.beginPath();
                            for (var p = 0; p < s.length; ++p) {
                                var errv = s[p][2];
                                var lgv2 = errv > 0 ? Math.log(errv) / Math.LN10 : minLog;
                                var px = xPix(s[p][1]), py = yPix(lgv2);
                                if (p === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
                            }
                            ctx.stroke();
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 20 }
    }
    }
}
