#pragma once

#include "kf/Painter.hpp"


namespace kfgui {

struct Widget {

private:

    kf::Painter *current_painter{nullptr};

protected:

    virtual void doRender(kf::Painter &painter) const noexcept = 0;

public:

    void render() const noexcept {
        if (current_painter == nullptr) {
            return;
        }

        doRender(*current_painter);
    }

    void bindPainter(kf::Painter &new_painter) noexcept {
        current_painter = &new_painter;
    }
};


}