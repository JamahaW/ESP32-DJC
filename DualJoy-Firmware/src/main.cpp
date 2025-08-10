#include <WiFi.h>
#include "Arduino.h"

#include "kf/GFX.hpp"
#include "kf/SSD1306.h"
#include "kf/Joystick.hpp"

#include "espnow/Protocol.hpp"


static constexpr espnow::Mac target = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct Window {
    virtual void render(kf::Painter &p) const noexcept = 0;
};

struct GUI {

private:

    Window *current_window{nullptr};

public:

    static GUI &instance() noexcept {
        static GUI instance;

        return instance;
    }

    void bindWindow(Window &window) noexcept {
        current_window = &window;
    }

    void render(kf::Painter &painter) noexcept {
        if (current_window == nullptr) { return; }

        painter.fill(false);

        current_window->render(painter);
    }

private:

    GUI() = default;

public:

    GUI(const GUI &) = delete;

};

struct InitWindow : Window {

    static InitWindow &instance() noexcept {
        static InitWindow instance;
        return instance;
    }

    void render(kf::Painter &p) const noexcept override {
        p.text(0, 0, "Init");
    }

private:

    InitWindow() = default;
};

struct JoyWidget final {

    const float *x{nullptr};
    const float *y{nullptr};

    void render(kf::Painter &p) const noexcept {
        const auto r = static_cast<kf::Position>(std::min(p.frame.width - 1, p.frame.height - 1) / 2);
        p.circle(r, r, r, kf::Painter::Mode::FillBorder);

        if (x == nullptr or y == nullptr) { return; }

        const auto line_x = static_cast<kf::Position>(static_cast<float>(r) + *x * static_cast<float>(r));
        const auto line_y = static_cast<kf::Position>(static_cast<float>(r) - *y * static_cast<float>(r));

        p.line(r, r, line_x, line_y);
    }
};

struct AxisWidget final {

    static constexpr kf::Position width = 8;

    const float *value{nullptr};

    void render(kf::Painter &p) const noexcept {
        const auto x_end = static_cast<kf::Position>(p.frame.width - 1);
        const auto y_end = static_cast<kf::Position>(p.frame.height - 1);
        const auto x_start = static_cast<kf::Position>(x_end - width);

        if (value == nullptr) {
            p.rect(x_start, 0, x_end, y_end, kf::Painter::Mode::FillBorder);
            return;
        }

        const auto y_center = static_cast<kf::Position>(y_end / 2);
        const auto height = static_cast<kf::Position>(static_cast<float>(y_center) - *value * static_cast<float>(y_center));

        p.rect(x_start, y_center, x_end, height, kf::Painter::Mode::Fill);
    }
};

struct JoyControlWindow : Window {

    JoyWidget joy_widget;
    AxisWidget axis_widget;

    static JoyControlWindow &instance() noexcept {
        static JoyControlWindow instance;
        return instance;
    }

    void render(kf::Painter &p) const noexcept override {
        joy_widget.render(p);
        axis_widget.render(p);
    }

private:

    JoyControlWindow() = default;
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
    static auto left_joystick = kf::Joystick(32, 33, 0.9);
    static auto right_axis = kf::AnalogAxis(34, 0.6);

    struct JoyPacket {
        float x, y, a;
    } packet{};

    left_joystick.init();
    right_axis.init();

    left_joystick.axis_x.inverted = true;
    left_joystick.axis_y.inverted = false;
    right_axis.inverted = true;

    left_joystick.calibrate(100);
    {
        int s = 0;

        for (int i = 0; i < 100; i++) {
            s += right_axis.readRaw();
            delay(1);
        }
        s /= 100;
        right_axis.updateCenter(s);
    }

    auto &joy = JoyControlWindow::instance();
    joy.axis_widget.value = &packet.a;
    joy.joy_widget.x = &packet.x;
    joy.joy_widget.y = &packet.y;

    GUI::instance().bindWindow(joy);

    while (true) {
        const auto left_data = left_joystick.read();
        packet.a = right_axis.read();

        packet.x = left_data.x;
        packet.y = left_data.y;

        const auto result = espnow::Protocol::send(target, packet);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }

        delay(10);
    }
}

[[noreturn]] void displayTask(void *) {
    constexpr auto target_fps = 20;
    constexpr auto ms_per_frame = 1000 / target_fps;

    static auto display_driver = kf::SSD1306();

    static auto main_gfx = kf::Painter(kf::FrameView::create(
        display_driver.buffer,
        kf::SSD1306::width,
        kf::SSD1306::width,
        kf::SSD1306::height, 0, 0
    ).value);

    main_gfx.setFont(kf::fonts::gyver_5x7_en);

    display_driver.init();

    auto &gui = GUI::instance();

    while (true) {
        delay(ms_per_frame);
        gui.render(main_gfx);
        display_driver.update();
    }
}

void setup() {
    GUI::instance().bindWindow(InitWindow::instance());

    Serial.begin(115200);

    initEspNow();

    xTaskCreate(displayTask, "display", 4096, nullptr, 1, nullptr);
    xTaskCreate(inputTask, "input", 4096, nullptr, 1, nullptr);
}

void loop() {}