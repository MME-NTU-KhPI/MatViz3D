import QtQuick

QtObject {
    id: theme

    property bool dark: true

    property color windowBackground: dark ? "#282828" : "#f2f2f2"

    property color plotBackground: dark ? "#1e1e1e" : "#ffffff"
    property color plotBorder:     dark ? "#4a4a4a" : "#b0b0b0"

    property color gridLine: dark ? "#33ffffff" : "#22000000"
    property color tickMark: dark ? "#969696"   : "#555555"

    property color chartTitle:  dark ? "#d9d9d9" : "#1a1a1a"
    property color axisTitle:   dark ? "#c6c6c6" : "#333333"
    property color axisLabel:   dark ? "#969696" : "#555555"
    property color placeholder: dark ? "#5a5a5a" : "#aaaaaa"

    property color seriesStroke: dark ? "#00897b"   : "#00564d"
    property color seriesFill:   dark ? "#9600564d" : "#5000897b"

    property color controlText:       dark ? "#c6c6c6" : "#222222"
    property color controlBackground: dark ? "#282828" : "#ffffff"
    property color controlBorder:     dark ? "#969696" : "#909090"

    // ── Texture Editor roles ────────────────────────────────────────────────
    // The dark values are exactly the palette TextureView.qml used to hardcode,
    // so dark mode is unchanged; the light values are their counterparts.
    property color appBackground:   dark ? "#1f1f1f" : "#fafafa"
    property color panelBackground: dark ? "#282828" : "#eeeeee"
    property color panelAlt:        dark ? "#2f2f2f" : "#e3e3e3"
    property color selection:       dark ? "#31404a" : "#d3e7e3"
    property color borderColor:     dark ? "#3c3c3c" : "#c6c6c6"
    property color textColor:       dark ? "#d9d9d9" : "#1a1a1a"
    property color subTextColor:    dark ? "#8a8a8a" : "#5f5f5f"
    property color accentColor:     dark ? "#22c3a6" : "#00796b"

    // Scatter points (pole figure, Euler section) and the ideal-component markers.
    property color pointColor:  dark ? "#e8e8e8" : "#2b2b2b"
    property real  pointAlpha:  dark ? 0.38      : 0.45
    property color markerColor: dark ? "#e8b835" : "#a37400"
}
