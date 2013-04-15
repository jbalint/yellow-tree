#!/bin/bash
set -e
set -x
javac -g TestBug1.java
# run the test
export LD_LIBRARY_PATH=`pwd`/../../..:$LD_LIBRARY_PATH
export LD_PRELOAD=$JAVA_HOME/jre/lib/amd64/libjsig.so
java -Xcheck:jni -agentlib:yt=runfile=testbug1.lua -cp . TestBug1
