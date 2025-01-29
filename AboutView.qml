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

    FontLoader {
        id: montserrat
        source: "fonts/Montserrat-VariableFont_wght.tff"
    }

    Grid {
        id: grid
        width: grid.childrenRect.width
        height: grid.childrenRect.height
        columns: 1
        rows: 3
        rowSpacing: 20
        anchors.centerIn: parent

        Text {
            id: overview
            width: 650
            color: "#ffffff"
            textFormat: Text.RichText
            text: qsTr("<span style='color: #06B09E;'>MaterialViz3D</span> - this is a computer application for studying the structure of a material, which implements four algorithms for analyzing the structure of a material: Moore, von Neumann, probability circle, probability ellipse. <br>A detailed overview of the material structure using visualization")
            font.pixelSize: 22
            wrapMode: Text.WordWrap
            font.family: montserrat.name
        }

        Text {
            id: team
            width: 650
            color: "#ffffff"
            textFormat: Text.RichText
            text: qsTr("Project team<br>Software developers: Oleh Semenenko, Valeriia Hritskova, <br>Oleksii Vodka, Nikita Mityasov<br>Testers: Anastasiia Korzh, Hanna Khominich<br>UX/UA designers: Kateryna Skrynnyk, Violetta Katsylo<br> 3D technology developer: Yulia Chepela<br>Mentors: Oleksii Vodka, Lyudmila Rozova")
            font.pixelSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.family: montserrat.name
        }

        Text {
            id: link
            width: 650
            color: "#ffffff"
            textFormat: Text.RichText
            text: qsTr("Link to Github, where the project is hosted: <br><a href='https://github.com/MME-NTU-KhPI/MatViz3D' style='color: #0E8E80; text-decoration: none;'>https://github.com/MME-NTU-KhPI/MatViz3D</a>")
            font.pixelSize: 22
            wrapMode: Text.WordWrap
            font.family: montserrat.name
        }
    }
}
