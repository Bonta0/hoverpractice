#pragma once

#include <functional>
#include <string>

#include "controller.h"

bool xinput_init();
void xinput_exit();

using xinput_enum_cb = std::function<void(DWORD dwIndex, const std::string& name)>;
void xinput_enum(const xinput_enum_cb& callback);

Controller* xinput_open(DWORD dwIndex);

void xinput_close(const Controller* controller);
