#include <WiFi.h>
#include "Arduino.h"

#include "KiraFlux-GUI.hpp"

#include "kf/SSD1306.h"
#include "kf/Joystick.hpp"
#include "kf/Button.hpp"

#include "espnow/Protocol.hpp"
#include "gui/JoyWidget.hpp"
#include "gui/FlagDisplay.hpp"


static constexpr espnow::Mac broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


struct DualJoyApp final : kfgui::WindowsManager {
    kfgui::Window joy_window;
    kfgui::Window init_window;

    JoyWidget left_joy_widget;
    JoyWidget right_joy_widget;
    FlagDisplay toggle_mode;

    static DualJoyApp &instance() noexcept {
        static DualJoyApp instance;
        return instance;
    }

private:
    DualJoyApp() = default;

public:
    DualJoyApp(const DualJoyApp &) = delete;
};

static void onEspNowReceive(const espnow::Mac &mac, const void *data, rs::u8 size) noexcept {
    Serial.printf("[%s] : ", rs::toArrayString(mac).data());
    Serial.print(static_cast<const char *>(data));
}

static void initEspNow() noexcept {
    if (not WiFiClass::mode(WIFI_MODE_STA)) {
        return;
    }

    if (const auto result = espnow::Protocol::init(); result.fail()) {
        Serial.println(rs::toString(result.error));
        return;
    }

    if (const auto result = espnow::Peer::add(broadcast); result.fail()) {
        Serial.println(rs::toString(result.error));
        return;
    }

    if (const auto result = espnow::Protocol::instance().setReceiveHandler(onEspNowReceive); result.fail()) {
        Serial.println(rs::toString(result.error));
        return;
    }
}

[[noreturn]] static void inputTask(void *) noexcept {
    constexpr auto rate_hz = 20;

    static struct {
        float left_x, left_y;
        float right_x, right_y;
        bool toggle_mode;
    } packet{};

    kf::Button left_button{GPIO_NUM_15, kf::Button::Mode::PullUp};
    left_button.init(false);

    left_button.handler = []() {
        packet.toggle_mode ^= 1;
    };

    kf::Button right_button{GPIO_NUM_4, kf::Button::Mode::PullUp};
    right_button.init(false);

    right_button.handler = []() {

    };

    kf::Joystick left_joystick{GPIO_NUM_32, GPIO_NUM_33, 0.8f};
    left_joystick.init();
    left_joystick.axis_x.inverted = true;
    left_joystick.axis_y.inverted = false;
    left_joystick.calibrate(1000);

    kf::Joystick right_joystick{GPIO_NUM_35, GPIO_NUM_34, 0.8f};
    right_joystick.init();
    right_joystick.axis_y.inverted = true;
    right_joystick.axis_x.inverted = false;
    right_joystick.calibrate(1000);

    auto &app = DualJoyApp::instance();
    app.left_joy_widget.bindAxis(packet.left_x, packet.left_y);
    app.right_joy_widget.bindAxis(packet.right_x, packet.right_y);
    app.toggle_mode.flag = &packet.toggle_mode;
    app.bindWindow(app.joy_window);

    while (true) {
        packet.left_x = left_joystick.axis_x.read();
        packet.left_y = left_joystick.axis_y.read();
        packet.right_x = right_joystick.axis_x.read();
        packet.right_y = right_joystick.axis_y.read();

        left_button.poll();
        right_button.poll();

        if (const auto result = espnow::Protocol::send(broadcast, packet); result.fail()) {
            Serial.println(rs::toString(result.error));
        }

        constexpr auto period_ms = 1000 / rate_hz;
        delay(period_ms);
    }
}

[[noreturn]] static void displayTask(void *) noexcept {
    constexpr auto target_fps = 25;

    kf::SSD1306 display_driver{};

    kf::Painter main_painter{
        kf::FrameView{
            display_driver.buffer, kf::SSD1306::width,
            kf::SSD1306::width, kf::SSD1306::height, 0, 0
        },
        kf::fonts::gyver_5x7_en
    };

    auto [up, down] = main_painter.splitVertically<2>({7, 1});
    auto [left_joy, right_joy] = up.splitHorizontally<2>({});

    auto &app = DualJoyApp::instance();
    app.left_joy_widget.bindPainter(left_joy);
    app.right_joy_widget.bindPainter(right_joy);
    app.toggle_mode.bindPainter(down);

    display_driver.init();
    Wire.setClock(1000000UL);

    while (true) {
        app.render(main_painter);
        display_driver.update();

        constexpr auto frame_period_ms = 1000 / target_fps;
        delay(frame_period_ms);
    }
}

void setup() {
    auto &app = DualJoyApp::instance();
    app.joy_window.add(app.left_joy_widget);
    app.joy_window.add(app.right_joy_widget);
    app.joy_window.add(app.toggle_mode);

    app.toggle_mode.label = "Toggle";

    app.init_window.title = "Initializing";
    app.bindWindow(app.init_window);

    Serial.begin(115200);

    initEspNow();

    xTaskCreate(displayTask, "display", 4096, nullptr, 1, nullptr);
    xTaskCreate(inputTask, "input", 4096, nullptr, 1, nullptr);
}

void loop() {}