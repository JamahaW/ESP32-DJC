#pragma once

#include "kfgui/abc/Widget.hpp"


namespace kfgui {

struct TextDisplay final : Widget {

    const char *text;

    explicit TextDisplay(const char *text) :
        text{text} {}

    explicit TextDisplay() :
        text{nullptr} {}

    void render() noexcept override {
        painter.setCursor(0, 0);
        painter.text((text == nullptr) ? "null" : text);
    }

};

}
