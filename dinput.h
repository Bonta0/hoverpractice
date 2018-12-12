#pragma once

#include <guiddef.h>

#include <functional>
#include <string>

#include "controller.h"

bool dinput_init();
void dinput_exit();

using dinput_enum_cb = std::function<void(const GUID& guidInstance, const std::string& name)>;
void dinput_enum(const dinput_enum_cb& callback);

Controller* dinput_open(const GUID& guidInstance);

void dinput_close(const Controller* controller);
