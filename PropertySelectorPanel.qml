import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

// The "Selector" tab of the main window's Data panel: target elastic properties
// and the printing material in, the closest accepted cells of the open dataset
// out (PropertySelectorController). Choosing a result opens that cell in the 3D
// view and its parameters in DatasetCellPanel on the right, like the Dataset tab.
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

    // Target values survive switching between one value for all directions and per-axis ones.
    property bool sameInAllDirections: true
    property var targets: ({
        E:    { on: true,  v: "300" }, G:    { on: false, v: "100" }, nu:   { on: false, v: "0.3" },
        Ex:   { on: true,  v: "300" }, Ey:   { on: true,  v: "300" }, Ez:   { on: true,  v: "300" },
        Gxy:  { on: false, v: "100" }, Gyz:  { on: false, v: "100" }, Gxz:  { on: false, v: "100" },
        nuXy: { on: false, v: "0.3" }
    })
    readonly property var isoRows: [
        { key: "E",  label: qsTr("E"), unit: "MPa" },
        { key: "G",  label: qsTr("G"), unit: "MPa" },
        { key: "nu", label: qsTr("ν"), unit: "" }
    ]
    readonly property var axisRows: [
        { key: "Ex", label: qsTr("E x"), unit: "MPa" }, { key: "Ey", label: qsTr("E y"), unit: "MPa" },
        { key: "Ez", label: qsTr("E z"), unit: "MPa" }, { key: "Gxy", label: qsTr("G xy"), unit: "MPa" },
        { key: "Gyz", label: qsTr("G yz"), unit: "MPa" }, { key: "Gxz", label: qsTr("G xz"), unit: "MPa" },
        { key: "nuXy", label: qsTr("ν xy"), unit: "" }
    ]

    function num(t) { return Number(String(t).trim().replace(",", ".")); }

    function startIndexIfNeeded() {
        if (panel.visible && ctrl.hasDataset && !ctrl.ready && !ctrl.indexing)
            Qt.callLater(ctrl.buildIndex);
    }

    function runSearch() {
        let t = {};
        function put(key, names) {
            const s = panel.targets[key];
            if (!s.on || String(s.v).trim() === "") return;
            for (const n of names) t[n] = panel.num(s.v);
        }
        if (panel.sameInAllDirections) {
            put("E", ["Ex", "Ey", "Ez"]);
            put("G", ["Gxy", "Gyz", "Gxz"]);
            put("nu", ["nuXy", "nuXz", "nuYz"]);
        } else {
            for (const k of ["Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy"]) put(k, [k]);
        }
        ctrl.search({
            material: { e_gpa: panel.num(matE.text), nu: panel.num(matNu.text), aniso: panel.num(matR.text) },
            targets: t,
            rho_min: panel.num(rhoMin.text),
            rho_max: panel.num(rhoMax.text),
            family: familyBox.currentValue === undefined ? "" : familyBox.currentValue,
            count: 10
        });
    }

    onVisibleChanged: startIndexIfNeeded()
    Connections {
        target: panel.ctrl
        function onIndexChanged() { panel.startIndexIfNeeded(); }
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
    component NumberField: TextField {
        Layout.preferredHeight: 30
        topInset: 0
        bottomInset: 0
        horizontalAlignment: Text.AlignRight
        font.family: panel.fontSans
        font.pixelSize: 12
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        validator: RegularExpressionValidator { regularExpression: /^-?[0-9]*[.,]?[0-9]*$/ }
        onAccepted: panel.runSearch()
    }
    component Unit: Text {
        color: panel.subText
        font.family: panel.fontSans
        font.pixelSize: 11
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

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

        // ── Material ──
        Caption { text: qsTr("Material you print with") }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Unit { text: "E" }
            NumberField { id: matE; Layout.fillWidth: true; text: "2.0" }
            Unit { text: "GPa  ν" }
            NumberField { id: matNu; Layout.fillWidth: true; text: "0.38" }
            Unit { text: qsTr("across ×") }
            NumberField {
                id: matR
                Layout.fillWidth: true
                text: "1.0"
                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("E across the printed layers (along Z) as a share of E along them; 1 = the same in every direction")
            }
        }

        // ── Targets ──
        RowLayout {
            Layout.fillWidth: true
            Caption { text: qsTr("Target properties") }
            CheckBox {
                id: isoBox
                text: qsTr("same in all directions")
                checked: panel.sameInAllDirections
                font.family: panel.fontSans
                font.pixelSize: 11
                topPadding: 0
                bottomPadding: 0
                onToggled: panel.sameInAllDirections = checked
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: panel.sameInAllDirections ? 1 : 2
            columnSpacing: 8
            rowSpacing: 2
            Repeater {
                model: panel.sameInAllDirections ? panel.isoRows : panel.axisRows
                delegate: RowLayout {
                    id: targetRow
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 4
                    CheckBox {
                        id: useBox
                        Layout.preferredWidth: panel.sameInAllDirections ? 70 : 64
                        text: targetRow.modelData.label
                        checked: panel.targets[targetRow.modelData.key].on
                        font.family: panel.fontSans
                        font.pixelSize: 12
                        topPadding: 0
                        bottomPadding: 0
                        onToggled: panel.targets[targetRow.modelData.key].on = checked
                    }
                    NumberField {
                        Layout.fillWidth: true
                        enabled: useBox.checked
                        text: panel.targets[targetRow.modelData.key].v
                        onTextEdited: panel.targets[targetRow.modelData.key].v = text
                    }
                    Unit {
                        Layout.preferredWidth: 26
                        text: targetRow.modelData.unit
                    }
                }
            }
        }

        // ── Limits ──
        Caption { text: qsTr("Limits") }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Unit { text: qsTr("ρ from") }
            NumberField { id: rhoMin; Layout.preferredWidth: 52; text: "0.0" }
            Unit { text: qsTr("to") }
            NumberField { id: rhoMax; Layout.preferredWidth: 52; text: "1.0" }
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
            }
        }
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            topInset: 0
            bottomInset: 0
            text: qsTr("Find cells")
            highlighted: true
            enabled: panel.ctrl.ready
            font.family: panel.fontSans
            font.pixelSize: 13
            onClicked: panel.runSearch()
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: panel.ctrl.message
            wrapMode: Text.WordWrap
            color: panel.subText
            font.family: panel.fontSans
            font.pixelSize: 11
        }

        Text {
            Layout.fillWidth: true
            visible: resultList.count > 0
            text: qsTr("Values below are for your material; the panel on the right shows the cell as it was solved.")
            wrapMode: Text.WordWrap
            color: panel.subText
            font.family: panel.fontSans
            font.pixelSize: 10
        }

        // ── Results ──
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            color: "#33000000"
            radius: 6

            ListView {
                id: resultList
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                model: panel.ctrl.results
                currentIndex: panel.ctrl.currentResult
                boundsBehavior: Flickable.StopAtBounds
                spacing: 2
                ScrollBar.vertical: ScrollBar { }

                delegate: ItemDelegate {
                    id: resultDelegate
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    highlighted: index === panel.ctrl.currentResult
                    topPadding: 6
                    bottomPadding: 6
                    onClicked: panel.ctrl.show(index)

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

                Text {
                    anchors.centerIn: parent
                    visible: resultList.count === 0
                    width: parent.width - 30
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: !panel.ctrl.hasDataset ? qsTr("Open a dataset (Dataset tab or the button above), then give the properties you need.")
                          : panel.ctrl.ready ? qsTr("Give the properties you need and press Find cells.")
                          : qsTr("Reading the accepted cells…")
                    color: panel.subText
                    font.family: panel.fontSans
                    font.pixelSize: 12
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
