#pragma once

#include <vector>
#include "kfgui/abc/Widget.hpp"


namespace kfgui {

struct Window {

public:

    const char *title{nullptr};

private:

    std::vector<const Widget *> widgets;

public:

    void add(const Widget &widget) noexcept {
        widgets.push_back(&widget);
    }

    void render(kf::Painter &painter) const noexcept {
        if (title != nullptr) {
            painter.text(0, 0, title);
        }

        for (auto w: widgets) {
            w->render();
        }
    }
};


}