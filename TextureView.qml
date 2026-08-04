import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 1100
    minimumHeight: 600
    visible: true
    color: "#1f1f1f"
    title: qsTr("MatViz3D — Texture Editor")

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    property var ctrl: textureController

    FontLoader { id: inter;      source: "qrc:/fonts/Inter-VariableFont_opsz,wght.ttf" }
    FontLoader { id: montserrat; source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf" }

    readonly property color colBg:     "#1f1f1f"
    readonly property color colPanel:  "#282828"
    readonly property color colPanel2: "#2f2f2f"
    readonly property color colSel:    "#31404a"
    readonly property color colBorder: "#3c3c3c"
    readonly property color colText:   "#d9d9d9"
    readonly property color colSub:    "#8a8a8a"
    readonly property color colAccent: "#22c3a6"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: colBg
            border.color: colBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 14

                Row {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 6
                    Label {
                        text: qsTr("Method:") + " " + processList[ctrl.process].name
                        color: colSub
                        font.pixelSize: 12; font.family: montserrat.name
                    }
                    RoundButton {
                        text: "\u2212"; implicitWidth: 34; implicitHeight: 34
                        onClicked: ctrl.grainCount = Math.max(10, ctrl.grainCount - 100)
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 54; horizontalAlignment: Text.AlignHCenter
                        text: ctrl.grainCount; color: colText
                        font.pixelSize: 14; font.bold: true; font.family: montserrat.name
                    }
                    RoundButton {
                        text: "+"; implicitWidth: 34; implicitHeight: 34
                        onClicked: ctrl.grainCount = ctrl.grainCount + 100
                    }
                }

                Button {
                    text: qsTr("Generate")
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignVCenter
                    highlighted: true
                    font.pixelSize: 14; font.family: inter.name
                    onClicked: ctrl.regenerate()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Apply to stress")
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: ctrl.applyToStress()
                }
            }
        }

        RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    id: leftPanel
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    color: colPanel

                    readonly property var processList: [
                        { name: "Random", desc: "Isotropic background", icon: "🎲" },
                        { name: "Extrusion", desc: "Fiber textures (<111>+<100> / <110>)", icon: "⭱" },
                        { name: "Rolling", desc: "Copper, Brass, S / Alpha, Gamma", icon: "⇌" },
                        { name: "Recrystallization", desc: "Cube & Goss components", icon: "❄" },
                        { name: "Shear", desc: "Torsion/Shear components", icon: "⇋" }
                    ]

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            Layout.margins: 16
                            text: qsTr("Texture Type")
                            color: colText
                            font.pixelSize: 15; font.bold: true; font.family: inter.name
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Repeater {
                                model: leftPanel.processList
                                delegate: Rectangle {
                                    id: procDelegate
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 64

                                    property bool active: index === ctrl.process
                                    color: active ? colSel : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 16
                                        spacing: 16

                                        Label {
                                            text: modelData.icon
                                            color: procDelegate.active ? colAccent : colSub
                                            font.pixelSize: 18
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label {
                                                text: modelData.name
                                                color: procDelegate.active ? colAccent : colText
                                                font.pixelSize: 14; font.bold: true; font.family: inter.name
                                            }
                                            Label {
                                                text: modelData.desc
                                                color: colSub
                                                font.pixelSize: 11; font.family: montserrat.name
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: 3; height: parent.height
                                        anchors.left: parent.left
                                        color: procDelegate.active ? colAccent : "transparent"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: ctrl.process = index
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: colBorder; Layout.topMargin: 16 }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 16
                            spacing: 12

                            Label { text: "Lattice:"; color: colSub; font.pixelSize: 12; font.family: montserrat.name }

                            RowLayout {
                                spacing: 8
                                Repeater {
                                    model: ["FCC (Cu, Al)", "BCC (Fe, W)"]
                                    delegate: Rectangle {
                                        width: 100; height: 32; radius: 16
                                        color: index === ctrl.lattice ? colSel : "transparent"
                                        border.color: colBorder

                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: index === ctrl.lattice ? colAccent : colText
                                            font.pixelSize: 12; font.family: montserrat.name
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: ctrl.lattice = index
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: colBorder }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.margins: 16
                            spacing: 4
                            Label {
                                text: qsTr("TEXTURE SHARPNESS")
                                color: colSub; font.pixelSize: 11; font.family: montserrat.name
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("Scatter σ (°)"); color: colText; font.pixelSize: 12; font.family: montserrat.name }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: ctrl.scatterDeg.toFixed(1) + "°"
                                    color: colAccent; font.pixelSize: 12; font.bold: true; font.family: montserrat.name
                                }
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 1; to: 40; value: ctrl.scatterDeg
                                onMoved: ctrl.scatterDeg = value
                                Material.accent: colAccent
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: colBorder }
                        Item { Layout.fillHeight: true }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        color: colBg
                        border.color: colBorder

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 24
                            anchors.rightMargin: 24
                            spacing: 16

                            Label {
                                text: qsTr("Euler space")
                                color: colText
                                font.pixelSize: 13; font.bold: true; font.family: montserrat.name
                            }
                            Rectangle {
                                width: 1; height: 20; color: colBorder
                            }
                            Label {
                                text: qsTr("Method:") + " " + (ctrl.processNames.length ? ctrl.processNames[ctrl.process].name : "")
                                color: colSub
                                font.pixelSize: 12; font.family: montserrat.name
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: "φ₂ = " + ctrl.sectionPhi2.toFixed(0) + "°"
                                color: colText
                                font.pixelSize: 13; font.family: montserrat.name
                            }
                            Slider {
                                Layout.preferredWidth: 160
                                from: 0; to: 90
                                value: ctrl.sectionPhi2
                                onMoved: ctrl.sectionPhi2 = value
                                Material.accent: colAccent
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Item {
                            id: plot
                            anchors.fill: parent
                            anchors.leftMargin: 50
                            anchors.rightMargin: 24
                            anchors.topMargin: 20
                            anchors.bottomMargin: 24

                            Rectangle {
                                anchors.fill: parent
                                color: "#181a20"
                                border.color: colBorder
                            }

                            property real phi2Tol: 8.0

                            Repeater {
                                model: 7   // 0, 15, ..., 90
                                Item {
                                    property real phiVal: index * 15
                                    width: plot.width
                                    y: (phiVal / 90.0) * plot.height
                                    height: 1

                                    Rectangle {
                                        width: plot.width; height: 1
                                        color: "#1affffff"
                                    }
                                    Label {
                                        x: -40
                                        y: -7
                                        text: parent.phiVal + "°"
                                        color: colSub
                                        font.pixelSize: 11
                                        font.family: montserrat.name
                                    }
                                }
                            }

                            Canvas {
                                id: eulerCanvas
                                anchors.fill: parent
                                property var pts: ctrl.eulerPoints
                                property real sect: ctrl.sectionPhi2
                                property real tol: plot.phi2Tol
                                onPtsChanged: requestPaint()
                                onSectChanged: requestPaint()

                                onPaint: {
                                    var g = getContext("2d"); g.reset()
                                    g.fillStyle = "rgba(200,200,200,0.4)"
                                    for (var i=0;i<pts.length;++i){
                                        var dp = Math.abs(pts[i].phi2 - sect)
                                        if (dp > 180) dp = 360 - dp
                                        if (dp > tol) continue
                                        g.beginPath()
                                        g.arc(pts[i].x*w, pts[i].y*h, 3.5, 0, 2*Math.PI)
                                        g.fill()
                                    }
                                }
                            }

                            Repeater {
                                model: ctrl.componentLabels
                                delegate: Item {
                                    property real dp: {
                                        var d = Math.abs(modelData.phi2 - ctrl.sectionPhi2)
                                        return d > 180 ? 360 - d : d
                                    }
                                    visible: dp <= plot.phi2Tol + 5
                                    x: modelData.x * plot.width
                                    y: modelData.y * plot.height

                                    Rectangle {
                                        width: 8; height: 8; radius: 4
                                        color: "#e8b835"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        x: 12; anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.name
                                        color: "#e8b835"
                                        font.pixelSize: 11
                                        font.bold: true
                                        font.family: montserrat.name
                                    }
                                }
                            }
                        }
                    }
                }
        }
    }
}
