#ifdef _WIN32
#include <tchar.h>
#include <SDKDDKVer.h>
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include <list>
#include <map>
#include <memory>
#include <string>

#include "sdl.h"

class SDLLoader {
public:
    SDLLoader() = default;

    ~SDLLoader() {
        Free();
    }

    bool Load() {
        Free();

#ifdef _WIN32
        hSDL = LoadLibrary(_T("SDL2.dll"));
#else
        hSDL = dlopen("libSDL2.so", RTLD_LAZY);
#endif

        const auto& get_sym = [&](const std::string& name, auto& sym_ptr) -> bool {
#ifdef _WIN32
            auto ptr = GetProcAddress(hSDL, name.c_str());
#else
            auto ptr = dlsym(hSDL, name.c_str());
#endif
            return (sym_ptr = reinterpret_cast<decltype(sym_ptr)>(ptr)) != nullptr;
        };

        if (hSDL == nullptr
            || !get_sym("SDL_Init", Init)
            || !get_sym("SDL_Quit", Quit)
            || !get_sym("SDL_NumJoysticks", NumJoysticks)
            || !get_sym("SDL_JoystickOpen", JoystickOpen)
            || !get_sym("SDL_JoystickClose", JoystickClose)
            || !get_sym("SDL_JoystickNameForIndex", JoystickNameForIndex)
            || !get_sym("SDL_JoystickNumButtons", JoystickNumButtons)
            || !get_sym("SDL_JoystickGetButton", JoystickGetButton)
            || !get_sym("SDL_JoystickUpdate", JoystickUpdate)
            || Init(SDL_INIT_JOYSTICK) != 0) {
            Free();
            return false;
        }

        return true;
    }

    void Free() {
        if (hSDL) {
            if (Quit != nullptr) {
                Quit();
            }
#ifdef _WIN32
            FreeLibrary(hSDL);
            hSDL = NULL;
#else
            dlclose(hSDL);
            hSDL = nullptr;
#endif
        }
        Init = nullptr;
        Quit = nullptr;
        NumJoysticks = nullptr;
        JoystickOpen = nullptr;
        JoystickClose = nullptr;
        JoystickNameForIndex = nullptr;
        JoystickNumButtons = nullptr;
        JoystickGetButton = nullptr;
        JoystickUpdate = nullptr;
    }

    using SDL_Joystick = void;
    static constexpr unsigned int SDL_INIT_JOYSTICK = 0x200;

    int (*Init)(unsigned int) = nullptr;
    int (*Quit)() = nullptr;
    int (*NumJoysticks)() = nullptr;
    SDL_Joystick* (*JoystickOpen)(int) = nullptr;
    void (*JoystickClose)(SDL_Joystick*) = nullptr;
    const char* (*JoystickNameForIndex)(int) = nullptr;
    int (*JoystickNumButtons)(SDL_Joystick*) = nullptr;
    unsigned char (*JoystickGetButton)(SDL_Joystick*, int) = nullptr;
    int (*JoystickUpdate)() = nullptr;

private:
#ifdef _WIN32
    HMODULE hSDL = NULL;
#else
    void* hSDL = nullptr;
#endif
};

std::unique_ptr<SDLLoader> sdl;

class SDLController;
std::list<std::unique_ptr<SDLController>> controllers;

bool sdl_init() {
    sdl = std::make_unique<SDLLoader>();
    if (sdl != nullptr && sdl->Load()) {
        return true;
    }
    sdl.reset();
    return false;
}

void sdl_exit() {
    controllers.clear();
    sdl.reset();
}

void sdl_enum(const sdl_enum_cb& callback) {
    if (sdl == nullptr) {
        return;
    }

    int total = sdl->NumJoysticks();
    for (int i = 0; i < total; ++i) {
        callback(i, sdl->JoystickNameForIndex(i));
    }
}

class SDLController final : public Controller {
public:
    SDLController(int index) : Controller() {
        joystick = sdl->JoystickOpen(index);
        buttons = sdl->JoystickNumButtons(joystick);
    }

    ~SDLController() {
        sdl->JoystickClose(joystick);
    }

    std::string BindAction(Action action) override {
        sdl->JoystickUpdate();
        for (int i = 0; i < buttons; ++i) {
            if (sdl->JoystickGetButton(joystick, i)) {
                bindings[action] = i;
                return "Button " + std::to_string(i);
            }
        }
        return "";
    }

    Action GetState() override {
        sdl->JoystickUpdate();
        Action res{};
        for (auto& pair : bindings) {
            if (sdl->JoystickGetButton(joystick, pair.second)) {
                res = static_cast<Action>(res | pair.first);
            }
        }

        return res;
    }

private:
    std::map<Action, int> bindings;
    SDLLoader::SDL_Joystick* joystick = nullptr;
    int buttons = 0;
};

Controller* sdl_open(int index) {
    if (sdl == nullptr) {
        return nullptr;
    }

    auto controller = std::make_unique<SDLController>(index);
    return &*controllers.emplace_back(std::move(controller));
}

void sdl_close(const Controller* controller) {
    controllers.remove_if([&](auto& ptr) {
        return ptr.get() == controller; }
    );
}
