#!/bin/sh

javac -g TelescopeTest.java EarlyReturnTest.java

export LD_LIBRARY_PATH=/home/jbalint/sw/yellow-tree

java -Xcheck:jni -agentlib:yt=runfile=tsc_runner.lua -cp . TelescopeTest
