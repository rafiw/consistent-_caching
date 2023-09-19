#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

#include "process_runner.hpp"
#include <pqxx/pqxx>
#include <sw/redis++/redis++.h>

using namespace std::chrono_literals;

class Client
{
using  redis_keys_deleted = std::vector<std::string>;
    pqxx::connection postgres_;
    sw::redis::Redis redis_;
    redis_keys_deleted key_states_; // map if ip to redis key status
    std::string ip_port_;
    std::unique_ptr<ProcessRunner> pr;
    std::mutex db_lock_;
public:
    Client(std::string postgres_uri, std::string redis_ip);
    // ~Client();
    void change_param(int idx);
    void change_params(std::vector<int> idxs);
    std::string read_param(int idx);
    void populate_db(int num_of_entries, int ttl=0);
    void clear();
    void start_monitor();
    void stop_monitor();
    std::string ip();
    void debug_params_table();
    std::vector<std::string> get_exp_deleted_keys();
private:
    std::string param(int i);
    std::string value(int i);
    std::string next_param();
    std::string next_value();
    
    void drop_postgres_tables();
    void drop_redis_tables();

};