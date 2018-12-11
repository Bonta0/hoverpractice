#pragma once

#include <string>

class Controller {
public:
    Controller() = default;
    virtual ~Controller() = default;

    enum Action {
        Dash =  1 << 0,
        Slash = 1 << 1,
        Item =  1 << 2, 
        Map =   1 << 3,
        Menu =  1 << 4,
        Pause = 1 << 5
    };

    virtual std::string BindAction(Action action) = 0;
    virtual Action GetState() = 0;
};
