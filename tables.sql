-- tables
-- simple table that contains data
CREATE TABLE parameter_data (
    id serial PRIMARY KEY,
    parameter_name text UNIQUE NOT NULL,
    parameter_value text,
    ttl double precision,
    timestamp timestamp with time zone DEFAULT CURRENT_TIMESTAMP
);

--table that logs which client read data and when
CREATE TABLE read_log (
    id SERIAL PRIMARY KEY,
    username TEXT,
    read_timestamp TIMESTAMP,
    parameter TEXT
);

-- function to read data from parameter_data table
-- since there is no trigger for select we use a function
CREATE OR REPLACE FUNCTION get_parameter(param_name text) RETURNS parameter_data AS $$
DECLARE
    result parameter_data;
BEGIN
    SELECT * INTO result FROM parameter_data WHERE parameter_name = param_name;

    IF FOUND THEN
        UPDATE read_log
        SET read_timestamp = NOW()
        WHERE username = session_user AND parameter_name = param_name;
        IF NOT FOUND THEN
            INSERT INTO read_log (username, read_timestamp, parameter_name)
            VALUES (session_user, NOW(), param_name);
        END IF;
    END IF;

    RETURN result;
END;
$$ LANGUAGE plpgsql;


--Trigger function to send notification when a value changes
CREATE OR REPLACE FUNCTION set_parameter()
RETURNS TRIGGER AS $$
BEGIN
  PERFORM pg_notify('data_update', NEW.parameter_name);
  RETURN NEW;
END
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_queue_with_task
AFTER UPDATE ON parameter_data
FOR EACH ROW
EXECUTE FUNCTION set_parameter();

-- sql permission
-- GRANT USAGE ON SEQUENCE parameter_data_id_seq,read_log_id_seq TO [my_username];
-- GRANT INSERT, UPDATE, DELETE, SELECT ON TABLE parameter_data,read_log TO [my_username];

