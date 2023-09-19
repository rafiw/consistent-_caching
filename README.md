# Consistent caching
This Repository contains a database technique used to increase the cache consistence and reduce the number of invalidations sent in the system.
The idea behind this is to save TTL (time to live) for values in the SQL table and settings values in the Redis cache based on these TTL. When a value changed in the table, the DB knows what caches contains the data and need to be invalidated. Using a time synchronized algorithms like PTP reduces even more the number of invalidations needed since we know the exact time and we don't need to add coefficient for the time inaccuracy.
## Usage
This section will contain instruction for how to setup and run a basic test.
### Preparation
You should have a setup with PostgreSQL(tm) server and few Redis(tm) servers. You can run a few Redis servers from one physical server, you can use a script inside the tools folder for that. The SQL should have different users created as the number of Redis server you have, you can create a user with this query:
    `CREATE USER new_username WITH PASSWORD 'your_password';`
 1. Run the file `tables.sql` in the root folder to create the needed tables.
 2. Give permission on that tables to the users you created
 `GRANT USAGE ON SEQUENCE parameter_data_id_seq,read_log_id_seq TO [my_username];`
 `GRANT INSERT, UPDATE, DELETE, SELECT ON TABLE parameter_data,read_log TO [my_username];`

### Build
1. `git clone...`
2. `git submodule init`
3. `git submodule update`
4. `cd consistant_caching && mkdir build`
5. `cd build`
6. `cmake .. && make -j`

### Run
Under `build/bin` you will find two binaries
1. redis_invalidator - This binary listens on events arriving from the Postgres and sends invalidation to the relevant Redis servers.
```
Usage: ./redis_invalidator [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --postgres-host TEXT REQUIRED
                              PostgresDB host name
  --postgres-db-name TEXT REQUIRED
                              PostgresDB db name
  --postgres-db-username TEXT PostgresDB username
  --postgres-db-password TEXT PostgresDB password
  --redis-servers TEXT        comma separated list of "username:redis_server_ip:port"
  --timeout INT               how many times to query for events, each time 10 seconds
  ```
For example:
`./redis_invalidator --postgres-host 192.168.0.1 --postgres-db-name my_db  --postgres-db-username user1 --postgres-db-password password --redis-servers username1@192.168.0.4:6379,username2@192.168.0.3:6379`

2. invalidation_test - This binary contains three basic tests showing the value of this approach.

```
Usage: ./invalidation_test [OPTIONS]
Options:
  -h,--help                   Print this help message and exit
  --postgres-host TEXT REQUIRED
                              PostgresDB host name
  --postgres-db-name TEXT REQUIRED
                              PostgresDB database name
  --postgres-db-usernames-passwords TEXT ... REQUIRED
                              comma separeted list of PostgresDB username:password
  --redis-servers TEXT REQUIRED
                              comma separated list of "username:redis;servers ip:port"
  --test TEXT:{test_no_invalidation,test_has_invalidations,random_stress} REQUIRED
                              test to run
  -t,--threads INT            number of threads to use in stress test
```
For example:
`./invalidation_test   --postgres-host 192.168.0.1 --postgres-db-name db_name  --postgres-db-usernames-passwords username1:password1,username2:password2 --redis-servers username1@192.168.0.2:6379,username2@192.168.0.2:6379`
