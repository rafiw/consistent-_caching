
#include <iostream>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "process_runner.hpp"

ProcessRunner::ProcessRunner(const std::string& path, const std::vector<std::string>& args) :
    path_(path),
    args_(args),
    pid_(0)
    {}

void ProcessRunner::start()
{
    thread_ = std::thread(&ProcessRunner::run_process, this);
}

void ProcessRunner::stop() {
    if (pid_ > 0) {
        kill(pid_, SIGKILL);
        thread_.join();
    }
}

std::string ProcessRunner::get_output() {
    return output_;
}

void ProcessRunner::run_process() {
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        std::cerr << "Failed to create pipe." << std::endl;
        return;
    }

    pid_ = fork();

    if (pid_ < 0) {
        std::cerr << "Failed to fork." << std::endl;
        return;
    }

    // Child process
    if (pid_ == 0) {
        close(pipefd[0]); // Close read-end, as we're going to write to it
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the pipe
        close(pipefd[1]); // This descriptor is no longer needed
        if (execle(path_.c_str(), args_[0].c_str(), args_[1].c_str(), args_[2].c_str(), args_[3].c_str(), (char*)nullptr, environ) == -1) {
            perror("execl error\n");
        }
        std::cout <<"child failed " << errno;
        exit(EXIT_FAILURE); // Exit if exec() fails
    }

    // Parent process
    close(pipefd[1]); // Close write-end, as we're reading from it

    char buffer[128];
    ssize_t bytesRead;
    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) != 0) {
        if (bytesRead == -1) {
            std::cerr << "Failed to read from pipe." << std::endl;
            break;
        }
        output_.append(buffer, bytesRead);
    }

    close(pipefd[0]);
    int status;
    waitpid(pid_, &status, 0);
}

