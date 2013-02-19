yellow-tree
===========
Yellow Tree is a Java debugger.

Status
======
As of February 2013, Yellow Tree is currently build on Windows with GNU Make and the Visual Studio Express 2008 toolchain. It has in the past ran on Linux, but is not currently run on Linux.

Installation
============
* Get Lua (Lua source can be built on Windows by writing a quick Makefile)
* Set JAVA_HOME and LUA_HOME appropriately
* Build yt.dll
* Copy yt.dll and lua52.dll somewhere on your path.
* Put debuglib.lua somewhere it can be loaded (current directly where Java app is launched?)
* Pass the -agentlib:yt argument to Java
