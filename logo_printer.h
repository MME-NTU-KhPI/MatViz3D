#pragma once
#include <cstdio>

namespace Logo {

enum class Style {
    Full,   // icon + text side by side
    Icon,   // icon only
    Text    // text only
};

void print(Style style = Style::Full);

} // namespace Logo