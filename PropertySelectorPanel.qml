import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

// The "Selector" tab of the main window's Data panel: target elastic properties
// and the printing material in, the closest accepted cells of the open dataset
// out (PropertySelectorController). Every number is a slider; the moduli move on
// a logarithmic scale over what the dataset spans for the chosen material, and
// the search reruns a moment after a slider is let go. Choosing a result opens
// that cell in the 3D view and its parameters in DatasetCellPanel on the right.
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
                                           : ({ E: [1, 1000], G: [0.5, 400], nu: [-0.3, 0.5], rho: [0, 1] })

    // Target values survive switching between one value for all directions and per-axis ones.
    property bool sameInAllDirections: true
    property var targets: ({
        E:    { on: true,  v: 300 }, G:    { on: false, v: 100 }, nu:   { on: false, v: 0.3 },
        Ex:   { on: true,  v: 300 }, Ey:   { on: true,  v: 300 }, Ez:   { on: true,  v: 300 },
        Gxy:  { on: false, v: 100 }, Gyz:  { on: false, v: 100 }, Gxz:  { on: false, v: 100 },
        nuXy: { on: false, v: 0.3 }
    })
    readonly property var isoRows: [
        { key: "E",  label: qsTr("E"),  kind: "E" },
        { key: "G",  label: qsTr("G"),  kind: "G" },
        { key: "nu", label: qsTr("ν"),  kind: "nu" }
    ]
    readonly property var axisRows: [
        { key: "Ex", label: qsTr("E x"), kind: "E" }, { key: "Ey", label: qsTr("E y"), kind: "E" },
        { key: "Ez", label: qsTr("E z"), kind: "E" }, { key: "Gxy", label: qsTr("G xy"), kind: "G" },
        { key: "Gyz", label: qsTr("G yz"), kind: "G" }, { key: "Gxz", label: qsTr("G xz"), kind: "G" },
        { key: "nuXy", label: qsTr("ν xy"), kind: "nu" }
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
            put("E", ["Ex", "Ey", "Ez"]);
            put("G", ["Gxy", "Gyz", "Gxz"]);
            put("nu", ["nuXy", "nuXz", "nuYz"]);
        } else {
            for (const k of ["Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy"]) put(k, [k]);
        }
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

    onVisibleChanged: startIndexIfNeeded()
    Connections {
        target: panel.ctrl
        function onIndexChanged() {
            panel.startIndexIfNeeded();
            if (panel.ctrl.ready) {
                rhoRange.setValues(rhoRange.from, rhoRange.to);
                panel.scheduleSearch();
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
        Layout.preferredWidth: 78
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
        RowLabel { id: rowLabel; Layout.preferredWidth: 72 }
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
                    delegate: RowLayout {
                        id: targetRow
                        required property var modelData
                        readonly property bool isModulus: modelData.kind !== "nu"
                        readonly property var ends: panel.span[modelData.kind]
                        Layout.fillWidth: true
                        spacing: 6
                        CheckBox {
                            id: useBox
                            Layout.preferredWidth: 30
                            checked: panel.targets[targetRow.modelData.key].on
                            padding: 0
                            onToggled: { panel.targets[targetRow.modelData.key].on = checked; panel.scheduleSearch(); }
                        }
                        RowLabel {
                            Layout.preferredWidth: 36
                            text: targetRow.modelData.label
                            opacity: useBox.checked ? 1.0 : 0.5
                        }
                        // Moduli on a log scale (the slider holds log10 of MPa), ratios plain.
                        Slider {
                            id: targetSlider
                            Layout.fillWidth: true
                            Layout.preferredHeight: 26
                            topPadding: 0
                            bottomPadding: 0
                            enabled: useBox.checked
                            from: targetRow.isModulus ? panel.log10(targetRow.ends[0]) : targetRow.ends[0]
                            to: targetRow.isModulus ? panel.log10(targetRow.ends[1]) : targetRow.ends[1]
                            stepSize: targetRow.isModulus ? 0.005 : 0.01
                            value: targetRow.isModulus ? panel.log10(panel.targets[targetRow.modelData.key].v)
                                                       : panel.targets[targetRow.modelData.key].v
                            readonly property real actual: targetRow.isModulus ? Math.pow(10, value) : value
                            onActualChanged: panel.targets[targetRow.modelData.key].v = actual
                            onMoved: panel.scheduleSearch()
                        }
                        ValueText {
                            opacity: useBox.checked ? 1.0 : 0.4
                            text: panel.fmt(targetSlider.actual) + (targetRow.isModulus ? " MPa" : "")
                        }
                    }
                }

                // ── Limits ──
                Caption { text: qsTr("Limits") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLabel { text: qsTr("density ρ"); Layout.preferredWidth: 72 }
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
                    RowLabel { text: qsTr("family"); Layout.preferredWidth: 72 }
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
