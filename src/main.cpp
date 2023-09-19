#include <map>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#include <pqxx/pqxx>
#include <sw/redis++/redis++.h>

#include "CLI/CLI.hpp"
#include "utils.hpp"

const bool DEBUG = false;
const char* channel = "data_update";
const std::chrono::milliseconds time_uncertainty_ms{500};

void handle_event(sw::redis::Redis &redis_conn, std::string key) {
    redis_conn.del(key);
}

class NotificationHandler : public pqxx::notification_receiver {
    std::map<std::string, sw::redis::Redis> redis_connections;
    std::atomic<int> queries_saved_;
    std::atomic<int> total_queries_;
public:
    NotificationHandler(pqxx::connection_base & c, const std::string & channel, std::map<std::string, std::string>& redis_data)
        : pqxx::notification_receiver(c, channel),
        queries_saved_(0),
        total_queries_(0)
    {
        std::for_each(redis_data.begin(), redis_data.end(), [this](auto &elem) {
            redis_connections.emplace(std::pair<std::string, sw::redis::Redis>(elem.first, "tcp://" + elem.second + "?keep_alive=true"));
        });
    }
    int get_queries_saved() { return queries_saved_; }
    int get_total_queries() { return total_queries_; }

    void operator() (const std::string & payload, int pid) override
    {
        const std::string data_table = "parameter_data";
        const std::string log_table = "read_log";
        std::map<std::string, std::chrono::system_clock::time_point> user_to_read_times;
        std::string query = "SELECT username, read_timestamp FROM " + log_table + " WHERE parameter_name = $1";

        pqxx::read_transaction txn(conn());
        pqxx::result result = txn.exec_params(query, payload);

        // Check if a row was returned
        if (result.empty()) {
            queries_saved_++;
            return;
        }

        // Iterate through the rows and populate the dictionary
        for (const auto &row : result) {
            auto username = row["username"].c_str();
            std::string timestamp_str = row["read_timestamp"].c_str();
            user_to_read_times[username] = parse_time(timestamp_str);
        }
        // get the TS data is still valid
        query = "SELECT ttl, timestamp FROM " + data_table + " WHERE parameter_name = $1";
        result = txn.exec_params(query, payload);
        if (result.empty()) {
            std::cerr << "No matching rows found for parameter_name1: " << payload << std::endl;
            return;
        }
        double ttl_ms = result[0]["ttl"].as<double>();
        std::string timestamp_str = result[0]["timestamp"].c_str();


        // Create a system_clock time_point with microseconds
        std::chrono::system_clock::time_point param_eol_time = parse_time(timestamp_str);
        param_eol_time += std::chrono::microseconds((long long)(ttl_ms));
        auto now = std::chrono::system_clock::now();
        std::vector<std::thread> threads;
        // need to send notifications only to the relavent Redis servers
        for (auto& [username, conn] : redis_connections) {
            total_queries_++;
            auto iter = user_to_read_times.find(username);
            if (iter != user_to_read_times.end() && (now + time_uncertainty_ms) < param_eol_time) {
                if (DEBUG) {
                    std::cout << "user:" << username << " key:" << payload << " deleted " << "t1:" <<
                            now.time_since_epoch().count() << " t2 " << param_eol_time.time_since_epoch().count() << " delta " <<
                            std::max(now.time_since_epoch().count(), param_eol_time.time_since_epoch().count())  -
                            std::min(now.time_since_epoch().count(), param_eol_time.time_since_epoch().count())<< std::endl;
                }
                threads.emplace_back([payload, &conn]() {
                    handle_event(conn, payload);
                });
            }
            else
            {
                queries_saved_++;
                if (iter == user_to_read_times.end())
                {
                    if (DEBUG) {
                        std::cout << "user:" << username << " key:" << payload << " not deleted " << std::endl;
                    }
                }
                else
                {
                    if (DEBUG) {
                        auto redis_t_val = conn.get(payload);
                        std::string redis_val;
                        if (redis_t_val) {
                            redis_val = *redis_t_val;
                        } else {
                            redis_val = "None";
                        }
                        std::cout << "user:" << username << " key:" << payload << " not deleted " << "t1:" <<
                            now.time_since_epoch().count() << " t2 " << param_eol_time.time_since_epoch().count() << " delta " <<
                            std::max(now.time_since_epoch().count(), param_eol_time.time_since_epoch().count())  -
                            std::min(now.time_since_epoch().count(), param_eol_time.time_since_epoch().count()) <<
                            "redis:" << redis_val << std::endl;
                    }
                }
            }
        }
        // delete values from log_table since they are not needed anymore
        query = "DELETE FROM " + log_table + " WHERE parameter_name = $1";
        result = txn.exec_params(query, payload);

        // Wait for all threads to finish
        for (std::thread &thread : threads) {
            thread.join();
        }
    };
};

int main(int argc, char* argv[]) {
    CLI::App app{"Consistant cache invalidator"};
    std::string footer = std::string("Example:\n") + argv[0] + " --postgres-host 192.168.0.1 --postgres-db-name my_db  --postgres-db-username user1 --postgres-db-password password --redis-servers username1@192.168.0.2:6379";
    app.footer(footer);
    std::string postgres_host;
    std::string postgres_db_name;
    std::string postgres_db_username;
    std::string postgres_db_password;
    std::map<std::string, std::string> redis_data;
    std::string redis_str;
    int retries = 20;
    app.add_option("--postgres-host", postgres_host, "PostgresDB host name")->required();
    app.add_option("--postgres-db-name", postgres_db_name, "PostgresDB db name")->required();
    app.add_option("--postgres-db-username", postgres_db_username, "PostgresDB username");
    app.add_option("--postgres-db-password", postgres_db_password, "PostgresDB password");
    app.add_option("--redis-servers", redis_str, "comma separated list of \"username:redis_server_ip:port\"")->required();
    app.add_option("--timeout", retries, "how many times to query for events, each time 10 seconds");
    CLI11_PARSE(app);

    std::string postgres_uri = "host=" + postgres_host + " " + "dbname=" + postgres_db_name;
    redis_data = parse_redis_data(redis_str);
    if (postgres_db_username.size()) {
        postgres_uri += (" user=" + postgres_db_username);
    }
    if (postgres_db_password.size()) {
        postgres_uri += (" password=" + postgres_db_password);
    }
    // PostgreSQL Connection
    try {
        pqxx::connection conn(postgres_uri);
        if (!conn.is_open()) {
            std::cerr << "Failed to open database" << std::endl;
            return 1;
        }
        NotificationHandler handler(conn, channel, redis_data);
        for (int i = 0; i < retries; i++) {
            conn.await_notification();
        }
        std::cout << "total queries:"<<handler.get_total_queries() << " saved queries:" << handler.get_queries_saved() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
