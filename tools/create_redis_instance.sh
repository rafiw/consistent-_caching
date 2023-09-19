#!/usr/bin/env bash

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 number of redis instances"
    exit 1
fi

CREATE_AMOUNT=$1
LATEST_IDX=$(ls /var/lib/ | grep -oP 'redis\K\d+' | sort -n | tail -n 1)
if [[ -z "$LATEST_IDX" ]]; then
    LATEST_IDX=2
fi
for i in $(seq $LATEST_IDX $(($LATEST_IDX+$CREATE_AMOUNT))); do
    echo "creating instance ($i - $LATEST_IDX)"
    CONF_FILE=/etc/redis/redis$i.conf
    sudo install -o redis -g redis -d /var/lib/redis$i

    # redis configuration file
    sudo cp -p /etc/redis/redis.conf ${CONF_FILE}
    sed -i "s|/var/run/redis/redis-server.pid|/var/run/redis$i/redis-server.pid|" ${CONF_FILE}
    sed -i "s|logfile /var/log/redis/redis-server.log|logfile /var/log/redis$i/redis-server.log|" ${CONF_FILE}
    sed -i "s|dir /var/lib/redis|dir /var/lib/redis$i|" ${CONF_FILE}
    # service file
    sudo mkdir /var/log/redis$i
    SERVICE_FILE=/lib/systemd/system/redis-server$i.service
    sudo cp /lib/systemd/system/redis-server.service /lib/systemd/system/redis-server$i.service
    sed -i "s|ExecStart=/usr/bin/redis-server /etc/redis/redis.conf|ExecStart=/usr/bin/redis-server /etc/redis/redis$i.conf|" ${SERVICE_FILE}
    sed -i "s|PIDFile=/run/redis/redis-server.pid|PIDFile=/run/redis/redis-server$i.pid|" ${SERVICE_FILE}
    sed -i "s|RuntimeDirectory=redis|RuntimeDirectory=redis$i|" ${SERVICE_FILE}
    sed -i "s|ReadWritePaths=-/var/lib/redis|ReadWritePaths=-/var/lib/redis$i|" ${SERVICE_FILE}
    sed -i "s|ReadWritePaths=-/var/log/redis|ReadWritePaths=-/var/log/redis$i|" ${SERVICE_FILE}
    sed -i "s|ReadWritePaths=-/var/run/redis|ReadWritePaths=-/var/run/redis$i|" ${SERVICE_FILE}
    sed -i "s|Alias=redis.service|Alias=redis$i.service|" ${SERVICE_FILE}
    sudo systemctl enable redis-server$i.service
    echo "run sudo systemctl start redis-server$i.service to start"
done