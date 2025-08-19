#pragma once

#include "KiraFlux-GUI.hpp"


struct FlagDisplay : kfgui::Widget {

public:

    const char *label{nullptr};
    const bool *flag{nullptr};

    void render() noexcept override {
        const bool lit = (flag != nullptr) and *flag;
        painter.setCursor(0, 0);
        painter.text_value_on = not lit;
        painter.text((label == nullptr) ? "null" : label);
    }

};