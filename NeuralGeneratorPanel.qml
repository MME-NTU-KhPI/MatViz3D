import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The "Neural" tab of the main window's Data panel: target elastic properties
// and the printing material in, a cell made by the network out
// (NeuralGeneratorController). The form is the Selector's: every target is a
// tick box and a slider, the moduli on a logarithmic scale over what the open
// dataset spans (fixed ends without a dataset), plus the density as a target
// and the size of the cell.
//
// "Generate" passes the ticked targets and the material to the diffusion
// network, which writes the cell's code in 50 steps (the button counts them),
// and the decoder turns the code into the cell. The result block under the
// form reports what was asked and obtained, what the request did with the
// targets, and what the printability check says. Without the diffusion weights
// (ctrl.diffusion is false) the tab decodes a random code instead: the targets
// are then collected but the cell does not depend on them.
//
// "Solve and compare" under the result block solves the cell the way the cells
// of the dataset were solved (minutes, can be stopped) and fills the table
// "target / asked / predicted / FFT / deviation" for the ticked targets; the
// density row is there as soon as the cell is made. The "predicted" column is
// the estimate of the property network (ctrl.predictor: models/cell_predictor.bin
// was found), filled as soon as the cell is made; until the cell is solved the
// deviation column shows the estimate's deviation, marked ≈.
//
// Above the buttons the request check (ctrl.checkRequest) lists, while the form
// is being filled in, the targets that cannot hold together: moduli beyond what
// any shape of the material reaches at the asked density, and numbers that
// contradict each other (among them "same in all directions" against an
// anisotropy target that asks for unequal axes or a modulus that changes with
// direction); the targets it names turn red in the form. It warns only:
// Generate sends the request as it is.
Item {
    id: panel
    clip: true

    property var ctrl: neuralGeneratorController
    property var selector: propertySelectorController
    property string fontSans: "Inter"
    property string fontTitle: "Montserrat"

    readonly property color textColor: "#d9d9d9"
    readonly property color subText: "#969696"
    readonly property color accent: "#22c3a6"
    readonly property color warn: "#ffb74d"
    readonly property color danger: "#ef6f62"

    // The sliders' ends: what the open dataset spans for any material the E
    // slider can give; fixed ends when no dataset has been read.
    readonly property var span: selector.ready ? selector.ranges(matE.from, matE.to)
                                               : ({ E: [1, 1000], G: [0.5, 400], K: [1, 1000], nu: [-0.3, 0.5],
                                                    nuMin: [-1, 0.5], rho: [0.05, 0.7], axisRatio: [0.2, 5], shearRatio: [0.2, 5],
                                                    allDir: [1, 10], allDirG: [1, 10], aU: [0, 5], zener: [0.2, 5] })

    // Target values survive switching between one value for all directions and per-axis ones.
    property bool sameInAllDirections: true
    property var targets: ({
        E:    { on: true,  v: 300 }, G:    { on: false, v: 100 }, nu:   { on: false, v: 0.3 },
        K:    { on: false, v: 300 }, nuMin: { on: false, v: 0.0 },
        Ex:   { on: true,  v: 300 }, Ey:   { on: true,  v: 300 }, Ez:   { on: true,  v: 300 },
        Gxy:  { on: false, v: 100 }, Gyz:  { on: false, v: 100 }, Gxz:  { on: false, v: 100 },
        nuXy: { on: false, v: 0.3 }, nuXz: { on: false, v: 0.3 }, nuYz: { on: false, v: 0.3 },
        EzEx: { on: false, v: 1.0 }, EyEx: { on: false, v: 1.0 }, EzEy: { on: false, v: 1.0 },
        GxzGxy: { on: false, v: 1.0 }, GyzGxy: { on: false, v: 1.0 }, GyzGxz: { on: false, v: 1.0 },
        EmaxEmin: { on: false, v: 1.5 }, GmaxGmin: { on: false, v: 1.5 }, AU: { on: false, v: 0.5 },
        zener: { on: false, v: 1.0 },
        rho: { on: true, v: 0.30 }
    })
    readonly property var anisoKeys: ["EzEx", "EyEx", "EzEy", "GxzGxy", "GyzGxy", "GyzGxz",
                                      "EmaxEmin", "GmaxGmin", "AU", "zener"]
    // `targets` is a plain JS object, so ticking a box does not notify; this does.
    property int targetsRevision: 0
    // The same for a moved target slider: the request check below reads it.
    property int formRevision: 0

    // Targets that cannot hold together (ctrl.checkRequest: physical bounds of the material and numbers that
    // contradict each other; no data). Recomputed on every change of the form, microseconds.
    readonly property var issues: {
        formRevision; targetsRevision; panel.sameInAllDirections; matE.value; matNu.value; matR.value;
        return ctrl.checkRequest(condition());
    }
    // The keys of the request the conflicts name, for the labels of the form.
    readonly property var issueKeys: {
        let k = {};
        for (const issue of panel.issues) for (const key of issue.keys) k[key] = true;
        return k;
    }
    // A row of the form against those keys: E, G and ν of "same in all directions" stand for the three axes.
    function rowInIssue(key) {
        const names = key === "E" ? ["Ex", "Ey", "Ez", "Emean"] : key === "G" ? ["Gxy", "Gyz", "Gxz", "Gmean"]
                    : key === "nu" ? ["nuXy", "nuXz", "nuYz"] : key === "rho" ? ["density"] : [key];
        return names.some(n => panel.issueKeys[n] === true);
    }
    // "Same in all directions" means it: E x = E y = E z (G, ν likewise), and the request check flags an
    // anisotropy target that asks otherwise. Unequal axes are set with the box unticked.
    readonly property var isoRows: [
        { key: "E",  label: qsTr("E"), kind: "E",
          tip: qsTr("The same E along every direction: E x = E y = E z. Untick \"same in all directions\" to set the axes one by one") },
        { key: "G",  label: qsTr("G"), kind: "G",
          tip: qsTr("The same shear modulus in every plane: G xy = G yz = G xz") },
        { key: "nu", label: qsTr("ν"),  kind: "nu" }
    ]
    // In both modes, below E, G and ν.
    readonly property var extraRows: [
        { key: "K", label: qsTr("K"), kind: "K",
          tip: qsTr("Bulk modulus: uniform pressure over the volume change it causes (squeezing from all sides at once)") },
        { key: "nuMin", label: qsTr("ν min"), kind: "nuMin",
          tip: qsTr("The smallest Poisson's ratio over every load direction and every direction across it. Below 0 the cell widens when stretched (auxetic)") }
    ]
    readonly property var axisAnisoRows: [
        { key: "EzEx", label: qsTr("E z / E x"), kind: "axisRatio",
          tip: qsTr("E along Z (the build direction) over E along X: 1 = the same, 3 = three times stiffer along Z") },
        { key: "EyEx", label: qsTr("E y / E x"), kind: "axisRatio",
          tip: qsTr("E along Y over E along X: 1 = the same") },
        { key: "EzEy", label: qsTr("E z / E y"), kind: "axisRatio",
          tip: qsTr("E along Z (the build direction) over E along Y: 1 = the same") },
        { key: "GxzGxy", label: qsTr("G xz / G xy"), kind: "shearRatio",
          tip: qsTr("Shear across the layers (plane XZ) over shear in the layer plane (XY): 1 = the same") },
        { key: "GyzGxy", label: qsTr("G yz / G xy"), kind: "shearRatio",
          tip: qsTr("Shear across the layers (plane YZ) over shear in the layer plane (XY): 1 = the same") },
        { key: "GyzGxz", label: qsTr("G yz / G xz"), kind: "shearRatio",
          tip: qsTr("Shear in plane YZ over shear in plane XZ: 1 = the same") }
    ]
    readonly property var allDirRows: [
        { key: "EmaxEmin", label: qsTr("E max / min"), kind: "allDir",
          tip: qsTr("The stiffest direction over the weakest, over all directions, not only the axes: 1 = isotropic") },
        { key: "GmaxGmin", label: qsTr("G max / min"), kind: "allDirG",
          tip: qsTr("The largest shear modulus over the smallest, over every shear plane and every direction in it: 1 = isotropic") },
        { key: "AU", label: qsTr("Aᵁ"), kind: "aU",
          tip: qsTr("Universal anisotropy index 5·G_V/G_R + K_V/K_R − 6 (Ranganathan and Ostoja-Starzewski, 2008): one number for any symmetry, 0 = isotropic, the larger the more the stiffness depends on direction") },
        { key: "zener", label: qsTr("Zener"), kind: "zener",
          tip: qsTr("Zener ratio 2·C44 / (C11 − C12) of a cubic cell: 1 = isotropic, above 1 stiffer along the cube diagonals, below 1 along the axes") }
    ]
    readonly property var axisRows: [
        { key: "Ex", label: qsTr("E x"), kind: "E" }, { key: "Ey", label: qsTr("E y"), kind: "E" },
        { key: "Ez", label: qsTr("E z"), kind: "E" }, { key: "Gxy", label: qsTr("G xy"), kind: "G" },
        { key: "Gyz", label: qsTr("G yz"), kind: "G" }, { key: "Gxz", label: qsTr("G xz"), kind: "G" },
        { key: "nuXy", label: qsTr("ν xy"), kind: "nu", tip: qsTr("Stretched along X, the share it narrows along Y") },
        { key: "nuXz", label: qsTr("ν xz"), kind: "nu", tip: qsTr("Stretched along X, the share it narrows along Z") },
        { key: "nuYz", label: qsTr("ν yz"), kind: "nu", tip: qsTr("Stretched along Y, the share it narrows along Z") }
    ]
    readonly property var densityRows: [
        { key: "rho", label: qsTr("density ρ"), kind: "rho",
          tip: qsTr("The share of the cell's volume filled with material: 0.30 = 30 % solid, 70 % void") }
    ]

    readonly property var report: ctrl.report
    readonly property bool hasReport: report.seed !== undefined

    // The comparison of the last cell with what was asked (ctrl.comparison, ctrl.solveInfo).
    readonly property var solve: ctrl.solveInfo
    readonly property bool solved: solve.state !== undefined
    readonly property var targetLabels: ({
        Ex: qsTr("E x"), Ey: qsTr("E y"), Ez: qsTr("E z"), Gxy: qsTr("G xy"), Gyz: qsTr("G yz"), Gxz: qsTr("G xz"),
        nuXy: qsTr("ν xy"), nuXz: qsTr("ν xz"), nuYz: qsTr("ν yz"), Emean: qsTr("E mean"), Gmean: qsTr("G mean"),
        K: qsTr("K"), nuMin: qsTr("ν min"), EzEx: qsTr("E z / E x"), EyEx: qsTr("E y / E x"), EzEy: qsTr("E z / E y"),
        GxzGxy: qsTr("G xz / G xy"), GyzGxy: qsTr("G yz / G xy"), GyzGxz: qsTr("G yz / G xz"),
        EmaxEmin: qsTr("E max / min"), GmaxGmin: qsTr("G max / min"), AU: qsTr("Aᵁ"), zener: qsTr("Zener"),
        density: qsTr("density ρ")
    })
    // Deviations up to this size are shown as a hit: 10 % of the asked value, 0.05 for a plain difference.
    readonly property real hitPercent: 10
    readonly property real hitDifference: 0.05
    // The deviation of a row: of the solved value, and until the cell is solved of the predictor's estimate (marked ≈).
    function rowDeviation(row) { return isNaN(row.deviation) ? row.predicted_deviation : row.deviation; }
    function deviationText(row) {
        const d = panel.rowDeviation(row);
        if (isNaN(d)) return "-";
        const text = row.relative ? Math.abs(d).toFixed(Math.abs(d) < 10 ? 1 : 0) + " %" : Math.abs(d).toFixed(2);
        return (isNaN(row.deviation) ? "≈ " : "") + (d < 0 ? "−" : "+") + text;
    }
    function deviationColor(row) {
        const d = panel.rowDeviation(row);
        if (isNaN(d)) return panel.subText;
        return Math.abs(d) <= (row.relative ? panel.hitPercent : panel.hitDifference) ? panel.accent : panel.warn;
    }
    // The predictor's estimate of the axis properties that are not rows of the table.
    function otherPredicted() {
        if (!panel.hasReport || panel.report.predicted === undefined) return "";
        const asked = panel.ctrl.comparison.map(r => r.key);
        let parts = [];
        for (const k of ["Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz"])
            if (asked.indexOf(k) < 0 && panel.report.predicted[k] !== undefined) parts.push(panel.targetLabels[k] + " " + panel.fmt(panel.report.predicted[k]));
        return parts.join(" · ");
    }
    // The axis properties the solve gave that are not rows of the table.
    function otherValues() {
        if (!panel.solved) return "";
        const asked = panel.ctrl.comparison.map(r => r.key);
        let parts = [];
        for (const k of ["Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz"])
            if (asked.indexOf(k) < 0) parts.push(panel.targetLabels[k] + " " + panel.fmt(panel.solve.values[k]));
        return parts.join(" · ");
    }

    function fmt(x) {
        const a = Math.abs(x);
        return a >= 100 ? x.toFixed(0) : a >= 10 ? x.toFixed(1) : a >= 1 ? x.toFixed(2) : x.toPrecision(2);
    }
    function log10(x) { return Math.log(x) / Math.LN10; }

    // What the network is asked for; the targets map has the Selector's keys.
    function condition() {
        let t = {};
        function put(key, names) {
            const s = panel.targets[key];
            if (!s.on) return;
            for (const n of names) t[n] = s.v;
        }
        if (panel.sameInAllDirections) {
            // One value for the three axes, whatever the anisotropy targets say (the check flags a contradiction).
            put("E", ["Ex", "Ey", "Ez"]);
            put("G", ["Gxy", "Gyz", "Gxz"]);
            put("nu", ["nuXy", "nuXz", "nuYz"]);
        } else {
            for (const k of ["Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz"]) put(k, [k]);
        }
        put("K", ["K"]);
        put("nuMin", ["nuMin"]);
        for (const k of panel.anisoKeys) put(k, [k]);
        let c = {
            material: { e_gpa: matE.value, nu: matNu.value, aniso: matR.value },
            targets: t,
            same_in_all_directions: panel.sameInAllDirections,
            cell_mm: cellSize.value,
            smooth: codeSmooth.value,
            clean: cleanBox.checked
        };
        if (panel.targets.rho.on) c.density = panel.targets.rho.v;
        return c;
    }
    // seed > 0: that starting noise; 0: the last one again when "same noise again" is ticked, else a new one.
    function generate(seed) {
        let c = condition();
        if (seed > 0) c.seed = seed;
        else if (keepSeed.checked && panel.hasReport) c.seed = panel.report.seed;
        ctrl.generate(c);
    }

    component Caption: Text {
        Layout.fillWidth: true
        Layout.topMargin: 4
        color: panel.subText
        font.family: panel.fontSans
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }
    component RowLabel: Text {
        color: panel.textColor
        font.family: panel.fontSans
        font.pixelSize: 12
    }
    component ValueText: Text {
        Layout.preferredWidth: 64
        horizontalAlignment: Text.AlignRight
        color: panel.textColor
        font.family: panel.fontSans
        font.pixelSize: 12
    }
    component Note: Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: panel.subText
        font.family: panel.fontSans
        font.pixelSize: 11
    }
    // A plain slider row: label, slider, value with its unit.
    component SliderRow: RowLayout {
        id: row
        property alias label: rowLabel.text
        property alias from: rowSlider.from
        property alias to: rowSlider.to
        property alias stepSize: rowSlider.stepSize
        property alias value: rowSlider.value
        property string unit: ""
        property int decimals: 2
        property string tip: ""
        Layout.fillWidth: true
        spacing: 6
        RowLabel { id: rowLabel; Layout.preferredWidth: 100 }
        Slider {
            id: rowSlider
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            topPadding: 0
            bottomPadding: 0
            snapMode: Slider.SnapAlways
            ToolTip.visible: hovered && row.tip !== ""
            ToolTip.delay: 400
            ToolTip.text: row.tip
        }
        ValueText { text: rowSlider.value.toFixed(row.decimals) + (row.unit ? " " + row.unit : "") }
    }

    // One target: tick box, label, slider, value. Moduli and the anisotropy ratios
    // move on a log scale (the slider holds log10 of the value), A^U on the log of
    // 1 + A^U (it starts at 0), the Poisson's ratios and the density plainly.
    component TargetRow: RowLayout {
        id: targetRow
        required property var modelData
        property int labelWidth: 84
        readonly property string kind: modelData.kind
        readonly property bool plain: kind === "nu" || kind === "nuMin" || kind === "rho"
        readonly property bool isModulus: kind === "E" || kind === "G" || kind === "K"
        readonly property var ends: panel.span[kind]
        function toSlider(v) { return plain ? v : kind === "aU" ? panel.log10(1 + v) : panel.log10(v); }
        function fromSlider(x) { return plain ? x : kind === "aU" ? Math.pow(10, x) - 1 : Math.pow(10, x); }
        Layout.fillWidth: true
        spacing: 6
        CheckBox {
            id: useBox
            Layout.preferredWidth: 30
            checked: panel.targets[targetRow.modelData.key].on
            padding: 0
            onToggled: {
                panel.targets[targetRow.modelData.key].on = checked;
                panel.targetsRevision++;
            }
        }
        RowLabel {
            Layout.preferredWidth: targetRow.labelWidth
            text: targetRow.modelData.label
            opacity: useBox.checked ? 1.0 : 0.5
            // Named by a conflict of the request check.
            color: useBox.checked && panel.rowInIssue(targetRow.modelData.key) ? panel.danger : panel.textColor
        }
        Slider {
            id: targetSlider
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            topPadding: 0
            bottomPadding: 0
            enabled: useBox.checked
            from: targetRow.toSlider(targetRow.ends[0])
            to: targetRow.toSlider(targetRow.ends[1])
            stepSize: targetRow.plain ? 0.01 : 0.005
            value: targetRow.toSlider(panel.targets[targetRow.modelData.key].v)
            readonly property real actual: targetRow.fromSlider(value)
            onActualChanged: {
                panel.targets[targetRow.modelData.key].v = actual;
                panel.formRevision++;
            }
            ToolTip.visible: hovered && (targetRow.modelData.tip || "") !== ""
            ToolTip.delay: 400
            ToolTip.text: targetRow.modelData.tip || ""
        }
        ValueText {
            opacity: useBox.checked ? 1.0 : 0.4
            text: panel.fmt(targetSlider.actual) + (targetRow.isModulus ? " MPa" : "")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        // Shown when the weights could not be read (nothing can be decoded then) or a cell could not be made.
        Note {
            visible: panel.ctrl.message !== ""
            text: panel.ctrl.message
            color: panel.warn
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 110
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            // Always shown while the form goes below the fold, so it is clear there is more.
            ScrollBar.vertical.policy: contentHeight > height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

            ColumnLayout {
                // Room on the right for the scroll bar.
                width: scroll.availableWidth - 20
                spacing: 4

                // Without the diffusion weights: a random code, the targets play no role.
                Note {
                    Layout.topMargin: 2
                    visible: !panel.ctrl.diffusion
                    color: panel.warn
                    text: qsTr("Simulation: the cell is decoded from a random code. "
                               + "The targets are collected, but not used: the diffusion weights were not found.")
                    HoverHandler { id: simHover }
                    ToolTip.visible: simHover.hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("The decoder turns a code of 16 × 16 × 16 × 4 numbers into a 64³ cell. Here the code is random "
                                       + "numbers with the mean and spread of the codes of real cells, so the cell is noise, not a design. ")
                                  + panel.ctrl.diffusionMissing
                }
                Note {
                    Layout.topMargin: 2
                    visible: panel.ctrl.diffusion
                    text: qsTr("The diffusion network writes the cell's code from the ticked targets and the material in %1 steps; "
                               + "the decoder turns the code into the cell.").arg(panel.ctrl.steps)
                }
                // What was measured on the cells of this model (written into the weights file at export).
                Note {
                    visible: panel.ctrl.diffusion && panel.ctrl.diffusionNote !== ""
                    color: panel.warn
                    text: panel.ctrl.diffusionNote
                }

                // ── Material ──
                Caption { text: qsTr("Material you print with") }
                SliderRow { id: matE; label: qsTr("E"); from: 0.5; to: 5.0; stepSize: 0.05; value: 2.0; unit: "GPa" }
                SliderRow { id: matNu; label: qsTr("ν"); from: 0.25; to: 0.49; stepSize: 0.01; value: 0.38 }
                SliderRow {
                    id: matR
                    label: qsTr("across ×")
                    from: 0.3; to: 1.0; stepSize: 0.01; value: 1.0
                    tip: qsTr("E across the printed layers (along Z) as a share of E along them; 1 = the same in every direction")
                }

                // ── Targets ──
                RowLayout {
                    Layout.fillWidth: true
                    Caption { text: qsTr("Target properties") }
                    CheckBox {
                        text: qsTr("same in all directions")
                        checked: panel.sameInAllDirections
                        font.family: panel.fontSans
                        font.pixelSize: 11
                        topPadding: 0
                        bottomPadding: 0
                        onToggled: panel.sameInAllDirections = checked
                    }
                }
                Repeater {
                    model: panel.sameInAllDirections ? panel.isoRows : panel.axisRows
                    delegate: TargetRow {}
                }
                Repeater {
                    model: panel.extraRows
                    delegate: TargetRow {}
                }

                // ── Anisotropy ──
                Caption { text: qsTr("Anisotropy along the axes") }
                Repeater {
                    model: panel.axisAnisoRows
                    delegate: TargetRow {}
                }
                Caption { text: qsTr("Anisotropy over all directions") }
                Repeater {
                    model: panel.allDirRows
                    delegate: TargetRow {}
                }

                // ── Density and size ──
                Caption { text: qsTr("Density and cell size") }
                Repeater {
                    model: panel.densityRows
                    delegate: TargetRow {}
                }
                SliderRow {
                    id: cellSize
                    label: qsTr("cell size")
                    from: 6; to: 30; stepSize: 0.5; value: 12; unit: "mm"; decimals: 1
                    tip: qsTr("The edge of the printed cell. The cell is 64 voxels across, so 12 mm gives voxels of 0.19 mm; the printability check measures walls in mm")
                }

                // ── Code ──
                Caption { text: panel.ctrl.diffusion ? qsTr("Starting noise") : qsTr("Random code") }
                SliderRow {
                    id: codeSmooth
                    visible: !panel.ctrl.diffusion
                    label: qsTr("smoothness")
                    from: 0; to: 3; stepSize: 0.1; value: 0; decimals: 1
                    tip: qsTr("0 = every number of the code is drawn on its own. Above 0 the code is blurred over that many of its 16 positions per axis (one position = 4 voxels of the cell), so neighbouring parts of the cell become alike and the pieces get larger")
                }
                CheckBox {
                    id: keepSeed
                    text: panel.ctrl.diffusion
                          ? (panel.hasReport ? qsTr("same noise again (seed %1)").arg(panel.report.seed) : qsTr("same noise again"))
                          : (panel.hasReport ? qsTr("same code again (seed %1)").arg(panel.report.seed) : qsTr("same code again"))
                    enabled: panel.hasReport
                    font.family: panel.fontSans
                    font.pixelSize: 11
                    topPadding: 0
                    bottomPadding: 0
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: panel.ctrl.diffusion
                                  ? qsTr("Start from the same noise again: the same request then gives the same cell, and a changed target shows what that target alone changes")
                                  : qsTr("Decode the same random numbers again, to see what the smoothness or the cell size alone changes")
                }
                CheckBox {
                    id: cleanBox
                    visible: panel.ctrl.diffusion
                    checked: true
                    text: qsTr("remove loose pieces")
                    font.family: panel.fontSans
                    font.pixelSize: 11
                    topPadding: 0
                    bottomPadding: 0
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Keep the largest connected body of the periodic cell and clear the rest. Loose pieces cannot be printed; the report says how much was removed")
                }

                // ── The network ──
                Note {
                    Layout.topMargin: 4
                    visible: panel.ctrl.diffusion
                    text: qsTr("Diffusion: ") + panel.ctrl.diffusionInfo
                }
                Note {
                    Layout.topMargin: panel.ctrl.diffusion ? 0 : 4
                    Layout.bottomMargin: panel.ctrl.predictor ? 0 : 4
                    visible: panel.ctrl.available
                    text: qsTr("Decoder: ") + panel.ctrl.modelInfo
                }
                Note {
                    Layout.bottomMargin: 4
                    visible: panel.ctrl.predictor
                    text: qsTr("Predictor: ") + panel.ctrl.predictorInfo
                    HoverHandler { id: predictorInfoHover }
                    ToolTip.visible: predictorInfoHover.hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("The property network: reads the finished cell and estimates its 24 properties without a solve. ") + panel.ctrl.predictorNote
                }
            }
        }

        // ── The last cell ── (scrolls on its own once it would take more than 45 % of the tab, so the form keeps its room)
        ScrollView {
            id: resultScroll
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.preferredHeight: Math.min(resultColumn.implicitHeight, Math.round(panel.height * 0.45))
            visible: panel.hasReport
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: contentHeight > height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

            ColumnLayout {
                id: resultColumn
                width: resultScroll.availableWidth - 20
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: !panel.hasReport ? "" : panel.report.ok ? qsTr("Printable")
                                                                   : qsTr("Not printable: ") + panel.report.verdict
                    color: panel.hasReport && panel.report.ok ? panel.accent : panel.warn
                    font.family: panel.fontSans
                    font.pixelSize: 12
                }
                Note {
                    readonly property bool shown: panel.hasReport && panel.report.diffusion && panel.report.clipped.length > 0
                    visible: shown
                    color: panel.warn
                    text: !shown ? "" : qsTr("outside the training range, its edge was taken: ") + panel.report.clipped.join(", ")
                }
                Note {
                    readonly property bool shown: panel.hasReport && panel.report.diffusion && panel.report.unknown.length > 0
                    visible: shown
                    color: panel.warn
                    text: !shown ? "" : qsTr("not passed: ") + panel.report.unknown.join(", ")
                }

                // ── Asked and obtained ──
                Caption {
                    visible: panel.ctrl.canCompare
                    text: qsTr("Asked and obtained") + (compareRows.count > 0 ? qsTr(" · moduli in MPa") : "")
                }
                RowLayout {
                    visible: compareRows.count > 0
                    Layout.fillWidth: true
                    spacing: 4
                    Note { text: qsTr("target") }
                    Note { Layout.fillWidth: false; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight; text: qsTr("asked") }
                    Note {
                        visible: panel.ctrl.predictor
                        Layout.fillWidth: false
                        Layout.preferredWidth: 54
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("predicted")
                        HoverHandler { id: predictedHover }
                        ToolTip.visible: predictedHover.hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("The estimate of the property network (%1), read off the finished cell at once. An estimate, not a solve: %2")
                                      .arg(panel.ctrl.predictorInfo).arg(panel.ctrl.predictorNote)
                    }
                    Note {
                        Layout.fillWidth: false
                        Layout.preferredWidth: 46
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("FFT")
                        HoverHandler { id: obtainedHover }
                        ToolTip.visible: obtainedHover.hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("What the cell has: solved by the FFT solver with the settings of the dataset (Solve and compare, minutes). "
                                           + "The density is counted from the voxels at once.")
                    }
                    Note {
                        Layout.fillWidth: false
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("deviation")
                        HoverHandler { id: deviationHover }
                        ToolTip.visible: deviationHover.hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("FFT against asked: in percent of the asked value for the moduli, the anisotropy ratios and the density, "
                                           + "as a plain difference for Poisson's ratios and Aᵁ. Green up to %1 % (up to %2 for a difference). "
                                           + "Until the cell is solved: the predicted value against asked, marked ≈.")
                                      .arg(panel.hitPercent).arg(panel.hitDifference)
                    }
                }
                // A row per ticked target (the stiffness columns are filled once the cell is solved).
                Repeater {
                    id: compareRows
                    model: panel.ctrl.comparison
                    delegate: RowLayout {
                        id: compareRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4
                        RowLabel {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: panel.targetLabels[compareRow.modelData.key] || compareRow.modelData.key
                        }
                        ValueText { Layout.preferredWidth: 46; text: panel.fmt(compareRow.modelData.asked) }
                        ValueText {
                            visible: panel.ctrl.predictor
                            Layout.preferredWidth: 54
                            text: isNaN(compareRow.modelData.predicted) ? "-" : panel.fmt(compareRow.modelData.predicted)
                        }
                        ValueText {
                            Layout.preferredWidth: 46
                            text: isNaN(compareRow.modelData.obtained) ? "-" : panel.fmt(compareRow.modelData.obtained)
                        }
                        ValueText {
                            Layout.preferredWidth: 60
                            text: panel.deviationText(compareRow.modelData)
                            color: panel.deviationColor(compareRow.modelData)
                        }
                    }
                }
                Note {
                    visible: panel.ctrl.canCompare && compareRows.count === 0 && !panel.solved && panel.solve.error === undefined
                    text: qsTr("no target was ticked; the cell can still be solved for its properties")
                }
                // What the predictor said beyond the rows, and which network it is.
                Note {
                    readonly property bool shown: panel.hasReport && panel.report.predicted !== undefined
                    visible: shown
                    text: !shown ? "" : qsTr("predicted by ") + panel.report.predictor + qsTr(" in ") + panel.report.predict_ms.toFixed(0) + qsTr(" ms")
                                        + (panel.otherPredicted() !== "" ? qsTr(" · also predicted, moduli in MPa: ") + panel.otherPredicted() : "")
                    HoverHandler { id: predictorHover }
                    ToolTip.visible: predictorHover.hovered
                    ToolTip.delay: 400
                    ToolTip.text: panel.ctrl.predictorNote
                }
                // What the solve was.
                Note {
                    visible: panel.solved
                    text: !panel.solved ? ""
                          : qsTr("solved in ") + panel.solve.seconds.toFixed(panel.solve.seconds < 100 ? 1 : 0) + qsTr(" s · ") + panel.solve.iterations + qsTr(" iterations · ")
                            + (panel.solve.backend ? qsTr("GPU") : panel.solve.threads + qsTr(" threads")) + qsTr(" · grid ") + panel.solve.grid + qsTr("³ · the solid holds ")
                            + (100 * panel.solve.share).toFixed(1) + qsTr(" % of the strain energy")
                    HoverHandler { id: solveHover }
                    ToolTip.visible: solveHover.hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("The settings of the dataset: six unit loads, conjugate gradients, tolerance 1e-5, the void as a material "
                                       + "%1 times softer than the solid. The share is the smallest over the six loads: near 100 % the void "
                                       + "carries nothing, as it should.").arg(panel.solved ? Math.round(1 / panel.solve.void_eta) : "")
                }
                Note {
                    visible: panel.solved && panel.otherValues() !== ""
                    text: !panel.solved ? "" : qsTr("also obtained, moduli in MPa: ") + panel.otherValues()
                }
                Note {
                    visible: panel.solved && panel.solve.state === "first"
                    color: panel.warn
                    text: !visible ? "" : qsTr("the void was holding part of a load (the solid had %1 % of the strain energy): these values are a first "
                                               + "estimate, the cell is being solved again with a softer void").arg((100 * panel.solve.share_first).toFixed(1))
                }
                Note {
                    visible: panel.solved && panel.solve.state === "stopped"
                    color: panel.warn
                    text: !visible ? "" : qsTr("stopped before the second solve ended: in the first one the void was holding part of a load "
                                               + "(the solid had %1 % of the strain energy), so these values overstate the stiffness")
                                          .arg((100 * panel.solve.share_first).toFixed(1))
                }
                Note {
                    visible: panel.solved && panel.solve.state === "done" && !panel.solve.load_path
                    color: panel.warn
                    text: qsTr("no solid path for one of the loads: the cell does not carry it, the value there is that of the void. "
                               + "The dataset rejects such cells")
                }
                Note {
                    visible: panel.solved && !panel.solve.converged
                    color: panel.warn
                    text: qsTr("the solver stopped at its iteration limit before the tolerance in some load")
                }
                Note {
                    visible: panel.solve.error !== undefined
                    color: panel.warn
                    text: panel.solve.error !== undefined ? qsTr("the solve failed: ") + panel.solve.error : ""
                }

                // ── How the cell came out ──
                Note {
                    Layout.topMargin: 4
                    text: !panel.hasReport ? ""
                          : qsTr("density ") + panel.report.density.toFixed(2)
                            + (panel.report.asked_density !== undefined ? qsTr(" (asked ") + panel.report.asked_density.toFixed(2) + ")" : "")
                            + " · " + panel.report.bodies + (panel.report.bodies === 1 ? qsTr(" piece") : qsTr(" pieces"))
                            + qsTr(" · joined in ") + panel.report.directions + qsTr(" of 3 directions")
                }
                Note {
                    readonly property bool shown: panel.hasReport && panel.report.diffusion && panel.report.removed_pieces > 0
                    visible: shown
                    text: !shown ? "" : (panel.report.removed_pieces === 1 ? qsTr("removed 1 loose piece")
                                                                             : qsTr("removed %1 loose pieces").arg(panel.report.removed_pieces))
                                            + qsTr(" (%1 % of the solid)").arg(panel.report.removed_pct.toFixed(2))
                }
                Note {
                    text: !panel.hasReport ? ""
                          : qsTr("thin walls ") + panel.report.thin_pct.toFixed(1) + " %"
                            + qsTr(" · mean wall ") + panel.report.wall_mm.toFixed(2) + " mm"
                            + qsTr(" · voxel ") + panel.report.voxel_mm.toFixed(2) + " mm"
                }
                Note {
                    text: !panel.hasReport ? ""
                          : panel.report.diffusion
                            ? qsTr("seed ") + panel.report.seed
                              + qsTr(" · generated in ") + (panel.report.sample_ms / 1000).toFixed(1) + qsTr(" s (")
                              + panel.report.steps + qsTr(" steps)")
                              + qsTr(" · decoded in ") + panel.report.decode_ms.toFixed(0) + qsTr(" ms")
                            : qsTr("seed ") + panel.report.seed
                              + qsTr(" · decoded in ") + panel.report.decode_ms.toFixed(0) + qsTr(" ms")
                              + (panel.report.targets > 0 ? qsTr(" · targets not used: ") + panel.report.target_names.join(", ")
                                                          : qsTr(" · no target was ticked"))
                }
                // What the request did with the targets (the diffusion network only).
                Note {
                    readonly property bool shown: panel.hasReport && panel.report.diffusion
                    visible: shown
                    text: !shown ? "" : panel.report.used.length > 0
                                        ? qsTr("targets passed: ") + panel.report.used.join(", ")
                                        : qsTr("no target was ticked: the cell is generated for the material alone")
                }
            }
        }
        // ── Request check ── the targets that cannot hold together, next to the button that sends them.
        ColumnLayout {
            visible: panel.ctrl.available && panel.issues.length > 0
            Layout.fillWidth: true
            Layout.topMargin: 2
            spacing: 2
            Caption {
                text: panel.issues.length === 1 ? qsTr("Conflict in the targets") : qsTr("%1 conflicts in the targets").arg(panel.issues.length)
                color: panel.danger
            }
            Repeater {
                model: panel.issues.slice(0, 3)
                delegate: Note {
                    required property var modelData
                    text: "• " + modelData.text
                    color: panel.danger
                }
            }
            Note {
                visible: panel.issues.length > 3
                text: qsTr("and %1 more; the targets they name are marked in the form").arg(panel.issues.length - 3)
                color: panel.danger
            }
        }
        // While the cell is being solved: where the solver is, and the way out.
        RowLayout {
            visible: panel.ctrl.solving
            Layout.fillWidth: true
            spacing: 6
            Note { text: panel.ctrl.solveProgress }
            Button {
                Layout.preferredHeight: 30
                topInset: 0
                bottomInset: 0
                text: qsTr("Stop")
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: panel.ctrl.cancelCompare()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                topInset: 0
                bottomInset: 0
                highlighted: true
                text: !panel.ctrl.busy ? qsTr("Generate")
                      : !panel.ctrl.diffusion ? qsTr("Decoding…")
                      : panel.ctrl.step > 0 ? qsTr("Generating… step %1 of %2").arg(panel.ctrl.step).arg(panel.ctrl.steps)
                                            : qsTr("Generating…")
                enabled: panel.ctrl.available && !panel.ctrl.busy && !panel.ctrl.solving
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: panel.generate(0)
            }
            // Shown until the last cell has been solved to the end.
            Button {
                visible: panel.ctrl.canCompare && !panel.ctrl.solving && !(panel.solved && panel.solve.state === "done")
                Layout.preferredHeight: 34
                topInset: 0
                bottomInset: 0
                text: qsTr("Solve and compare")
                enabled: !panel.ctrl.busy
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: panel.ctrl.compare()
                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("Solves this cell with the FFT solver the way the cells of the dataset were solved (six loads, the material "
                                   + "set above) and puts what it has next to what was asked. Takes minutes; it can be stopped.")
            }
        }
    }
}
