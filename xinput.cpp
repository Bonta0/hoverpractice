#include <tchar.h>
#include <SDKDDKVer.h>
#include <Windows.h>

#include <Xinput.h>
#define XINPUT_GAMEPAD_GUIDE 0x400
#define AXIS_MIN -32768
#define AXIS_MAX 32767

#include <list>
#include <map>
#include <memory>

#include "xinput.h"

class XInputLoader {
public:
    XInputLoader() = default;

    ~XInputLoader() {
        Free();
    }

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

        if (hXInput) {
            pXInputGetCapabilities = (LPXINPUTGETCAPABILITIES)GetProcAddress(hXInput, "XInputGetCapabilities");
        }

        if (!pXInputGetState || !pXInputGetCapabilities) {
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
        pXInputGetCapabilities = NULL;
    }

    DWORD GetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
        if (pXInputGetState) {
            return pXInputGetState(dwUserIndex, pState);
        }

        return NULL;
    }

    DWORD GetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities) {
        if (pXInputGetCapabilities) {
            return pXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);
        }
    
        return NULL;
    }

private:
    HMODULE hXInput = NULL;

    using LPXINPUTGETSTATE = DWORD(WINAPI*) (DWORD dwUserIndex, XINPUT_STATE* pState);
    LPXINPUTGETSTATE pXInputGetState = NULL;

    using LPXINPUTGETCAPABILITIES = DWORD(WINAPI*) (DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
    LPXINPUTGETCAPABILITIES pXInputGetCapabilities = NULL;
};

std::unique_ptr<XInputLoader> xinput;

class XInputController;
std::list<std::unique_ptr<XInputController>> controllers;

bool xinput_init() {
    xinput = std::make_unique<XInputLoader>();
    if (xinput != nullptr && xinput->Load()) {
        return true;
    }
    xinput.reset();
    return false;
}

void xinput_exit() {
    controllers.clear();
    xinput.reset();
}

void xinput_enum(const xinput_enum_cb& callback) {
    for (DWORD i = 0; i < 4; ++i) {
        XINPUT_CAPABILITIES caps{};
        if (xinput->GetCapabilities(i, 0, &caps) != ERROR_SUCCESS) {
            continue;
        }

        std::string name;

        switch (caps.SubType) {
        case XINPUT_DEVSUBTYPE_GAMEPAD:
            name = "Gamepad controller";
            break;
        case XINPUT_DEVSUBTYPE_WHEEL:
            name = "Racing wheel controller";
            break;
        case XINPUT_DEVSUBTYPE_ARCADE_STICK:
            name = "Arcade stick controller";
            break;
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
            name = "Flight stick controller";
            break;
        case XINPUT_DEVSUBTYPE_DANCE_PAD:
            name = "Dance pad controller";
            break;
        case XINPUT_DEVSUBTYPE_GUITAR:
        case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
        case XINPUT_DEVSUBTYPE_GUITAR_BASS:
            name = "Guitar controller";
            break;
        case XINPUT_DEVSUBTYPE_DRUM_KIT:
            name = "Drum controller";
            break;
        case XINPUT_DEVSUBTYPE_ARCADE_PAD:
            name = "Arcade pad controller";
            break;
        case XINPUT_DEVSUBTYPE_UNKNOWN:
        default:
            name = "Unknown";
            break;
        }
        name += " (" + std::to_string(i + 1) + ")";

        callback(i, name);
    }
}

static const std::map<WORD, std::string_view> XINPUT_BUTTONS{
    {XINPUT_GAMEPAD_DPAD_UP, "D-Pad Up"},
    {XINPUT_GAMEPAD_DPAD_DOWN, "D-Pad Down"},
    {XINPUT_GAMEPAD_DPAD_LEFT, "D-Pad Left"},
    {XINPUT_GAMEPAD_DPAD_RIGHT, "D-Pad Right"},
    {XINPUT_GAMEPAD_START, "Start"},
    {XINPUT_GAMEPAD_BACK, "Back"},
    {XINPUT_GAMEPAD_LEFT_THUMB, "LS"},
    {XINPUT_GAMEPAD_RIGHT_THUMB, "RS"},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"},
    {XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"},
    {XINPUT_GAMEPAD_A, "A"},
    {XINPUT_GAMEPAD_B, "B"},
    {XINPUT_GAMEPAD_X, "X"},
    {XINPUT_GAMEPAD_Y, "Y"},
    {XINPUT_GAMEPAD_GUIDE, "Guide"}
};

class XInputController final : public Controller {
public:
    XInputController(DWORD dwIndex) : Controller(), id(dwIndex) {}
    ~XInputController() = default;

    std::string BindAction(Action action) override {
        XINPUT_STATE state;
        if (xinput->GetState(id, &state) == ERROR_SUCCESS) {
            for (auto& pair : XINPUT_BUTTONS) {
                if (state.Gamepad.wButtons & pair.first) {
                    bindings[action] = pair.first;
                    return std::string(pair.second);
                }
            }
        }
        return "";
    }

    Action GetState() override {
        Action res{};
        XINPUT_STATE state;
        if (xinput->GetState(id, &state) == ERROR_SUCCESS) {
            for (auto& pair : bindings) {
                if (state.Gamepad.wButtons & pair.second) {
                    res = static_cast<Action>(res | pair.first);
                }
            }
        }

        return res;
    }

private:
    std::map<Action, WORD> bindings;
    DWORD id;
};

Controller* xinput_open(DWORD dwIndex) {
    if (xinput == nullptr || (dwIndex < 0 && dwIndex >= 4)) {
        return nullptr;
    }

    return &*controllers.emplace_back(std::make_unique<XInputController>(dwIndex));
}

void xinput_close(const Controller* controller) {
    controllers.remove_if([&] (auto& ptr) {
        return ptr.get() == controller; }
    );
}
