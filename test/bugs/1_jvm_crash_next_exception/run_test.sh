#!/bin/bash
set -e
set -x
javac -g TestBug1.java
# run the test
export LD_LIBRARY_PATH=`pwd`/../../..:$LD_LIBRARY_PATH
java -agentlib:yt=runfile=testbug1.lua -cp . TestBug1
