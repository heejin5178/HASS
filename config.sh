#!/bin/bash

server=$1
cd mongodbShard
git pull origin master
cd mongo-r3.6.18
if [ $server == 3 ] 
then
#mongos, mongod - apple
buildscripts/scons.py MONGO_VERSION=3.6.18 mongos
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf -fork
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf -fork
sudo ./mongos -f /home/heejin/config/mongos.conf --fork

elif [ $sever == 4 ]
then
#mongod - apple,banana
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf -fork
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf -fork
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf -fork
elif [ $server == 5 ]
then
#mongod - apple,banana,mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_apple.conf -fork
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf -fork
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_mango.conf -fork
sudo ./mongod --configsvr -f /home/heejin/config/mongodb_config.conf -fork
elif [ $server == 6 ]
then
#mongod - banana,mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_mango.conf -fork
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_banana.conf -fork
else
#mongod - mango
buildscripts/scons.py MONGO_VERSION=3.6.18 mongod
sudo ./mongod --shardsvr -f /home/heejin/config/mongodb_mango.conf -fork
fi

