import QtQuick

/**
 * Палитра графика. Все цвета — производные от одного флага dark.
 * Использование:
 *     ChartTheme { id: chartTheme }
 *     ...
 *     color: chartTheme.gridLine
 *
 * Смена темы:  chartTheme.dark = false
 */
QtObject {
    id: theme

    // ═══ Единственный переключатель ═══
    property bool dark: true

    // ── Фон окна ──
    property color windowBackground: dark ? "#282828" : "#f2f2f2"

    // ── Поле построения ──
    property color plotBackground: dark ? "#1e1e1e" : "#ffffff"
    property color plotBorder:     dark ? "#4a4a4a" : "#b0b0b0"

    // ── Сетка и деления ──
    property color gridLine: dark ? "#33ffffff" : "#22000000"
    property color tickMark: dark ? "#969696"   : "#555555"

    // ── Текст ──
    property color chartTitle:  dark ? "#d9d9d9" : "#1a1a1a"
    property color axisTitle:   dark ? "#c6c6c6" : "#333333"
    property color axisLabel:   dark ? "#969696" : "#555555"
    property color placeholder: dark ? "#5a5a5a" : "#aaaaaa"

    // ── Кривая гистограммы ──
    property color seriesStroke: dark ? "#00897b"   : "#00564d"
    property color seriesFill:   dark ? "#9600564d" : "#5000897b"

    // ── Элементы управления ──
    property color controlText:       dark ? "#c6c6c6" : "#222222"
    property color controlBackground: dark ? "#282828" : "#ffffff"
    property color controlBorder:     dark ? "#969696" : "#909090"
}
