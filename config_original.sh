#/bin/bash

server=$1
#sudo git pull origin master
PASSWD="heejin"
OPT_CNT=1000000
WORKLOAD="a"
for record in 4 8 10;
do
RECORD_CNT=$(($record*100000))
#zipfian/uniform
for PATTERN in zipfian uniform;
do
DSTAT_LOG="/home/heejin/original_output/dstat_log_${RECORD_CNT}_${PATTERN}_${server}.txt"
YCSB_LOG="/home/heejin/original_output/ycsb_log_${RECORD_CNT}_${PATTERN}.txt"

if [ ! -d /home/heejin/original_output ]
	mkdir /home/heejin/original_output
fi

touch ${DSTAT_LOG}
TEST="/home/heejin/mongodb_original/done.txt"
START="/home/heejin/mongodb_original/start.txt"

cd /home/heejin/mongodb_original/mongo-r3.6.18
if [ $server == "3" ] 
then
touch ${YCSB_LOG}
#mongos, mongod - apple
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongo
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongos
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
echo $PASSWD | sudo -S ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf & 
echo "apple shard on"
echo ${PASSWD} | sudo -S ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf & 
echo "config on"
#sudo ./mongo 10.20.16.165:50001 --eval "printjson(use ycsb)"
#sudo ./mongos -f /home/heejin/config/mongos.conf --bind_ip 10.20.16.165 --port 50001 &
touch ${TEST}
touch ${START}
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${START} heejin@10.20.16.110:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF

expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${START} heejin@10.20.16.111:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${START} heejin@10.20.16.112:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${START} heejin@10.20.16.115:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
rm ${TEST}
dstat -tcdm --output=${DSTAT_LOG} &
echo ${PASSWD} | sudo -S ./mongo 10.20.16.165:50001 < /home/heejin/mongodb_original/mongo_drop.js ;
cd /home/heejin/YCSB; 
./bin/ycsb load mongodb -s -P workloads/workloada -p mongodb.url="mongodb://10.20.16.165:50001" -p recordcount=${RECORD_CNT} -p operationcount=${OPT_CNT} -p requestdistribtuion=${PATTERN} >> ${YCSB_LOG};
./bin/ycsb run mongodb -s -P workloads/workload${WORKLOAD} -p mongodb.url="mongodb://10.20.16.165:50001" -p recordcount=${RECORD_CNT} -p operationcount=${OPT_CNT} -p requestdistribtuion=${PATTERN} >> ${YCSB_LOG} ;
kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`
touch ${TEST}
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${TEST} heejin@10.20.16.110:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF

expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${TEST} heejin@10.20.16.111:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${TEST} heejin@10.20.16.112:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
expect << EOF
	set timeout 1
	spawn scp -o StrictHostKeyChecking=no ${TEST} heejin@10.20.16.115:/home/heejin/mongodb_original
	expect "password:"
	send "$PASSWD\r"
	expect eof
EOF
#kill -9 `ps -ef | grep 'mongos' | awk '{print $2}'`

rm ${TEST}
echo "mongos on"
elif [ $server == "4" ]
then
#mongod - apple,banana
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongodwhile :
while :
do
	if [ -f ${START} ];
	then
		break;
	fi
done;
kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`;
rm ${START};
echo ${PASSWD} | sudo -S ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf & 
echo ${PASSWD} | sudo -S ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf &
echo ${PASSWd} | sudo -S ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf &
dstat -tcdm --output=${DSTAT_LOG} &
while :
do
	if [ -f ${TEST} ];
	then
		break;
	fi
done

if [ ${record} -eq 8 ] && [ ${PATTERN} == uniform];
then

kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`

else
continue;
fi

echo "server 4 mongod on"
rm ${TEST};
exit 1
elif [ $server == "5" ]
then
#mongod - apple,banana,mango
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
while :
do
	if [ -f ${START} ];
	then
		break;
	fi
done;
kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`;
rm ${START};
echo ${PASSWD} | sudo -S ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf &

echo ${PASSWD} | sudo -S ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf &
echo ${PASSWD} | sudo -S ./mongod --shardsvr -f /home/heejin/config/mongodb_mango.conf &
echo ${PASSWD} | sudo -S ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf &
dstat -tcdm --output=${DSTAT_LOG} &
while :
do
	if [ -f ${TEST} ];
	then
		break;
	fi
done
echo "server 5 mongod on";
rm ${TEST};
if [ ${record} -eq 8 ] && [ ${PATTERN} == uniform];
then

kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`

else
continue;
fi

exit 1
elif [ $server == "6" ]
then
#mongod - banana,mango
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
while :
do
	if [ -f ${START} ];
	then
		break;
	fi
done;
kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`;
rm ${START};
echo ${PASSWD} | sudo -S ./mongod  --shardsvr -f /home/heejin/config/mongodb_mango.conf & 
echo ${PASSWD} | sudo -S ./mongod  --shardsvr -f /home/heejin/config/mongodb_banana.conf &
dstat -tcdm --output=${DSTAT_LOG} &
while :
do
	if [ -f ${TEST} ];
	then
		break;
	fi
done
echo "server 6 mongod on";
rm ${TEST}
if [ ${record} -eq 8 ] && [ ${PATTERN} == uniform];
then

kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`

else
continue;
fi

exit 1
else
#mongod - mango
#buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
while :
do
	if [ -f ${START} ];
	then
		break;
	fi
done;
kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`;
rm ${START};
echo ${PASSWD} | sudo -S ./mongod  --shardsvr -f /home/heejin/config/mongodb_mango.conf & 
dstat -tcdm --output=${DSTAT_LOG} &
while :
do
	if [ -f ${TEST} ];
	then
		break;
	fi
done
echo "server 8 mongod on";
rm ${TEST}
if [ ${record} -eq 8 ] && [ ${PATTERN} == uniform];
then

kill -9 `ps -ef | grep 'dstat' | awk '{print $2}'`

else
continue;
fi

exit 1
fi
done
done
