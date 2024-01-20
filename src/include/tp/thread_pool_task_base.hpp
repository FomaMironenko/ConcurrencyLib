#pragma once

class ITaskBase {
public:
    virtual ~ITaskBase() = default;
    virtual void run() = 0;
    virtual bool cancelled() { return false; }
};
