#include <WiFi.h>
#include <vector>
#include "Arduino.h"

#include "kf/GFX.hpp"
#include "kf/SSD1306.h"
#include "kf/Joystick.hpp"

#include "espnow/Protocol.hpp"


static constexpr espnow::Mac target = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


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

struct GuiSystem {

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

struct JoyWidget final : Widget {

    const float *x{nullptr};
    const float *y{nullptr};

    void doRender(kf::Painter &p) const noexcept override {
        constexpr auto text_offset = static_cast<kf::Position>(2);

        const auto max_x = static_cast<kf::Position>(p.frame.width - 1);
        const auto max_y = static_cast<kf::Position>(p.frame.height - 1);

        p.rect(0, 0, max_x, max_y, kf::Painter::Mode::FillBorder);

        const auto center_x = static_cast<kf::Position>(max_x / 2);
        const auto center_y = static_cast<kf::Position>(max_y / 2);

        if (x != nullptr) {
            const auto line_x = static_cast<kf::Position>(*x * static_cast<float>(center_x));
            p.line(center_x, center_y, static_cast<kf::Position>(center_x + line_x), center_y); // x

            const auto text = rs::formatted<12>("%+1.2f   X", *x);
            p.text(0, text_offset, text.data());
        }

        if (y != nullptr) {
            const auto line_y = static_cast<kf::Position>(*y * static_cast<float>(center_x));
            p.line(center_x, center_y, center_x, static_cast<kf::Position>(center_y - line_y)); // y

            const auto text = rs::formatted<12>("%+1.2f   Y", *y);
            p.text(0, static_cast<kf::Position>(center_y + text_offset), text.data());
        }
    }
};

struct DualJoyApp : GuiSystem {
    Window joy_window;
    Window init_window;

    JoyWidget left_joy_widget, right_joy_widget;

    static DualJoyApp &instance() {
        static DualJoyApp instance;
        return instance;
    }

private:

    DualJoyApp() = default;

public:

    DualJoyApp(const DualJoyApp &) = delete;
};

void initEspNow() {
    {
        WiFiClass::mode(WIFI_MODE_STA);

        const auto result = espnow::Protocol::init();

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
            return;
        }
    }

    {
        const auto result = espnow::Peer::add(target);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
            return;
        }
    }

    {
        auto on_receive = [](const espnow::Mac &mac, const void *data, rs::u8 size) {
            Serial.printf("[%s] : ", rs::toArrayString(mac).data());
            Serial.print(static_cast<const char *>(data));
        };

        const auto result = espnow::Protocol::instance().setReceiveHandler(on_receive);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
            return;
        }
    }
}

[[noreturn]] void inputTask(void *) {
    auto &app = DualJoyApp::instance();

    kf::Joystick left_joystick{32, 33, 0.8};
    kf::Joystick right_joystick{35, 34, 0.8};

    struct JoyPacket {
        float left_x, left_y;
        float right_x, right_y;
    } packet{};

    app.init_window.title = "Joystick: Left";
    left_joystick.init();
    left_joystick.axis_x.inverted = true;
    left_joystick.axis_y.inverted = false;
    left_joystick.calibrate(100);

    app.init_window.title = "Joystick: Right";
    right_joystick.init();
    right_joystick.axis_y.inverted = true;
    right_joystick.axis_x.inverted = false;
    right_joystick.calibrate(100);

    app.left_joy_widget.x = &packet.left_x;
    app.left_joy_widget.y = &packet.left_y;
    app.joy_window.add(app.left_joy_widget);

    app.right_joy_widget.x = &packet.right_x;
    app.right_joy_widget.y = &packet.right_y;
    app.joy_window.add(app.right_joy_widget);

    app.bindWindow(app.joy_window);

    while (true) {
        packet.left_x = left_joystick.axis_x.read();
        packet.left_y = left_joystick.axis_y.read();

        packet.right_x = right_joystick.axis_x.read();
        packet.right_y = right_joystick.axis_y.read();

        const auto result = espnow::Protocol::send(target, packet);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }

        delay(10);
    }
}

[[noreturn]] void displayTask(void *) {
    auto &app = DualJoyApp::instance();

    constexpr auto target_fps = 25;
    constexpr auto ms_per_frame = 1000 / target_fps;

    kf::SSD1306 display_driver;

    kf::Painter main_gfx{
        kf::FrameView::create(
            display_driver.buffer,
            kf::SSD1306::width,
            kf::SSD1306::width,
            kf::SSD1306::height, 0, 0
        ).value
    };
    main_gfx.setFont(kf::fonts::gyver_5x7_en);

    constexpr auto column_width = 63;
    constexpr auto joy_height = 32;

    const auto column_right_offset = static_cast<kf::Position>(main_gfx.frame.width - column_width - 1);

    kf::Painter left_joy{
        main_gfx.frame.sub(
            column_width,
            joy_height,
            0,
            0
        ).value
    };
    left_joy.setFont(kf::fonts::gyver_5x7_en);
    app.left_joy_widget.bindPainter(left_joy);

    kf::Painter right_joy{
        main_gfx.frame.sub(
            column_width,
            joy_height,
            column_right_offset,
            0
        ).value
    };
    right_joy.setFont(kf::fonts::gyver_5x7_en);
    app.right_joy_widget.bindPainter(right_joy);

    display_driver.init();
    Wire.setClock(1000000UL);

    while (true) {
        delay(ms_per_frame);
        app.render(main_gfx);
        display_driver.update();
    }
}

void setup() {
    auto &app = DualJoyApp::instance();
    app.init_window.title = "Initializing";
    app.bindWindow(app.init_window);

    Serial.begin(115200);

    initEspNow();

    xTaskCreate(displayTask, "display", 4096, nullptr, 1, nullptr);
    xTaskCreate(inputTask, "input", 4096, nullptr, 1, nullptr);
}

void loop() {}