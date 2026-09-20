pragma Singleton
import QtQuick 2.15

QtObject {
    id: theme

    // Dark Theme (default)
    readonly property color harborDark: "#0F1E27"
    readonly property color deckDark: "#16303C"
    readonly property color lineDark: "#21414F"
    readonly property color fogDark: "#E6EEF2"
    readonly property color mistDark: "#8FA6B2"
    readonly property color sonarDark: "#4FD8E8"
    readonly property color driftDark: "#F2B04C"
    readonly property color lostDark: "#F0625D"

    // Light Theme
    readonly property color harborLight: "#EEF3F6"
    readonly property color deckLight: "#FFFFFF"
    readonly property color lineLight: "#CBD8DF"
    readonly property color fogLight: "#0F1E27"
    readonly property color mistLight: "#4A6472"
    readonly property color sonarLight: "#0A7A8C"
    readonly property color driftLight: "#9A5B00"
    readonly property color lostLight: "#C7362F"

    // Active Theme State (0 = System, 1 = Dark, 2 = Light)
    property int themeMode: 1
    readonly property bool isDark: (themeMode === 1) || (themeMode === 0)

    // Dynamic Tokens
    readonly property color harbor: isDark ? harborDark : harborLight
    readonly property color deck: isDark ? deckDark : deckLight
    readonly property color line: isDark ? lineDark : lineLight
    readonly property color fog: isDark ? fogDark : fogLight
    readonly property color mist: isDark ? mistDark : mistLight
    readonly property color sonar: isDark ? sonarDark : sonarLight
    readonly property color drift: isDark ? driftDark : driftLight
    readonly property color lost: isDark ? lostDark : lostLight

    // Text on Sonar Accent
    readonly property color textOnSonar: harborDark

    // Typography
    readonly property string sansFont: "Manrope, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    readonly property string serifFont: "'Instrument Serif', Georgia, serif"

    readonly property int fontSizeSync: 56
    readonly property int fontSizeTitle: 28
    readonly property int fontSizeBody: 16
    readonly property int fontSizeSecondary: 14
    readonly property int fontSizeCaption: 12

    // Spacing
    readonly property int space4: 4
    readonly property int space8: 8
    readonly property int space12: 12
    readonly property int space16: 16
    readonly property int space24: 24
    readonly property int space32: 32
    readonly property int space48: 48

    // Radius
    readonly property int radiusControl: 4
    readonly property int radiusPanel: 12
    readonly property int radiusFull: 9999

    // Motion
    property bool reducedMotion: false
}
