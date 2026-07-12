import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Column {
    id: root
    spacing: 8

    // Repeater перебирает схему и создаёт по одному блоку на каждое поле
    Repeater {
        model: schemaController.currentSchema

        delegate: Column {
            spacing: 4
            property var f: modelData        // ссылка на текущее поле схемы

            // Подпись поля
            Text {
                text: f.label
                color: "#969696"
                font.pixelSize: 12
                font.family: montserrat.name
                font.bold: true
            }

            // Само поле ввода — переиспользуем существующий PlaceholderInput
            PlaceholderInput {
                placeholderText: String(f.defValue)
                initialValue: String(f.defValue)
                onTextChanged: schemaController.applyValue(f.key, text)
            }
        }
    }
}
