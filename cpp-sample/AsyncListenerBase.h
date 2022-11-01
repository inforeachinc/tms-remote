#pragma once

#include <string>

class AsyncListenerBase
{
public:
    virtual ~AsyncListenerBase();
    virtual void setDebug(bool debug) = 0;
    virtual void signalStop(const std::string &caller) = 0;
    virtual void waitForStop(const std::string &caller) = 0;
};
