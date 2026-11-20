import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

// The "Selector" tab of the main window's Data panel: target elastic properties
// and the printing material in, the closest accepted cells of the open dataset
// out (PropertySelectorController). Every number is a slider; the moduli move on
// a logarithmic scale over what the dataset spans for the chosen material, and
// the search reruns a moment after a slider is let go. Below the moduli (with the
// bulk modulus K and the smallest Poisson's ratio over all directions) sit the
// anisotropy targets: the ratios of E and of G between the axes, then E max / E min
// and G max / G min over all directions, the universal anisotropy index A^U and
// the Zener ratio of cubic cells. While one is ticked, E and G in the "same in
// all directions" mode set the level only (the geometric mean over the axes).
// Choosing a result opens that cell in the 3D view and its parameters in
// DatasetCellPanel on the right.
Item {
    id: panel
    clip: true

    property var ctrl: propertySelectorController
    property var browser: datasetBrowserController
    property string fontSans: "Inter"
    property string fontTitle: "Montserrat"
    // Asks the main window to load the open cell's parameters into the generator.
    signal generatorRequested()

    readonly property color textColor: "#d9d9d9"
    readonly property color subText: "#969696"
    readonly property color accent: "#22c3a6"
    readonly property color warn: "#ffb74d"

    // The sliders' ends: what the dataset spans for any material the E slider can
    // give, so moving the material never pushes a target off its slider.
    readonly property var span: ctrl.ready ? ctrl.ranges(matE.from, matE.to)
                                           : ({ E: [1, 1000], G: [0.5, 400], K: [1, 1000], nu: [-0.3, 0.5],
                                                nuMin: [-1, 0.5], rho: [0, 1], axisRatio: [0.2, 5], shearRatio: [0.2, 5],
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
        zener: { on: false, v: 1.0 }
    })
    readonly property var anisoKeys: ["EzEx", "EyEx", "EzEy", "GxzGxy", "GyzGxy", "GyzGxz",
                                      "EmaxEmin", "GmaxGmin", "AU", "zener"]
    // `targets` is a plain JS object, so ticking a box does not notify; this does.
    property int targetsRevision: 0
    readonly property bool anisoActive: {
        targetsRevision;
        return anisoKeys.some(k => targets[k].on);
    }
    readonly property var isoRows: [
        { key: "E",  label: anisoActive ? qsTr("E mean") : qsTr("E"), kind: "E",
          tip: anisoActive ? qsTr("The level of E: the geometric mean of E x, E y and E z (the anisotropy targets below set their ratios)") : "" },
        { key: "G",  label: anisoActive ? qsTr("G mean") : qsTr("G"), kind: "G",
          tip: anisoActive ? qsTr("The level of G: the geometric mean of G xy, G yz and G xz") : "" },
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
          tip: qsTr("Zener ratio 2·C44 / (C11 − C12) of a cubic cell: 1 = isotropic, above 1 stiffer along the cube diagonals, below 1 along the axes. Only cells within 5 % of cubic symmetry are searched") }
    ]
    readonly property var axisRows: [
        { key: "Ex", label: qsTr("E x"), kind: "E" }, { key: "Ey", label: qsTr("E y"), kind: "E" },
        { key: "Ez", label: qsTr("E z"), kind: "E" }, { key: "Gxy", label: qsTr("G xy"), kind: "G" },
        { key: "Gyz", label: qsTr("G yz"), kind: "G" }, { key: "Gxz", label: qsTr("G xz"), kind: "G" },
        { key: "nuXy", label: qsTr("ν xy"), kind: "nu", tip: qsTr("Stretched along X, the share it narrows along Y") },
        { key: "nuXz", label: qsTr("ν xz"), kind: "nu", tip: qsTr("Stretched along X, the share it narrows along Z") },
        { key: "nuYz", label: qsTr("ν yz"), kind: "nu", tip: qsTr("Stretched along Y, the share it narrows along Z") }
    ]

    function fmt(x) {
        const a = Math.abs(x);
        return a >= 100 ? x.toFixed(0) : a >= 10 ? x.toFixed(1) : a >= 1 ? x.toFixed(2) : x.toPrecision(2);
    }
    function log10(x) { return Math.log(x) / Math.LN10; }

    function startIndexIfNeeded() {
        if (panel.visible && ctrl.hasDataset && !ctrl.ready && !ctrl.indexing)
            Qt.callLater(ctrl.buildIndex);
    }
    // The search reruns shortly after the last change, not on every slider step.
    function scheduleSearch() { if (ctrl.ready) searchTimer.restart(); }

    function runSearch() {
        let t = {};
        function put(key, names) {
            const s = panel.targets[key];
            if (!s.on) return;
            for (const n of names) t[n] = s.v;
        }
        if (panel.sameInAllDirections) {
            // With an anisotropy target E and G cannot be the same along every axis: they set the level.
            put("E", panel.anisoActive ? ["Emean"] : ["Ex", "Ey", "Ez"]);
            put("G", panel.anisoActive ? ["Gmean"] : ["Gxy", "Gyz", "Gxz"]);
            put("nu", ["nuXy", "nuXz", "nuYz"]);
        } else {
            for (const k of ["Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz"]) put(k, [k]);
        }
        put("K", ["K"]);
        put("nuMin", ["nuMin"]);
        for (const k of panel.anisoKeys) put(k, [k]);
        ctrl.search({
            material: { e_gpa: matE.value, nu: matNu.value, aniso: matR.value },
            targets: t,
            rho_min: rhoRange.first.value,
            rho_max: rhoRange.second.value,
            family: familyBox.currentValue === undefined ? "" : familyBox.currentValue,
            count: 10
        });
    }

    Timer { id: searchTimer; interval: 250; onTriggered: panel.runSearch() }

    // --select: a search given on the command line. Keys of `spec`: the keys of `targets`
    // (those listed are ticked, the rest unticked; moduli in MPa), mat_e (GPa), mat_nu,
    // mat_r, rho_min, rho_max, axes (1 = per-axis targets), result (the row to show),
    // scroll (pixels to scroll the panel down, -1 = to the list of matches) and, in the
    // main window, console (0 hides it). Waits until the dataset is read.
    property var pendingSpec: null
    function applySpec(spec) {
        panel.pendingSpec = spec;
        if (ctrl.ready) applyPendingSpec(); else startIndexIfNeeded();
    }
    function applyPendingSpec() {
        const spec = panel.pendingSpec;
        if (!spec) return;
        panel.pendingSpec = null;
        if (spec.mat_e !== undefined) matE.value = spec.mat_e;
        if (spec.mat_nu !== undefined) matNu.value = spec.mat_nu;
        if (spec.mat_r !== undefined) matR.value = spec.mat_r;
        if (spec.axes !== undefined) panel.sameInAllDirections = !spec.axes;
        let t = {};
        for (const k in panel.targets)
            t[k] = spec[k] !== undefined ? { on: true, v: spec[k] } : { on: false, v: panel.targets[k].v };
        panel.targets = t;
        panel.targetsRevision++;
        rhoRange.setValues(spec.rho_min !== undefined ? spec.rho_min : rhoRange.from,
                           spec.rho_max !== undefined ? spec.rho_max : rhoRange.to);
        searchTimer.stop();
        panel.runSearch();
        if (ctrl.results.length > 0)
            ctrl.show(Math.min(spec.result !== undefined ? spec.result : 0, ctrl.results.length - 1));
        if (spec.scroll !== undefined) {
            specScroll.to = spec.scroll;
            specScroll.restart();
        }
    }
    // After the list of matches has been laid out.
    Timer {
        id: specScroll
        property real to: 0
        interval: 300
        onTriggered: {
            const f = scroll.contentItem;
            f.contentY = Math.max(0, Math.min(to < 0 ? resultsNote.y - 6 : to, f.contentHeight - f.height));
        }
    }

    onVisibleChanged: startIndexIfNeeded()
    Connections {
        target: panel.ctrl
        function onIndexChanged() {
            panel.startIndexIfNeeded();
            if (panel.ctrl.ready) {
                rhoRange.setValues(rhoRange.from, rhoRange.to);
                panel.scheduleSearch();
                panel.applyPendingSpec();
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Open a dataset folder (manifest.csv + cells/)")
        onAccepted: panel.browser.openUrl(selectedFolder)
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
            onMoved: panel.scheduleSearch()
            ToolTip.visible: hovered && row.tip !== ""
            ToolTip.delay: 400
            ToolTip.text: row.tip
        }
        ValueText { text: rowSlider.value.toFixed(row.decimals) + (row.unit ? " " + row.unit : "") }
    }

    // One target: tick box, label, slider, value. Moduli and the anisotropy ratios
    // move on a log scale (the slider holds log10 of the value), A^U on the log of
    // 1 + A^U (it starts at 0), the Poisson's ratios plainly.
    component TargetRow: RowLayout {
        id: targetRow
        required property var modelData
        property int labelWidth: 84
        readonly property string kind: modelData.kind
        readonly property bool plain: kind === "nu" || kind === "nuMin"
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
                panel.scheduleSearch();
            }
        }
        RowLabel {
            Layout.preferredWidth: targetRow.labelWidth
            text: targetRow.modelData.label
            opacity: useBox.checked ? 1.0 : 0.5
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
            onActualChanged: panel.targets[targetRow.modelData.key].v = actual
            onMoved: panel.scheduleSearch()
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

        // ── Source ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                Layout.fillWidth: true
                text: !panel.ctrl.hasDataset ? qsTr("No dataset open")
                      : panel.ctrl.indexing ? qsTr("Reading cells… ") + panel.ctrl.indexed + " / " + panel.ctrl.toIndex
                      : panel.ctrl.datasetName + "  ·  " + panel.ctrl.cellCount + qsTr(" accepted cells")
                elide: Text.ElideMiddle
                color: panel.subText
                font.family: panel.fontSans
                font.pixelSize: 11
            }
            Button {
                text: panel.ctrl.hasDataset ? qsTr("Refresh") : qsTr("Open folder…")
                Layout.preferredHeight: 30
                topInset: 0
                bottomInset: 0
                leftPadding: 10
                rightPadding: 10
                flat: panel.ctrl.hasDataset
                enabled: !panel.ctrl.indexing
                font.family: panel.fontSans
                font.pixelSize: 12
                onClicked: panel.ctrl.hasDataset ? panel.browser.reload() : folderDialog.open()
            }
        }
        ProgressBar {
            Layout.fillWidth: true
            visible: panel.ctrl.indexing
            from: 0
            to: Math.max(1, panel.ctrl.toIndex)
            value: panel.ctrl.indexed
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            // Always shown while the results go below the fold, so it is clear there is more.
            ScrollBar.vertical.policy: contentHeight > height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

            ColumnLayout {
                // Room on the right for the scroll bar.
                width: scroll.availableWidth - 12
                spacing: 4

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
                        onToggled: { panel.sameInAllDirections = checked; panel.scheduleSearch(); }
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

                // ── Limits ──
                Caption { text: qsTr("Limits") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLabel { text: qsTr("density ρ"); Layout.preferredWidth: 100 }
                    RangeSlider {
                        id: rhoRange
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        topPadding: 0
                        bottomPadding: 0
                        from: Math.floor(panel.span.rho[0] * 20) / 20
                        to: Math.ceil(panel.span.rho[1] * 20) / 20
                        stepSize: 0.01
                        snapMode: RangeSlider.SnapAlways
                        first.onMoved: panel.scheduleSearch()
                        second.onMoved: panel.scheduleSearch()
                    }
                    ValueText { text: rhoRange.first.value.toFixed(2) + "–" + rhoRange.second.value.toFixed(2) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLabel { text: qsTr("family"); Layout.preferredWidth: 100 }
                    ComboBox {
                        id: familyBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        topInset: 0
                        bottomInset: 0
                        font.pixelSize: 12
                        model: panel.ctrl.families
                        textRole: "label"
                        valueRole: "key"
                        onActivated: panel.scheduleSearch()
                    }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    visible: text !== ""
                    text: panel.ctrl.message
                    wrapMode: Text.WordWrap
                    color: panel.subText
                    font.family: panel.fontSans
                    font.pixelSize: 11
                }
                Text {
                    id: resultsNote
                    Layout.fillWidth: true
                    visible: panel.ctrl.results.length > 0
                    text: qsTr("Values below are for your material; the panel on the right shows the cell as it was solved.")
                    wrapMode: Text.WordWrap
                    color: panel.subText
                    font.family: panel.fontSans
                    font.pixelSize: 10
                }

                // ── Results ──
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    visible: panel.ctrl.results.length === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: !panel.ctrl.hasDataset ? qsTr("Open a dataset (Dataset tab or the button above), then set the properties you need.")
                          : panel.ctrl.ready ? qsTr("Tick a target property and move its slider.")
                          : qsTr("Reading the accepted cells…")
                    color: panel.subText
                    font.family: panel.fontSans
                    font.pixelSize: 12
                }
                Repeater {
                    model: panel.ctrl.results
                    delegate: ItemDelegate {
                        id: resultDelegate
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        highlighted: index === panel.ctrl.currentResult
                        topPadding: 6
                        bottomPadding: 6
                        onClicked: panel.ctrl.show(index)
                        background: Rectangle {
                            radius: 6
                            color: resultDelegate.highlighted ? "#3322c3a6" : resultDelegate.hovered ? "#22ffffff" : "#33000000"
                        }

                        contentItem: ColumnLayout {
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: resultDelegate.modelData.id
                                    font.family: "Menlo"
                                    font.pixelSize: 11
                                    color: panel.textColor
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: resultDelegate.modelData.title
                                    elide: Text.ElideRight
                                    color: panel.textColor
                                    font.family: panel.fontSans
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: qsTr("off ") + resultDelegate.modelData.fitPct.toFixed(1) + " %"
                                    color: resultDelegate.modelData.fitPct < 10 ? panel.accent : panel.textColor
                                    font.family: panel.fontSans
                                    font.pixelSize: 11
                                }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                Repeater {
                                    model: resultDelegate.modelData.rows
                                    delegate: Text {
                                        required property var modelData
                                        text: modelData.label + " " + modelData.value + " (" + modelData.deviation + ")"
                                        color: modelData.good ? panel.accent : panel.warn
                                        font.family: panel.fontSans
                                        font.pixelSize: 10
                                    }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: resultDelegate.modelData.anisotropy
                                wrapMode: Text.WordWrap
                                color: panel.subText
                                font.family: panel.fontSans
                                font.pixelSize: 10
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "ρ " + resultDelegate.modelData.density.toFixed(2)
                                      + " · " + resultDelegate.modelData.cellMm.toFixed(1) + " mm"
                                      + (resultDelegate.modelData.wallMm > 0 ? qsTr(" · wall ") + resultDelegate.modelData.wallMm.toFixed(2) + " mm" : "")
                                      + " · ±" + resultDelegate.modelData.uncertaintyPct.toFixed(0) + qsTr(" % for the material")
                                elide: Text.ElideRight
                                color: panel.subText
                                font.family: panel.fontSans
                                font.pixelSize: 10
                            }
                            Text {
                                Layout.fillWidth: true
                                text: resultDelegate.modelData.law
                                elide: Text.ElideRight
                                color: panel.subText
                                font.family: panel.fontSans
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            topInset: 0
            bottomInset: 0
            flat: true
            text: qsTr("Load this cell into the generator")
            enabled: panel.ctrl.currentResult >= 0 && panel.browser.currentFile !== ""
            font.family: panel.fontSans
            font.pixelSize: 12
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Load the cell's algorithm and parameters into the Generator tab, to rebuild it at another size or grid")
            onClicked: panel.generatorRequested()
        }
    }
}
