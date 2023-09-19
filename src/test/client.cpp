#include <map>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#include <pqxx/pqxx>
#include <sw/redis++/redis++.h>

#include "process_runner.hpp"

#include "client.hpp"
#include "utils.hpp"

const std::string data_table = "parameter_data";

const std::array<const char*, 3> gPostgresTables = {{
    "locked_params",
    "read_log",
    "parameter_data"
}};

std::atomic<int> counter;

Client::Client(std::string postgres_uri, std::string redis_ip):
    postgres_(postgres_uri),
    redis_("tcp://" + redis_ip),
    ip_port_(redis_ip)
{
    // Constructor code if needed
}

// Client::~Client()
// {
//     stop();
//     postgres_.close();
//     redis_.close();
// }

std::string Client::ip()
{
    size_t pos = ip_port_.find(':');
    if (pos == std::string::npos) {
        throw std::invalid_argument("Invalid format. Expected IPv4:port.");
    }
    return ip_port_.substr(0, pos);
}

void Client::start_monitor()
{
    std::string cmd = "/usr/bin/redis-cli";
    std::vector<std::string> args = { "/usr/bin/redis-cli", "-h", ip(), "MONITOR"};
    pr = std::make_unique<ProcessRunner>(cmd, args);
    pr->start();
}

std::vector<std::string> Client::get_exp_deleted_keys()
{
std::vector<std::string> extractedParams;
    std::istringstream stream(pr->get_output());
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("DEL") != std::string::npos) {
            std::istringstream iss(line);
            std::string word;
            std::string lastWord;

            while (iss >> std::quoted(word)) {
                lastWord = word;
            }

            extractedParams.push_back(lastWord);
        }
    }

    return extractedParams;
}

void Client::stop_monitor()
{
    pr->stop();
}

void Client::change_param(int idx)
{
    auto value = next_value();
    auto parameter = param(idx);
    std::string query = "UPDATE " + data_table +  " SET parameter_value = $1, timestamp=NOW() WHERE parameter_name = $2";
    {
        std::lock_guard<std::mutex> lock(db_lock_);
        pqxx::work txn(postgres_);
        txn.exec_params(query, value, parameter);
        txn.commit();
    }
    redis_.del(parameter);
}

void Client::change_params(std::vector<int> idxs)
{
    std::string query = "UPDATE " + data_table +  " SET timestamp=NOW(), parameter_value = CASE ";
    std::string params = "(";
    for (const auto idx : idxs) {
        auto value =  postgres_.quote(next_value());
        auto parameter = postgres_.quote(param(idx));
        query += "WHEN parameter_name = " + parameter + " THEN " + value +" ";
        params += parameter +",";
    }
    params.pop_back();
    params += ")";
    query += " ELSE parameter_value END";
    query += "WHERE parameter_name IN  " + params;
    {
        std::lock_guard<std::mutex> lock(db_lock_);
        pqxx::work txn(postgres_);
        txn.exec(query);
        txn.commit();
    }
    for (const auto idx : idxs) {
        auto parameter = param(idx);
        redis_.del(parameter);
    }
}

void Client::debug_params_table()
{
    pqxx::result result;
    {
        std::lock_guard<std::mutex> lock(db_lock_);
        pqxx::work txn(postgres_);
        result = txn.exec("SELECT * FROM " + data_table);
    }
    for (const auto& r : result) {
        std::cout << r["parameter_name"].c_str() << " " << r["timestamp"].c_str() << " " << r["ttl"].c_str() << std::endl;
    }
}

/*
 * this function reads keys from redis, if they don't exist it reads them from the database and then updates the redis
*/
std::string Client::read_param(int idx)
{
    auto parameter = param(idx);
    {
        auto val = redis_.get(parameter);
        if (val) {
            return *val;
        }
    }
    pqxx::row result;
    {
        std::lock_guard<std::mutex> lock(db_lock_);
        pqxx::work txn(postgres_);
        result = txn.exec1("SELECT ttl,timestamp,parameter_value FROM get_parameter(" + postgres_.quote(parameter) + ")");
        txn.commit(); // will trigger write to read_time table
    }

    std::string timestamp_str = result["timestamp"].c_str();
    double ttl_ms = result["ttl"].as<double>();
    std::string val = result["parameter_value"].c_str();

    // Calculate the TTL in milliseconds
    // long long ttl_duration_ms = static_cast<long long>(ttl_ms);

    // Create a system_clock time_point with milliseconds
    std::chrono::system_clock::time_point param_eol_time = parse_time(timestamp_str) + std::chrono::microseconds((long long)(ttl_ms * 1000));
    auto now = std::chrono::system_clock::now();
    if (param_eol_time > now + std::chrono::milliseconds{50}) {
        auto t = std::chrono::duration_cast<std::chrono::milliseconds>(param_eol_time - now);
        // std::cout << "setting key " << parameter << " for another " << t.count() << " ms" << std::endl;
        redis_.psetex(parameter, t, val);
    }

    return val;
}

void Client::clear()
{
    this->drop_postgres_tables();
    this->drop_redis_tables();
    counter = 0;
}

void Client::drop_postgres_tables()
{
    pqxx::work txn(postgres_);
    for (const auto &table : gPostgresTables) {
        txn.exec(std::string("DELETE FROM ") + table);
    }
    txn.commit();
}

void Client::drop_redis_tables()
{
    redis_.flushall();
}

void Client::populate_db(int num_of_entries, int ttl)
{
    try {
        pqxx::work txn(postgres_);
        std::string sql = "INSERT INTO " + data_table + " (parameter_name, parameter_value, ttl, timestamp) VALUES ";

        for (int i = counter; i < counter + num_of_entries; ++i) {
            auto temp_ttl = ttl + static_cast<double>(rand()) / RAND_MAX * 1000.0;  // Random TTL value
            sql += "(" + txn.quote(param(i)) + ","
                 + txn.quote(value(i)) + ","
                 + txn.quote(temp_ttl) + ", NOW()),";
        }
        sql.pop_back();
        txn.exec(sql);
        counter += num_of_entries;
        // Commit the transaction to insert data
        txn.commit();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

std::string Client::param(int i)
{
    return std::string("Parameter_") + std::to_string(i);
}

std::string Client::next_param()
{
    int val = counter++;
    return std::string("Parameter_") + std::to_string(val);
}

std::string Client::value(int i)
{
    return std::string("Value_") + std::to_string(i);
}

std::string Client::next_value()
{
    int val = counter++;
    return std::string("Value_") + std::to_string(val);
}