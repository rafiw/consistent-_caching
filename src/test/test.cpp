#include <map>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <map>
#include <cassert>
#include <functional>

#include "CLI/CLI.hpp"
#include "BS_thread_pool.hpp"

#include "client.hpp"
#include "utils.hpp"


std::vector<std::string> tests_names = { "test_no_invalidation", "test_has_invalidations", "random_stress" };


void reset_tables(std::vector<std::unique_ptr<Client>> &clients)
{
    for (auto &client : clients) {
        client->clear();
    }
}

/**
 * test will read some data from redis and
*/
int test_no_invalidation(std::vector<std::unique_ptr<Client>> &clients)
{
    reset_tables(clients);
    int keys_per_redis = 10;
    clients[0]->populate_db(keys_per_redis * clients.size(), 5500); // put keys in postgres with TTL

    for (int i = 0; i < keys_per_redis; i++) {
        clients[0]->read_param(i);
    }
    // sleep for all the keys to be expired
    std::this_thread::sleep_for(std::chrono::seconds{10});
    for (int i = 0; i < keys_per_redis; i++) {
        clients[1]->change_param(i);
    }
    for (auto &client : clients) {
        client->start_monitor();
    }
    std::map<std::string, std::vector<std::string>> results;
    for (auto &client : clients) {
        client->stop_monitor();
        results.emplace(client->ip(), client->get_exp_deleted_keys());
    }
    assert(results[clients[0]->ip()].empty());
    return 0;
}

int test_has_invalidations(std::vector<std::unique_ptr<Client>> &clients)
{
    reset_tables(clients);
    int keys_per_redis = 10;
    clients[0]->populate_db(keys_per_redis * clients.size(), 5500);

    for (int i = 0; i < keys_per_redis; i++) {
        clients[0]->read_param(i);
    }
    for (int i = 0; i < keys_per_redis; i++) {
        clients[1]->change_param(i);
    }
    for (auto &client : clients) {
        client->start_monitor();
    }
    std::map<std::string, std::vector<std::string>> results;
    for (auto &client : clients) {
        client->stop_monitor();
        results.emplace(client->ip(), client->get_exp_deleted_keys());
    }
    assert(!results[clients[0]->ip()].empty());

    return 0;
}

void random_stress(std::vector<std::unique_ptr<Client>> &clients, int number_of_threads)
{
    reset_tables(clients);
    BS::thread_pool pool;
    int params = 1000;
    int iterations = 1000;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> op_dist(0, 5);
    std::uniform_int_distribution<> client_idx(0, clients.size() - 1);
    std::uniform_int_distribution<> line_idx(0, params - 1);
    clients[0]->populate_db(params, 6000);
    for (int i = 0; i < iterations; i++) {
        if (op_dist(gen)) {
            pool.push_task(&Client::read_param, clients[client_idx(gen)].get(), line_idx(gen));
        } else {
            pool.push_task(&Client::change_param, clients[client_idx(gen)].get(), line_idx(gen));
        }
    }
    pool.wait_for_tasks();
}

int main() {
    CLI::App app{"Consistant cache test"};
    std::string footer = "Example:\n./invalidation_test   --postgres-host 1.1.65.200 --postgres-db-name rwiener  --postgres-db-usernames-passwords username1:3tango11,username2:3tango11,username3:3tango11,username4:3tango11,username5:3tango11,username6:3tango11 --redis-servers username1@1.1.65.201:6379,username2@1.1.65.202:6379,username3@1.1.65.203:6379,username4@1.1.65.204:6379,username5@1.1.65.205:6379,username6@1.1.65.206:6379";
    app.footer(footer);
    std::string postgres_host;
    std::string postgres_db_name;
    std::vector<std::string> post_db_usernames_passwords;
    std::string queue_table_name = "queue";
    std::map<std::string, std::string> redis_data;
    std::string redis_str;
    std::string test_name;
    int threads_number = 0;
    app.add_option("--postgres-host", postgres_host, "PostgresDB host name")->required();
    app.add_option("--postgres-db-name", postgres_db_name, "PostgresDB database name")->required();
    app.add_option("--postgres-db-usernames-passwords", post_db_usernames_passwords, "comma separeted list of PostgresDB username:password")->required()->delimiter(',');
    app.add_option("--redis-servers", redis_str, "comma separated list of \"username:redis;servers ip:port\"")->required();
    app.add_option("--test", test_name, "test to run")->required()->check(CLI::IsMember(tests_names));
    app.add_option("-t, --threads", threads_number, "number of threads to use in stress test");
    CLI11_PARSE(app);
    if (threads_number == 0) {
        threads_number = std::thread::hardware_concurrency();
    }
    redis_data = parse_redis_data(redis_str);
    std::map<std::string, std::string> postgres_uris;
    for (const auto& post_db_usernames_password: post_db_usernames_passwords) {
        std::string postgres_uri = "host=" + postgres_host + " " + "dbname=" + postgres_db_name;
        size_t pos = post_db_usernames_password.find(":");
        if (pos == std::string::npos) {
            std::cerr << "Colon not found! in post-db-usernames-passwords" << std::endl;
            return -1;
        }
        auto user = post_db_usernames_password.substr(0, pos);
        auto pass = post_db_usernames_password.substr(pos + 1);
        postgres_uri += (" user=" + user);
        postgres_uri += (" password=" + pass);
        postgres_uris.emplace(user, postgres_uri);
    }
    std::vector<std::unique_ptr<Client>> clients;
    for (const auto& [username, conn] : redis_data) {
        clients.emplace_back(std::make_unique<Client>(postgres_uris[username], conn));
    }
    if (test_name == "test_no_invalidation")
        test_no_invalidation(clients);
    else if (test_name == "test_has_invalidations")
        test_has_invalidations(clients);
    else if (test_name == "random_stress") {
        random_stress(clients, threads_number);
    }
    return 0;}
