#include <stdio.h>
#include <SDKDDKVer.h>
#include <Windows.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "controller.h"
#include "dinput.h"
#include "xinput.h"

#define COLOR_RESET  "\033[0m"
#define BOLD         "\033[1m"
#define UNDERLINE    "\033[4m"
#define BLACK_TEXT   "\033[30;1m"
#define RED_TEXT     "\033[31;1m"
#define GREEN_TEXT   "\033[32;1m"
#define YELLOW_TEXT  "\033[33;1m"
#define BLUE_TEXT    "\033[34;1m"
#define MAGENTA_TEXT "\033[35;1m"
#define CYAN_TEXT    "\033[36;1m"
#define WHITE_TEXT   "\033[37;1m"
#define BLACK_BACKGROUND   "\033[40;1m"
#define RED_BACKGROUND     "\033[41;1m"
#define GREEN_BACKGROUND   "\033[42;1m"
#define YELLOW_BACKGROUND  "\033[43;1m"
#define BLUE_BACKGROUND    "\033[44;1m"
#define MAGENTA_BACKGROUND "\033[45;1m"
#define CYAN_BACKGROUND    "\033[46;1m"
#define WHITE_BACKGROUND   "\033[47;1m"

#define CURSOR_HORIZONTAL_POS(x) "\033[" #x "G"
#define CURSOR_VERTICAL_POS(x)   "\033[" #x "d"

#define CURSOR_UP    "\033A"
#define CURSOR_DOWN  "\033B"
#define CURSOR_RIGHT "\033C"
#define CURSOR_LEFT  "\033D"

#define DELETE_LINE   "\033[K"

BOOL ConsoleSetup() {
    BOOL res = 0;

    const auto in_handle = GetStdHandle(STD_INPUT_HANDLE);
    const auto out_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD in_mode;
    DWORD out_mode;
    CONSOLE_CURSOR_INFO cursor_info;

    res = GetConsoleMode(in_handle, &in_mode);
    if (!res) return res;
    res = GetConsoleMode(out_handle, &out_mode);
    if (!res) return res;
    GetConsoleCursorInfo(out_handle, &cursor_info);
    if (!res) return res;

    in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    cursor_info.bVisible = FALSE;

    res = SetConsoleMode(in_handle, in_mode);
    if (!res) return res;
    res = SetConsoleMode(out_handle, out_mode);
    if (!res) return res;
    SetConsoleCursorInfo(out_handle, &cursor_info);
    if (!res) return res;

    return res;
}

void cleanup() {
    dinput_exit();
    xinput_exit();
}

int main()
{
    if (!dinput_init()) {
        std::cout << "DirectInput initialization failed" << std::endl;
        cleanup();
        return 1;
    }

    if (!xinput_init()) {
        std::cout << "XInput initialization failed" << std::endl;
        cleanup();
        return 1;
    }

    ConsoleSetup();

    std::cout << "-------------------------------" << std::endl;

    std::vector<std::pair<std::variant<DWORD, GUID>, std::string>> devices;

    std::cout << "XInput devices:" << std::endl;
    xinput_enum([&](auto dwIndex, auto& name) {
        devices.emplace_back(dwIndex, name);
        std::cout << " [" << std::to_string(devices.size()) << "] " << name << std::endl;
    });

    std::cout << "DirectInput devices:" << std::endl;
    dinput_enum([&](auto& guidInstance, auto& name) {
        devices.emplace_back(guidInstance, name);
        std::cout << " [" << std::to_string(devices.size()) << "] " << name << std::endl;
    });

    std::cout << "-------------------------------" << std::endl;

    std::cout << "Enter a number" << std::endl;

    int choice;
    std::cin >> choice;
    if (choice < 1 || choice > static_cast<int>(devices.size())) {
        std::cout << "Invalid choice" << std::endl;
        cleanup();
        return 1;
    }

    std::cout << "-------------------------------" << std::endl;

    auto& device_pair = devices[choice - 1];
    std::cout << "Opening \"" << device_pair.second << "\"" <<
        (std::holds_alternative<DWORD>(device_pair.first) ? " (XInput)" : " (DirectInput)") <<
        std::endl;

    auto controller = std::holds_alternative<DWORD>(device_pair.first) ?
        xinput_open(std::get<DWORD>(device_pair.first)) :
        dinput_open(std::get<GUID>(device_pair.first));

    if (controller == nullptr) {
        std::cout << "Failed" << std::endl;
        cleanup();
        return 1;
    }

    std::cout << "-------------------------------" << std::endl;

    const auto& bind_action = [&](const auto& action_str, const auto& action) {
        std::cout << "Press " << action_str << " button" << std::endl;

        std::string button_name;
        while ((button_name = controller->BindAction(action)).empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::cout << action_str << " = \"" << button_name << "\"" << std::endl;

        while (controller->GetState() & action) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    bind_action("Dash", Controller::Action::Dash);
    bind_action("Map", Controller::Action::Map);

    std::cout << "-------------------------------" << std::endl;

    auto prev_state = controller->GetState();
    auto button_time = std::chrono::high_resolution_clock::now();

    std::string output;
    unsigned int event_id = 0;

    for (;; std::this_thread::sleep_for(std::chrono::microseconds(500))) {
        auto state = controller->GetState();

        const auto buttons_event = state ^ prev_state;
        const auto buttons_down = buttons_event & state;
        const auto buttons_up = buttons_event & prev_state;

        const bool isdown = (prev_state & Controller::Action::Dash) != 0;
        const auto current_time = std::chrono::high_resolution_clock::now();
        const auto delta_time = current_time - button_time;

        output.clear();
        if ((buttons_down & Controller::Action::Map) != 0) {
            output += COLOR_RESET "\nMAP\n";
        }
        if ((buttons_down & Controller::Action::Pause) != 0) {
            output += COLOR_RESET "\nPAUSE\n";
        }
        if ((buttons_down & Controller::Action::Menu) != 0) {
            output += COLOR_RESET "\nMENU\n";
        }

        static constexpr auto frame_duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)) / 60;

        if (isdown) {
            output += (delta_time < frame_duration * 32) ? GREEN_TEXT : RED_TEXT;
        } else {
            if (delta_time >= frame_duration * 1.3f || delta_time < frame_duration * 0.2f) output += RED_TEXT;
            else if (delta_time < frame_duration * 0.5f || delta_time > frame_duration) output += YELLOW_TEXT;
            else output += GREEN_TEXT;
        }

        char event_id_buf[10];
        snprintf(event_id_buf, sizeof(event_id_buf), "%03u", event_id);

        output += "\r";
        output += std::string(event_id_buf) + " dash button " + (isdown ? "down" : "up");
        output += " (" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(delta_time).count()) + " ms)";

        output = output.substr(0, 64);
        output.resize(64, ' ');

        if (buttons_event & Controller::Action::Dash) {
            output += "\n";
            button_time = current_time;
            event_id = (event_id + 1) % 1000;
        }
        std::cout << output;

        prev_state = state;
    }

    cleanup();
    return 0;
}

