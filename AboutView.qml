import QtQuick 2.15
import QtQuick.Controls 2.15


Window {
    id: aboutView
    width: 800
    height: 600
    color: "#282828"
    maximumHeight: 600
    maximumWidth: 800
    title: qsTr("About MatViz3D")

    Grid {
        id: grid
        anchors.fill: parent
        columns: 1
        rows: 3
        rowSpacing: 20

        Text {
            id: overview
            width: grid.width * 0.8
            color: "#ffffff"
            text: qsTr("MaterialViz3D - this is a computer application for studying the structure of a material, which implements four algorithms for analyzing the structure of a material: Moore, von Neumann, probability circle, probability ellipse. \nA detailed overview of the material structure using visualization")
            font.pixelSize: 22
            wrapMode: Text.WordWrap
        }

        Text {
            id: team
            width: grid.width  * 0.8
            color: "#ffffff"
            text: qsTr("Project team\nSoftware developers: Oleh Semenenko, Valeriia Hritskova, \nOleksii Vodka, Nikita Mityasov\nTesters: Anastasiia Korzh, Hanna Khominich\nUX/UA designers: Kateryna Skrynnyk, Violetta Katsylo\n 3D technology developer: Yulia Chepela\nMentors: Oleksii Vodka, Lyudmila Rozova")
            font.pixelSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Text {
            id: link
            width: grid.width  * 0.8
            color: "#ffffff"
            text: qsTr("Link to Github, where the project is hosted:\nhttps://github.com/MME-NTU-KhPI/MatViz3D")
            font.pixelSize: 22
            wrapMode: Text.WordWrap
        }
    }
}
