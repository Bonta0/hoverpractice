#define DIRECTINPUT_VERSION 0x800
#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#include <algorithm>
#include <list>
#include <map>
#include <string>

#include "dinput.h"

LPDIRECTINPUT8 pDInput = nullptr;

class DInputController;
std::list<std::unique_ptr<DInputController>> controllers;

bool dinput_init() {
    auto res = DirectInput8Create(
        GetModuleHandle(NULL),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(&pDInput),
        NULL
    );

    if (res != DI_OK) {
        pDInput = nullptr;
        return false;
    }

    return true;
}

void dinput_exit() {
    controllers.clear();

    if (pDInput != nullptr) {
        pDInput->Release();
    }
    pDInput = nullptr;
}

void dinput_enum(const dinput_enum_cb& callback) {
    if (pDInput == nullptr) {
        return;
    }

    auto enum_devices = [&](DWORD type) {
        return pDInput->EnumDevices(
            type,
            [](LPCDIDEVICEINSTANCE instance, LPVOID reference) {
                auto& callback = *reinterpret_cast<const dinput_enum_cb*>(reference);
                std::wstring wname(instance->tszProductName);
                std::string name;
                std::transform(wname.begin(), wname.end(), std::back_inserter(name), [](const auto& wc) {
                    return static_cast<char>(wc);
                });
                callback(instance->guidInstance, name);
                return DIENUM_CONTINUE;
            },
            reinterpret_cast<LPVOID>(const_cast<dinput_enum_cb*>(&callback)),
            DIEDFL_ATTACHEDONLY
            );
    };

    enum_devices(DI8DEVCLASS_GAMECTRL);
    enum_devices(DI8DEVCLASS_KEYBOARD);
}

class DInputController final : public Controller {
public:
    DInputController(const GUID& guid) : Controller() {
        if (pDInput->CreateDevice(guid, &device, NULL) != DI_OK ||
            device->SetDataFormat(&c_dfDIJoystick2) != DI_OK ||
            device->Acquire() != DI_OK) {
            device = nullptr;
        }
    }

    ~DInputController() {
        if (device != nullptr) {
            device->Unacquire();
            device->Release();
            device = nullptr;
        }
    }

    std::string BindAction(Action action) override {
        if (device == nullptr) {
            return {};
        }

        DIJOYSTATE2 state;
        if (device->GetDeviceState(sizeof(state), reinterpret_cast<LPVOID>(&state)) == DI_OK) {
            for (size_t i = 0; i < std::size(state.rgbButtons); ++i) {
                if (state.rgbButtons[i]) {
                    bindings[action] = i;
                    return "Button " + std::to_string(i + 1);
                }
            }
        }

        return "";
    }

    Action GetState() override {
        if (device == nullptr) {
            return {};
        }

        Action res{};
        DIJOYSTATE2 state;
        if (device->GetDeviceState(sizeof(state), reinterpret_cast<LPVOID>(&state)) == DI_OK) {
            for (auto& pair : bindings) {
                if (state.rgbButtons[pair.second]) {
                    res = static_cast<Action>(res | pair.first);
                }
            }
        }

        return res;
    }

private:
    std::map<Action, size_t> bindings;
    LPDIRECTINPUTDEVICE8 device = nullptr;
};

Controller* dinput_open(const GUID& guidInstance) {
    if (pDInput == nullptr) {
        return nullptr;
    }

    return &*controllers.emplace_back(std::make_unique<DInputController>(guidInstance));
}

void dinput_close(const Controller* controller) {
    controllers.remove_if([&](auto& ptr) {
        return ptr.get() == controller; }
    );
}
