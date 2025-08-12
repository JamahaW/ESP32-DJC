#pragma once

#include "kfgui/core/Window.hpp"


namespace kfgui {

struct WindowsManager {

private:

    Window *current_window{nullptr};

public:

    void bindWindow(Window &window) noexcept {
        current_window = &window;
    }

    void render(kf::Painter &painter) noexcept {
        if (current_window == nullptr) { return; }

        painter.fill(false);

        current_window->render(painter);
    }
};


}