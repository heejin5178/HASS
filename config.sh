#!/bin/bash

server=$1
sudo git pull origin master
cd mongo-r3.6.18
if [ $server == "3" ] 
then
#mongos, mongod - apple
buildscripts/scons.py MONGO_VERSION=3.6.18 mongo
buildscripts/scons.py MONGO_VERSION=3.6.18 mongos
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf & 
echo "apple shard on"
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf & 
echo "config on"
#sudo ./mongos -f /home/heejin/config/mongos.conf --bind_ip 10.20.16.165 --port 50001;
echo "mongos on"
elif [ $server == "4" ]
then
#mongod - apple,banana
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf & 
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf &
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf &
echo "server 4 mongod on"
exit 1
elif [ $server == "5" ]
then
#mongod - apple,banana,mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf &
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf &
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_mango.conf &
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf &
echo "server 4 mongod on";
exit 1
elif [ $server == "6" ]
then
#mongod - banana,mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod  --shardsvr -f /home/heejin/config/mongodb_mango.conf & 
sudo ./mongod  --shardsvr -f /home/heejin/config/mongodb_banana.conf &
echo "server 6 mongod on";
exit 1
else
#mongod - mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod  --shardsvr -f /home/heejin/config/mongodb_mango.conf & 
echo "server 8 mongod on";
exit 1
fi

