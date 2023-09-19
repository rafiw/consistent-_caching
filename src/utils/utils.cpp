#include <string>
#include <map>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include "utils.hpp"


std::chrono::system_clock::time_point parse_time(const std::string& timeStr) {
    std::tm tm = {};
    std::istringstream ss(timeStr);

    // Parse year, month, day, hours, minutes, seconds
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    // Skip the '.' 
    ss.ignore();

    // Parse milliseconds or microseconds (assumes at least one digit)
    int subseconds = 0;
    ss >> subseconds;

    // Convert tm to time_point (without subseconds)
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

   // If we have 5 digits, it's 10^5, if 6 digits, it's 10^6
    int subsecDigits = timeStr.find('+') - timeStr.find('.') - 1;
    if (subsecDigits == 5) {
        tp += std::chrono::duration<int, std::ratio<1, 100000>>(subseconds);
    } else if (subsecDigits == 6) {
        tp += std::chrono::duration<int, std::ratio<1, 1000000>>(subseconds);
    }

    // Parse time zone offset
    char sign;
    int hoursOffset, minutesOffset = 0;
    ss >> sign >> hoursOffset;
    if (ss.peek() == ':') {
        ss.ignore(); // skip the ':'
        ss >> minutesOffset;
    }

    if (sign == '+') {
                     // - 2 hour hack
        tp -= std::chrono::hours(hoursOffset - 2) + std::chrono::minutes(minutesOffset);
    } else if (sign == '-') {
        tp += std::chrono::hours(hoursOffset) + std::chrono::minutes(minutesOffset);
    } else {
        tp -= std::chrono::hours(2);  // HACK
    }

    return tp;
}
/**
 * rwiener;x.x.x.x:yyyy -> {rwiener:x.x.x.x:yyyy,...}
*/
std::map<std::string, std::string> parse_redis_data(std::string redis_data)
{
    std::map<std::string, std::string> redis_map;
    std::stringstream ss(redis_data);
    std::string pair;
    while (std::getline(ss, pair, ',')) {
        // Split each pair by colons to separate key and value
        std::size_t colonPos = pair.find('@');
        if (colonPos != std::string::npos) {
            std::string key = pair.substr(0, colonPos);
            std::string value = pair.substr(colonPos + 1);

            // Insert the key-value pair into the map
            redis_map[key] = value;
        }
    }
    return redis_map;
}