#pragma once

#include "KiraFlux-GUI.hpp"


struct JoyWidget final : kfgui::Widget {

private:

    const float *x{nullptr};
    const float *y{nullptr};

public:

    void bindAxis(const float &axis_x, const float &axis_y) noexcept {
        x = &axis_x;
        y = &axis_y;
    }

protected:

    void doRender(kf::Painter &p) const noexcept override {
        constexpr auto text_offset = static_cast<kf::Position>(2);
        constexpr auto format = "%+1.2f";

        p.rect(0, 0, p.maxX(), p.maxY(), kf::Painter::Mode::FillBorder);

        const auto center_x = p.centerX();
        const auto center_y = p.centerY();

        const auto right_text_x = p.maxGlyphX() - text_offset;

        if (x != nullptr) {
            p.line(
                center_x,
                center_y,
                static_cast<kf::Position>(center_x + *x * static_cast<float>(center_x)),
                center_y
            );

            p.setCursor(text_offset, text_offset);
            p.text(rs::formatted<8>(format, *x).data());
            p.setCursor(right_text_x, text_offset);
            p.text("X");
        }

        if (y != nullptr) {
            p.line(
                center_x,
                center_y,
                center_x,
                static_cast<kf::Position>(center_y - *y * static_cast<float>(center_y))
            );

            const auto text_offset_y = static_cast<kf::Position>(center_y + text_offset);

            p.setCursor(text_offset, text_offset_y);
            p.text(rs::formatted<8>(format, *y).data());
            p.setCursor(right_text_x, text_offset_y);
            p.text("Y");
        }
    }
};
