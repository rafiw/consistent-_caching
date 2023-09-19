#pragma once

#include <iostream>
#include <vector>
#include <thread>

#include <sys/types.h>


class ProcessRunner
{
    std::thread thread_;
    std::string path_;
    std::vector<std::string> args_;
    pid_t pid_;
    std::string output_;
public:
    ProcessRunner(const std::string& cmd, const std::vector<std::string>& args);
    void start();
    void stop();
    std::string get_output();
private:
    void run_process();

};
