#include "logo_printer.h"
#include <cstdio>
#include <cstring>
#include <string>

// ── ANSI helpers ──────────────────────────────────────────────────────────────
// True-colour foreground: ESC[38;2;R;G;Bm
#define ANSI_FG(r,g,b)  "\033[38;2;" #r ";" #g ";" #b "m"
#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD       "\033[1m"

// ── Teal palette (matches app theme) ─────────────────────────────────────────
//   Character → solidification metaphor → colour
//
//   *   growth front     bright cyan
//   +   liquid interior  medium teal
//   =   transition       muted teal
//   -   solidifying      dark teal
//   #   grain boundary   deep green
//   %   fully solid      forest green

static const char* colorOf(char c)
{
    switch (c) {
        case '*': return ANSI_FG(0, 230, 210);   // bright cyan
        case '+': return ANSI_FG(0, 176, 158);   // medium teal  (#00B09E)
        case '=': return ANSI_FG(0, 137, 123);   // muted teal   (#00897B)
        case '-': return ANSI_FG(0,  86,  77);   // dark teal    (#00564D)
        case '#': return ANSI_FG(0,  64,  50);   // deep green
        case '%': return ANSI_FG(0,  48,  32);   // forest green
        default:  return ANSI_RESET;
    }
}

// Writes one line of the icon with per-character colouring.
static void printIconLine(const char* line)
{
    for (const char* p = line; *p; ++p) {
        fputs(colorOf(*p), stdout);
        fputc(*p, stdout);
    }
    fputs(ANSI_RESET, stdout);
}

// ── Logo data ─────────────────────────────────────────────────────────────────
//
// Each icon line is exactly 40 characters wide (padded with spaces) so the
// text block sits flush to the right at a fixed column.

static const char* ICON_LINES[] = {
    "                     **                ",
    "                 **********            ",
    "              #***************         ",
    "           #*********************      ",
    "        *****************************  ",
    "     **********************************",
    "  #*************************************#",
    "  *++++*****************************###%%",
    "  *+++++++***********************##%%%%%%",
    "  *++++++++++*****************##%%%%%%%%%",
    "  *+++===++++++++**********##%%%%%%#**%%%",
    "  *+++=--=++++++++++****#%%%%%%%%%*++#%%%",
    "  *+++=---++++++++++++##%%%%%#%%%#+++%%%%",
    "  *+++=-===++++====+++#####*++%%%#++#%%%%",
    "  *+++=-==-=++=----=++####*+++#%%*++%%%%%",
    "  *+++=-=+=-==-----=++#####++++##++#%%%%%",
    "  *+++=-=+=----==--=++#####*+++*+++#%%%%%",
    "  *+++===+=----+=--=++######*+++++*#####%",
    "   ++++++++=--=+=--=++#######+++++#######",
    "       +++++++++=--=++########+*#####    ",
    "          ++++++==-=++############       ",
    "             +++++++++#########          ",
    "                *+++++######             ",
    "                   +++###                ",
};
static const int ICON_ROWS = (int)(sizeof(ICON_LINES) / sizeof(ICON_LINES[0]));

// Text block lines — printed alongside the icon starting at row 8
static const char* TEXT_LINES[] = {
    "  __  __       ___      ___     ____  _____   ",
    " |  \\/  |     | \\ \\    / (_)   |___ \\|  __ \\  ",
    " | \\  / | __ _| |\\ \\  / / _ ____ __) | |  | | ",
    " | |\\/| |/ _` | __\\ \\/ / | |_  /|__ <| |  | | ",
    " | |  | | (_| | |_ \\  /  | |/ / ___) | |__| | ",
    " |_|  |_|\\__,_|\\__| \\/   |_/___|____/|_____/  ",
};
static const int TEXT_ROWS    = (int)(sizeof(TEXT_LINES) / sizeof(TEXT_LINES[0]));
static const int TEXT_START   = 8;   // icon row where text begins

// ── Printers ──────────────────────────────────────────────────────────────────

static void printFull()
{
    for (int row = 0; row < ICON_ROWS; ++row) {
        printIconLine(ICON_LINES[row]);

        int textRow = row - TEXT_START;
        if (textRow >= 0 && textRow < TEXT_ROWS) {
            fputs("  ", stdout);
            // Print text in bold bright-cyan
            fputs(ANSI_BOLD ANSI_FG(0, 230, 210), stdout);
            fputs(TEXT_LINES[textRow], stdout);
            fputs(ANSI_RESET, stdout);
        }
        fputc('\n', stdout);
    }
}

static void printIcon()
{
    for (int row = 0; row < ICON_ROWS; ++row) {
        printIconLine(ICON_LINES[row]);
        fputc('\n', stdout);
    }
}

static void printText()
{
    fputs(ANSI_BOLD ANSI_FG(0, 230, 210), stdout);
    for (int row = 0; row < TEXT_ROWS; ++row) {
        fputs(TEXT_LINES[row], stdout);
        fputc('\n', stdout);
    }
    fputs(ANSI_RESET, stdout);
}

// ── Public API ────────────────────────────────────────────────────────────────

namespace Logo {

void print(Style style)
{
    fflush(stdout);
    switch (style) {
        case Style::Full: printFull(); break;
        case Style::Icon: printIcon(); break;
        case Style::Text: printText(); break;
    }
    fflush(stdout);
}

} // namespace Logo