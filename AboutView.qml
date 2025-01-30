import QtQuick 2.15


Window {
    id: aboutView
    width: 800
    height: 600
    color: "#282828"
    minimumHeight: 600
    minimumWidth: 800
    maximumHeight: 600
    maximumWidth: 800
    title: qsTr("About MatViz3D")

    FontLoader {
        id: montserrat
        source: "qrc:/fonts/Montserrat-VariableFont_wght.ttf"
    }

    Grid {
        id: grid_aboutView
        width: grid_aboutView.childrenRect.width
        height: grid_aboutView.childrenRect.height
        columns: 1
        rows: 3
        rowSpacing: 23
        anchors.centerIn: parent

        Text {
            id: overview_aboutView
            width: 650
            color: "#ffffff"
            textFormat: Text.RichText
            text: qsTr("<span style='color: #06B09E; font-size: 24px;'><b>MaterialViz3D</b></span> - this is a computer application for studying the structure of a material, which implements four algorithms for analyzing the structure of a material: Moore, von Neumann, probability circle, probability ellipse. <br>A detailed overview of the material structure using visualization")
            font.pixelSize: 22
            wrapMode: Text.WordWrap
            font.family: montserrat.name
        }

        Text {
            id: team_aboutView
            width: 650
            color: "#ffffff"
            textFormat: Text.RichText
            text: qsTr("<span style='color: #06B09E; font-size: 24px;'><b>Project team</b></span><br>Software developers: Oleh Semenenko, Valeriia Hritskova, <br>Oleksii Vodka, Nikita Mityasov<br>Testers: Anastasiia Korzh, Hanna Khominich<br>UX/UA designers: Kateryna Skrynnyk, Violetta Katsylo<br> 3D technology developer: Yulia Chepela<br>Mentors: Oleksii Vodka, Lyudmila Rozova")
            font.pixelSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.family: montserrat.name
        }

        Rectangle {
            width: 650
            height: 58
            color: "transparent"

            Column {
                id: columnLink_aboutView
                anchors.fill: parent

                Text {
                    id: linkText
                    text: qsTr("Link to Github, where the project is hosted: ")
                    font.pixelSize: 22
                    color: "#ffffff"
                    font.family: montserrat.name
                }

                Text {
                    id: linkClickable
                    text: qsTr("https://github.com/MME-NTU-KhPI/MatViz3D")
                    font.pixelSize: 22
                    color: "#0E8E80"
                    font.family: montserrat.name

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Qt.openUrlExternally("https://github.com/MME-NTU-KhPI/MatViz3D")
                        }
                    }
                }
            }
        }
    }
}
