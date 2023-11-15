#pragma once

class ITaskBase {
public:
    virtual ~ITaskBase() = default;
    virtual void run() = 0;
};
