#pragma once

#include <functional>
#include <string>

#include "controller.h"

bool sdl_init();
void sdl_exit();

using sdl_enum_cb = std::function<void(int index, const std::string & name)>;
void sdl_enum(const sdl_enum_cb& callback);

Controller* sdl_open(int index);

void sdl_close(const Controller* controller);
