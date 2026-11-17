// ============================================================================
//  MaterialMatrixEditor.qml
//
//  Add/edit one material's row as a classical 6x6 symmetric stiffness
//  matrix, rather than the flat 21-column layout MaterialDatabaseView's
//  table uses. Opened from that window via openForRow()/openForNew().
//
//  Voigt order throughout, matching the rest of the codebase:
//  1=xx 2=yy 3=zz 4=yz 5=xz 6=xy.
//
//  Also reproduces the "Summary of properties" panel of the ELATE project
//  (progs.coudert.name/elate) for the matrix as currently edited: Voigt/
//  Reuss/Hill polycrystalline averages and the eigenvalues of the stiffness
//  matrix, via dbManager.elasticSummary() (see tensormath.hpp for the maths
//  and its unit test against ELATE's own FAU-zeolite worked example).
// ============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

Window {
    id: editor
    width: 640
    height: 900
    minimumWidth: 600
    minimumHeight: 820
    modality: Qt.NonModal
    color: theme.windowBg
    title: viewOnly ? qsTr("%1 \u2013 matrix view").arg(viewHeading)
         : targetRow >= 0 ? qsTr("Edit material \u2013 matrix view")
                          : qsTr("Add material \u2013 matrix view")

    // Without this, every plain (un-themed) Controls item -- TextField,
    // ComboBox, Button -- falls back to whatever style the platform default
    // resolves to, INDEPENDENTLY of this window's own dark background: on
    // some platforms that is a light-on-dark mismatch severe enough to draw
    // black text on a black fill. Every other top-level window in this app
    // sets this for the same reason (see MaterialDatabaseView.qml) -- it does
    // not inherit from the parent item across a Window boundary.
    Material.theme: editor.darkTheme ? Material.Dark : Material.Light
    Material.accent: Material.Teal

    /// Light/dark switch, bound by the host window so both stay in sync.
    property bool darkTheme: true

    QtObject {
        id: theme
        readonly property bool dark: editor.darkTheme
        readonly property color windowBg:   dark ? "#282828" : "#f4f4f4"
        readonly property color panelBg:    dark ? "#303030" : "#ffffff"
        readonly property color headerBg:   dark ? "#3a3a3a" : "#e6e6e6"
        readonly property color diagBg:     dark ? "#3d4d4a" : "#dff0ee"
        readonly property color border:     dark ? "#3a3a3a" : "#c8c8c8"
        readonly property color textStrong: dark ? "#CFCECE" : "#1c1c1c"
        readonly property color textBody:   dark ? "#c6c6c6" : "#333333"
        readonly property color textDim:    dark ? "#9e9e9e" : "#5c5c5c"
        readonly property color textFaint:  dark ? "#6a6a6a" : "#8a8a8a"
        readonly property color buttonBg:   dark ? "#303030" : "#ececec"
        readonly property color buttonHover:dark ? "#3a3a3a" : "#dcdcdc"
        readonly property color buttonEdge: dark ? "#969696" : "#b0b0b0"
        readonly property color accent:     "#4db6ac"
        readonly property color danger:     "#c0604d"

        /// Wraps URLs (http/https) in a string with <a href=...> tags for RichText.
        function linkify(text) {
            if (text === undefined || text === null) return "";
            return String(text).replace(/(https?:\/\/[^\s]+)/g, "<a href=\"$1\" style=\"color: #0E8E80;\">$1</a>");
        }
    }

    FontLoader { id: interFont; source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }

    /// Row in material_properties currently being edited; -1 means "not
    /// created yet" -- Save will insert a new row instead of updating one.
    property int targetRow: -1

    /// 6x6 symmetric working copy, always kept mirrored: setCell() writes
    /// both [i][j] and [j][i]. Nested-array mutation is invisible to QML's
    /// binding system, so every read goes through cell(), which depends on
    /// localRev -- the same "bump a counter" trick dbManager.revision uses.
    property var matrixData: emptyMatrix()
    property int localRev: 0
    property string materialName: ""
    property string materialType: ""
    property string materialComment: ""
    property string statusText: ""
    property bool statusIsError: false

    /// Read-only view of a matrix that is not a database row (a dataset cell's
    /// stiffness, opened from the cell panel by openForMatrix()): no name /
    /// type / comment, no editing, no New / Delete / Save, and a C / S switch.
    property bool viewOnly: false
    property string viewHeading: ""
    /// Unit of the entries and of the summary: GPa for the database, the
    /// cell panel's unit (MPa or GPa) in the view. The averages and the
    /// eigenvalues are linear in C, so they come out in the same unit.
    property string unit: "GPa"
    /// S, same order as matrixData, in sUnit; shown instead of C while showS.
    /// A thousand times the reciprocal of C's unit (1/GPa for C in MPa), so the
    /// entries read 4.784 rather than 0.004784 and fit their cells.
    property var viewS: null
    property string sUnit: "1/TPa"
    property bool showS: false
    readonly property bool showingS: viewOnly && showS && viewS !== null
    /// Voigt order of the rows and columns, for the axis labels.
    readonly property var axisNames: ["xx", "yy", "zz", "yz", "xz", "xy"]
    /// The largest entry shown, for treating rounding noise as 0 in the view.
    readonly property real shownMax: {
        localRev; showingS;
        var m = 0;
        for (var i = 0; i < 6; ++i)
            for (var j = 0; j < 6; ++j) m = Math.max(m, Math.abs(cell(i, j)));
        return m;
    }

    function emptyMatrix() {
        var m = [];
        for (var i = 0; i < 6; ++i) {
            var row = [];
            for (var j = 0; j < 6; ++j) row.push(0);
            m.push(row);
        }
        return m;
    }

    function cell(i, j) {
        localRev;
        return showingS ? viewS[i][j] : matrixData[i][j];
    }

    /// An entry of the read-only view: 4 significant digits, and 0 for what is
    /// below a millionth of the largest entry (solver noise of a symmetric cell).
    function viewText(v) {
        if (v === undefined || v === null || isNaN(v)) return "–";
        if (Math.abs(v) < 1e-6 * shownMax) return "0";
        return parseFloat(v.toPrecision(4)).toString();
    }

    function setCell(i, j, value) {
        var v = isNaN(value) ? 0 : value;
        matrixData[i][j] = v;
        matrixData[j][i] = v;
        ++localRev;
    }

    /// matrixData reshaped as the nested-list shape setElasticMatrix()/
    /// elasticMatrix() use.
    function matrixAsList() {
        var out = [];
        for (var i = 0; i < 6; ++i) {
            var row = [];
            for (var j = 0; j < 6; ++j) row.push(matrixData[i][j]);
            out.push(row);
        }
        return out;
    }

    /// Voigt/Reuss/Hill averages + eigenvalues for the matrix as currently
    /// edited (not yet saved). Depends on localRev for the same reason cell()
    /// does -- matrixData is mutated in place by setCell().
    property var summary: computeSummary()
    function computeSummary() {
        localRev;
        return dbManager.elasticSummary(matrixAsList());
    }

    function fmt(v, digits) {
        if (v === undefined || v === null || isNaN(v)) return "\u2013";
        if (Math.abs(v) >= 100000 || (Math.abs(v) < 0.001 && v !== 0))
            return v.toExponential(3);
        return parseFloat(v.toPrecision(digits === undefined ? 5 : digits)).toString();
    }

    /// Shows a matrix read-only: c and s are 6 rows of 6 numbers in Voigt order
    /// (xx yy zz yz xz xy), c in unitName (MPa or GPa), s in 1 / unitName. bringUp = false
    /// refreshes an open window without taking the focus (Prev / Next in the panel).
    function openForMatrix(heading, unitName, c, s, bringUp) {
        viewOnly = true;
        targetRow = -1;
        viewHeading = heading;
        unit = unitName;
        var m = emptyMatrix();
        for (var i = 0; i < 6 && i < c.length; ++i)
            for (var j = 0; j < 6 && j < c[i].length; ++j)
                m[i][j] = c[i][j];
        matrixData = m;
        sUnit = unitName === "MPa" ? "1/GPa" : "1/TPa";
        var sv = null;
        if (s && s.length === 6) {
            sv = emptyMatrix();
            for (var a = 0; a < 6; ++a)
                for (var b = 0; b < 6 && b < s[a].length; ++b)
                    sv[a][b] = 1000 * s[a][b];
        }
        viewS = sv;
        ++localRev;
        statusText = "";
        if (bringUp !== false) {
            editor.visible = true;
            editor.raise();
            editor.requestActivate();
        }
    }

    function leaveView() {
        viewOnly = false;
        showS = false;
        viewS = null;
        unit = "GPa";
    }

    /// Loads an existing row for editing.
    function openForRow(row) {
        leaveView();
        targetRow = row;
        if (row >= 0) {
            materialName = dbManager.materialNameAt(row);
            materialType = dbManager.materialTypeAt(row);
            materialComment = dbManager.materialCommentAt(row);
            var loaded = dbManager.elasticMatrix(row);
            var m = emptyMatrix();
            for (var i = 0; i < 6 && i < loaded.length; ++i) {
                var src = loaded[i];
                for (var j = 0; j < 6 && j < src.length; ++j)
                    m[i][j] = src[j];
            }
            matrixData = m;
            ++localRev;
        } else {
            openForNew();
            return;
        }
        statusText = "";
        editor.visible = true;
        editor.raise();
        editor.requestActivate();
    }

    /// Clears the form for entering a brand new material.
    function openForNew() {
        leaveView();
        targetRow = -1;
        materialName = "";
        materialType = "";
        materialComment = "";
        matrixData = emptyMatrix();
        ++localRev;
        statusText = "";
        editor.visible = true;
        editor.raise();
        editor.requestActivate();
    }

    function save() {
        if (materialName.trim().length === 0) {
            statusIsError = true;
            statusText = qsTr("Material needs a name.");
            return;
        }
        if (targetRow < 0) {
            dbManager.addMaterial(materialName);
            targetRow = dbManager.getModel().rowCount - 1;
        } else {
            dbManager.updateMaterial(targetRow, 1, materialName);
        }
        dbManager.updateMaterial(targetRow, 2, materialType);
        dbManager.updateMaterial(targetRow, 3, materialComment);
        dbManager.setElasticMatrix(targetRow, matrixAsList());
        statusIsError = false;
        statusText = qsTr("Saved.");
    }

    function remove() {
        if (targetRow < 0) return;
        dbManager.removeMaterial(targetRow);
        editor.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: editor.viewOnly ? editor.viewHeading : qsTr("Material as a 6\u00d76 stiffness matrix")
            wrapMode: Text.WordWrap
            color: theme.textStrong
            font.family: interFont.name
            font.pixelSize: 16
            font.weight: Font.Bold
        }

        // ── Name / type ─────────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            visible: !editor.viewOnly
            columns: 4
            columnSpacing: 8
            rowSpacing: 6

            Text {
                text: qsTr("Name:")
                color: theme.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            TextField {
                id: nameField
                Layout.fillWidth: true
                text: editor.materialName
                font.family: interFont.name
                font.pixelSize: 13
                color: theme.textStrong
                padding: 0
                topPadding: 4
                bottomPadding: 4
                leftPadding: 8
                rightPadding: 8
                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle {
                    radius: 6
                    color: theme.headerBg
                    border.color: theme.border
                    border.width: 1
                }
                onTextEdited: editor.materialName = text
            }
            Text {
                text: qsTr("Type:")
                color: theme.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }
            ComboBox {
                id: typeField
                Layout.preferredWidth: 110
                editable: true
                font.pixelSize: 13
                model: ["fcc", "bcc", "dc", "zb", "rs", "iso", "ti", "hcp"]
                currentIndex: -1
                editText: editor.materialType
                onEditTextChanged: editor.materialType = editText
            }
        }

        // ── Comment / source ───────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            visible: !editor.viewOnly
            columns: 2
            columnSpacing: 8
            rowSpacing: 6

            Text {
                text: qsTr("Comment:")
                color: theme.textDim
                font.family: interFont.name
                font.pixelSize: 13
            }

            // Show a clickable RichText preview above the edit field when there's a URL
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    visible: editor.materialComment.length > 0
                    textFormat: Text.RichText
                    text: theme.linkify(editor.materialComment)
                    color: theme.textStrong
                    font.family: interFont.name
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap

                    onLinkActivated: Qt.openUrlExternally(link)
                }

                TextField {
                    id: commentField
                    Layout.fillWidth: true
                    text: editor.materialComment
                    font.family: interFont.name
                    font.pixelSize: 13
                    color: theme.textStrong
                    padding: 0
                    topPadding: 4
                    bottomPadding: 4
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: 6
                        color: theme.headerBg
                        border.color: theme.border
                        border.width: 1
                    }
                    onTextEdited: editor.materialComment = text
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: editor.viewOnly
                  ? qsTr("Read only. Entries below are %1, Voigt order 1=xx 2=yy 3=zz 4=yz 5=xz 6=xy, engineering shear "
                         + "strains (C44 = G yz). The cell panel lists the same matrix in the order xx yy zz xy yz xz.")
                        .arg(editor.showingS ? editor.sUnit : editor.unit)
                  : targetRow >= 0
                  ? qsTr("Editing row %1. Entries below are GPa, Voigt order 1=xx 2=yy 3=zz 4=yz 5=xz 6=xy.")
                        .arg(targetRow)
                  : qsTr("New material. Entries below are GPa, Voigt order 1=xx 2=yy 3=zz 4=yz 5=xz 6=xy.")
            color: theme.textFaint
            font.family: interFont.name
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        // C or S, in the read-only view.
        RowLayout {
            Layout.fillWidth: true
            visible: editor.viewOnly && editor.viewS !== null
            spacing: 8
            RadioButton {
                text: qsTr("C, stiffness")
                checked: !editor.showS
                font.family: interFont.name
                font.pixelSize: 12
                onToggled: if (checked) editor.showS = false
            }
            RadioButton {
                text: qsTr("S, compliance")
                checked: editor.showS
                font.family: interFont.name
                font.pixelSize: 12
                onToggled: if (checked) editor.showS = true
            }
        }

        // ── The matrix ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            color: theme.panelBg
            border.color: theme.border
            border.width: 1
            radius: 6

            GridLayout {
                id: grid
                anchors.centerIn: parent
                columns: 7
                rowSpacing: 3
                columnSpacing: 3

                // Header row: blank corner + column indices 1..6.
                Repeater {
                    model: 7
                    delegate: Rectangle {
                        Layout.preferredWidth: index === 0 ? 50 : 62
                        Layout.preferredHeight: 28
                        color: "transparent"
                        Text {
                            anchors.centerIn: parent
                            visible: index > 0
                            text: index > 0 ? index + " " + editor.axisNames[index - 1] : ""
                            color: theme.textDim
                            font.family: interFont.name
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }
                    }
                }

                // Body: 6 rows, each a row-index label followed by 6 cells.
                // One delegate handles both the row-header cell (c === 0) and
                // the data cells, rather than a Loader with two Components --
                // Loader's default property is sourceComponent, which a
                // sibling Component child would collide with.
                Repeater {
                    model: 42
                    delegate: Item {
                        id: bodyCell
                        readonly property int r: Math.floor(index / 7)
                        readonly property int c: index % 7
                        readonly property int i: r
                        readonly property int j: c - 1
                        readonly property bool isHeader: c === 0
                        Layout.preferredWidth: isHeader ? 50 : 62
                        Layout.preferredHeight: 34

                        Text {
                            anchors.centerIn: parent
                            visible: bodyCell.isHeader
                            text: (bodyCell.r + 1) + " " + editor.axisNames[bodyCell.r]
                            color: theme.textDim
                            font.family: interFont.name
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: !bodyCell.isHeader
                            color: bodyCell.i === bodyCell.j ? theme.diagBg : theme.headerBg
                            border.color: theme.border
                            border.width: 1
                            radius: 3

                            TextField {
                                anchors.fill: parent
                                anchors.margins: 2
                                padding: 0
                                topPadding: 0
                                bottomPadding: 0
                                // Set explicitly: the Material style gives a TextField
                                // its own side padding (~16 px), which `padding: 0` does
                                // not override, and in a 62 px cell that cut the first
                                // digits off ("121.4" read "l21.4", "-0.68" read "68").
                                leftPadding: 2
                                rightPadding: 2
                                verticalAlignment: TextInput.AlignVCenter
                                horizontalAlignment: TextInput.AlignHCenter
                                font.family: interFont.name
                                font.pixelSize: 12
                                color: theme.textStrong
                                background: Rectangle { color: "transparent" }
                                validator: DoubleValidator { notation: DoubleValidator.StandardNotation }
                                readOnly: editor.viewOnly
                                // Guarded rather than relying on visibility:
                                // bindings evaluate even while hidden, and
                                // j === -1 in the header column would index
                                // matrixData out of range.
                                text: bodyCell.isHeader ? ""
                                    : editor.viewOnly ? editor.viewText(editor.cell(bodyCell.i, bodyCell.j))
                                    : editor.cell(bodyCell.i, bodyCell.j).toString()
                                onEditingFinished: {
                                    if (!bodyCell.isHeader && !editor.viewOnly)
                                        editor.setCell(bodyCell.i, bodyCell.j, parseFloat(text))
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !editor.viewOnly
            text: qsTr("Off-diagonal entries are mirrored automatically \u2014 edit either side of the diagonal.")
            color: theme.textFaint
            font.family: interFont.name
            font.pixelSize: 10
        }

        // ── Summary of properties (ELATE-style) ────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: summaryColumn.implicitHeight + 20
            color: theme.panelBg
            border.color: theme.border
            border.width: 1
            radius: 6
            visible: summary.valid === true

            ColumnLayout {
                id: summaryColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: qsTr("Summary of properties")
                    color: theme.textStrong
                    font.family: interFont.name
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 5
                    columnSpacing: 14
                    rowSpacing: 4

                    Text { text: ""; Layout.preferredWidth: 70 }
                    Text {
                        text: qsTr("Bulk modulus")
                        color: theme.textDim; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: qsTr("Young's modulus")
                        color: theme.textDim; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: qsTr("Shear modulus")
                        color: theme.textDim; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: qsTr("Poisson's ratio")
                        color: theme.textDim; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }

                    // Spelled out per averaging scheme (3 rows x 5 columns)
                    // rather than nested Repeaters -- GridLayout needs a flat
                    // child list, and three rows is little enough to write
                    // directly and read at a glance.
                    Text {
                        text: qsTr("Voigt")
                        color: theme.textBody; font.family: interFont.name; font.pixelSize: 12
                    }
                    Text {
                        text: fmt(summary.KV) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.EV) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.GV) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.nuV, 4)
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        text: qsTr("Reuss")
                        color: theme.textBody; font.family: interFont.name; font.pixelSize: 12
                    }
                    Text {
                        text: fmt(summary.KR) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.ER) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.GR) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.nuR, 4)
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        text: qsTr("Hill")
                        color: theme.textBody; font.family: interFont.name; font.pixelSize: 12
                    }
                    Text {
                        text: fmt(summary.KH) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.EH) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.GH) + " " + editor.unit
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                    Text {
                        text: fmt(summary.nuH, 4)
                        color: theme.textStrong; font.family: interFont.name; font.pixelSize: 12
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: summary.cubic === true
                    text: qsTr("Zener anisotropy ratio: %1").arg(fmt(summary.zener, 4))
                    color: theme.textDim
                    font.family: interFont.name
                    font.pixelSize: 12
                }

                Text {
                    text: qsTr("Eigenvalues of the stiffness matrix")
                    color: theme.textStrong
                    font.family: interFont.name
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Repeater {
                        model: summary.eigenvalues ? summary.eigenvalues : []
                        delegate: Text {
                            required property double modelData
                            text: fmt(modelData, 4) + " " + editor.unit
                            color: modelData > 0 ? theme.textStrong : theme.danger
                            font.family: interFont.name
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    // Coerced to a real bool: `undefined && ...` (before the
                    // first summary is computed) is `undefined`, and Qt warns
                    // on assigning that to a bool property rather than
                    // treating it as falsy.
                    visible: !!(summary.eigenvalues && summary.eigenvalues.length === 6
                                && Math.min.apply(null, summary.eigenvalues) <= 0)
                    text: qsTr("At least one eigenvalue is not positive \u2014 this matrix is not "
                             + "mechanically stable (not positive definite).")
                    color: theme.danger
                    font.family: interFont.name
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Enter a matrix to see Voigt/Reuss/Hill averages and its stiffness eigenvalues \u2014 all zero is not invertible.")
            color: theme.textFaint
            font.family: interFont.name
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            visible: summary.valid !== true
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            visible: statusText.length > 0
            text: statusText
            color: statusIsError ? theme.danger : theme.accent
            font.family: interFont.name
            font.pixelSize: 12
        }

        // ── Actions ──────────────────────────────────────────────────────
        // Explicitly skinned rather than plain Button/Material defaults, same
        // as every pill button in MaterialDatabaseView.qml -- and not just for
        // visual consistency: a plain Button's colours come from whatever
        // style/theme happens to resolve at the ambient level, which is what
        // produced illegible (black-on-black) text here before Material.theme
        // was set on this window at all. Explicit colours from `theme` cannot
        // silently mismatch the custom-painted background behind them.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                id: newButton
                visible: !editor.viewOnly
                Layout.preferredWidth: 90
                Layout.preferredHeight: 40
                text: qsTr("New")
                font.family: interFont.name
                font.pixelSize: 15

                background: Rectangle {
                    radius: 10
                    color: hoverAreaNew.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1
                    MouseArea {
                        id: hoverAreaNew
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: newButton.text
                    font: newButton.font
                    color: theme.textStrong
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: editor.openForNew()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Clear the form to add another material")
            }

            Item { Layout.fillWidth: true }

            Button {
                id: deleteButton
                visible: !editor.viewOnly
                Layout.preferredWidth: 90
                Layout.preferredHeight: 40
                text: qsTr("Delete")
                font.family: interFont.name
                font.pixelSize: 15
                enabled: targetRow >= 0
                opacity: enabled ? 1.0 : 0.5

                background: Rectangle {
                    radius: 10
                    color: hoverAreaDelete.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1
                    MouseArea {
                        id: hoverAreaDelete
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: deleteButton.text
                    font: deleteButton.font
                    color: theme.textStrong
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: editor.remove()
            }
            Button {
                id: closeButton
                Layout.preferredWidth: 90
                Layout.preferredHeight: 40
                text: qsTr("Close")
                font.family: interFont.name
                font.pixelSize: 15

                background: Rectangle {
                    radius: 10
                    color: hoverAreaClose.containsMouse ? theme.buttonHover : theme.buttonBg
                    border.color: theme.buttonEdge
                    border.width: 1
                    MouseArea {
                        id: hoverAreaClose
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.NoButton
                    }
                }
                contentItem: Text {
                    text: closeButton.text
                    font: closeButton.font
                    color: theme.textStrong
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: editor.close()
            }
            Button {
                id: saveButton
                visible: !editor.viewOnly
                Layout.preferredWidth: 90
                Layout.preferredHeight: 40
                text: targetRow >= 0 ? qsTr("Save") : qsTr("Add")
                font.family: interFont.name
                font.pixelSize: 15

                background: Rectangle {
                    radius: 10
                    color: theme.accent
                }
                contentItem: Text {
                    text: saveButton.text
                    font: saveButton.font
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: editor.save()
            }
        }
    }
}
