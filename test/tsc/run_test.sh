#!/bin/sh

export LD_LIBRARY_PATH=/home/jbalint/sw/yellow-tree

javac -g TelescopeTest.java EarlyReturnTest.java ArrayTest.java && \
	java -Xcheck:jni -agentlib:yt=runfile=tsc_runner.lua -cp . TelescopeTest
