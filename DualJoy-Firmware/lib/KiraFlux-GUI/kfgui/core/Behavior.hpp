#pragma once

#include <vector>
#include "kfgui/abc/Widget.hpp"


namespace kfgui {

struct Behavior {

private:

    std::vector<Widget *> widgets{};

public:

    void add(Widget &widget) noexcept {
        widgets.push_back(&widget);
    }

    void render() noexcept {
        for (auto w: widgets) {
            w->render();
        }
    }

    virtual void bindPainters(kf::Painter &root) noexcept {};

    virtual void onBind() noexcept {}

    virtual void loop() noexcept {}
};


}