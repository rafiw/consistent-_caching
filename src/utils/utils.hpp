#pragma once

#include <string>
#include <chrono>

std::chrono::system_clock::time_point parse_time(const std::string& timeStr);

std::map<std::string, std::string> parse_redis_data(std::string redis_data);