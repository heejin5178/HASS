#!/bin/bash

kill -9 `ps -ef | grep 'mongo' | awk '{print $2}'`
