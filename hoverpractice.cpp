// hovertest.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>
#include <SDKDDKVer.h>
#include <Windows.h>

#include <Xinput.h>
#define XINPUT_GAMEPAD_GUIDE 0x400
#define AXIS_MIN -32768
#define AXIS_MAX 32767

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#define PAD_ID 0
#define HOVER_BUTTON XINPUT_GAMEPAD_B
#define MAP_BUTTON XINPUT_GAMEPAD_Y
#define MENU_BUTTON XINPUT_GAMEPAD_START
#define PAUSE_BUTTON XINPUT_GAMEPAD_BACK

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

class XInputWrapper {
public:
    XInputWrapper() = default;

    bool Load() {
        Free();

        hXInput = LoadLibrary(_T("xinput1_4.dll"));
        if (!hXInput) hXInput = LoadLibrary(_T("xinput1_3.dll"));

        if (hXInput) {
            pXInputGetState = (LPXINPUTGETSTATE)GetProcAddress(hXInput, (LPCSTR)100); // XInputGetStateEx
        } else {
            hXInput = LoadLibrary(_T("XInput9_1_0.dll"));
            pXInputGetState = NULL;
        }

        if (hXInput && !pXInputGetState) {
            pXInputGetState = (LPXINPUTGETSTATE)GetProcAddress(hXInput, "XInputGetState");
        }

        if (!pXInputGetState) {
            Free();
            return false;
        }

        return true;
    }

    void Free() {
        if (hXInput) {
            FreeLibrary(hXInput);
            hXInput = NULL;
        }
        pXInputGetState = NULL;
    }

    DWORD WINAPI GetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
        if (pXInputGetState)
            return pXInputGetState(dwUserIndex, pState);

        return NULL;
    }

private:
    HMODULE hXInput = NULL;

    using LPXINPUTGETSTATE = DWORD(WINAPI*) (DWORD dwUserIndex, XINPUT_STATE* pState);
    LPXINPUTGETSTATE pXInputGetState = NULL;
};

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

int main()
{
    XInputWrapper xinput;
    if (!xinput.Load()) return 1;

    ConsoleSetup();

    XINPUT_STATE xinputPrevState;
    auto button_time = std::chrono::high_resolution_clock::now();
    xinput.GetState(PAD_ID, &xinputPrevState);

    std::string output;
    unsigned int event_id = 0;

    for (;; std::this_thread::sleep_for(std::chrono::microseconds(500))) {
        XINPUT_STATE state;
        if (xinput.GetState(PAD_ID, &state) != ERROR_SUCCESS) {
            continue;
        }

        const auto buttons_event = state.Gamepad.wButtons ^ xinputPrevState.Gamepad.wButtons;
        const auto buttons_down = buttons_event & state.Gamepad.wButtons;
        const auto buttons_up = buttons_event & xinputPrevState.Gamepad.wButtons;

        const bool isdown = (xinputPrevState.Gamepad.wButtons & HOVER_BUTTON) != 0;
        const auto current_time = std::chrono::high_resolution_clock::now();
        const auto delta_time = current_time - button_time;

        output.clear();
        if ((buttons_down & MAP_BUTTON) != 0) {
            output += COLOR_RESET "\nNice MAP\n";
        }
        if ((buttons_down & PAUSE_BUTTON) != 0) {
            output += COLOR_RESET "\nPAUSE\n";
        }
        if ((buttons_down & MENU_BUTTON) != 0) {
            output += COLOR_RESET "\nMENU time\n";
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

        output += "\r";// CURSOR_HORIZONTAL_POS(1);
        output += std::string(event_id_buf) + " dash button " + (isdown ? "down" : "up");
        output += " (" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(delta_time).count()) + " ms)";

        output = output.substr(0, 64);
        output.resize(64, ' ');

        if (buttons_event & HOVER_BUTTON) {
            output += "\n";
            button_time = current_time;
            event_id = (event_id + 1) % 1000;
        }
        std::cout << output;

        xinputPrevState = state;
    }

    return 0;
}

