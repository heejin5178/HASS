#!/bin/bash

server=$1
sudo git pull origin master
PASSWD="heejin"
cd mongo-r3.6.18
RECORD_CNT="100"
OPT_CNT="100"
#a/b/c/d/e
WORKLOAD="a"
#zipfian/uniform
PATTERN="zipfian"
DSTAT_LOG="/home/heejin/dstat_log.txt_${RECORD_CNT}_${PATTERN}"
YCSB_LOG="/home/heejin/ycsb_log.txt_${RECORD_CNT}_${PATTERN}"
if [ $server == "3" ] 
then
#mongos, mongod - apple
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongo
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongos
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf & 
echo "apple shard on"
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf & 
echo "config on"
dstat -tcdm --output=${DSTAT_LOG}_3 &
sudo ./mongos -f /home/heejin/config/mongos.conf --bind_ip 10.20.16.165 --port 50001 &
cd ../YCSB
./bin/ycsb load mongodb -s -P workloads/workload${WORKLOAD} -p mongodb.url="mongodb://10.20.16.165:50001" -p recordcount=${RECORD_CNT} -p operationcount=${OPT_CNT} -p requestdistribtuion=${PATTERN} >> ${YCSB_LOG} &&
./bin/ycsb run mongodb -s -P workloads/workload${WORKLOAD} -p mongodb.url="mongodb://10.20.16.165:50001" -p recordcount=${RECORD_CNT} -p operationcount=${OPT_CNT} -p requestdistribtuion=${PATTERN} >> ${YCSB_LOG} &&
touch done.txt
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no done.txt heejin@10.20.16.110:/home/heejin/mongodbShard
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF

expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no done.txt heejin@10.20.16.111:/home/heejin/mongodbShard
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no done.txt heejin@10.20.16.112:/home/heejin/mongodbShard
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no done.txt heejin@10.20.16.115:/home/heejin/mongodbShard
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`
#kill -9 `ps -ef | grep 'mongos' | awk '{print $2}'`

rm done.txt
echo "mongos on"
elif [ $server == "4" ]
then
#mongod - apple,banana
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
dstat -tcdm --output=${DSTAT_LOG}_4 &
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf & 
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf &
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf &
while :
do
	if [ -f /home/heejin/mongodbShard/done.txt ];
	then
		kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`
		break;
	fi
done
echo "server 4 mongod on"
rm done.txt
exit 1
elif [ $server == "5" ]
then
#mongod - apple,banana,mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
dstat -tcdm --output=${DSTAT_LOG}_5 &
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf &
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf &
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_mango.conf &
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf &
while :
do
	if [ -f /home/heejin/mongodbShard/done.txt ];
	then
		kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`
		break;
	fi
done
echo "server 5 mongod on";
rm done.txt
exit 1
elif [ $server == "6" ]
then
#mongod - banana,mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
dstat -tcdm --output=${DSTAT_LOG}_6 &
sudo ./mongod  --shardsvr -f /home/heejin/config/mongodb_mango.conf & 
sudo ./mongod  --shardsvr -f /home/heejin/config/mongodb_banana.conf &
while :
do
	if [ -f /home/heejin/mongodbShard/done.txt ];
	then
		kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`
		break;
	fi
done
echo "server 6 mongod on";
rm done.txt
exit 1
else
#mongod - mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
dstat -tcdm --output=${DSTAT_LOG}_8 &
sudo ./mongod  --shardsvr -f /home/heejin/config/mongodb_mango.conf & 
while :
do
	if [ -f /home/heejin/mongodbShard/done.txt ];
	then
		kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`
		break;
	fi
done
echo "server 8 mongod on";
rm done.txt
exit 1
fi

