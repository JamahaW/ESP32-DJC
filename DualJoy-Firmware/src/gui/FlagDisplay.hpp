#pragma once

#include "KiraFlux-GUI.hpp"


struct FlagDisplay : kfgui::Widget {

public:

    const char *label{nullptr};
    bool *flag{nullptr};

protected:

    void doRender(kf::Painter &painter) const noexcept override {
        const bool lit = (flag != nullptr) and *flag;

        painter.fill(lit);
        painter.text(0, 0, (label == nullptr) ? "null" : label, not lit);
    }

};