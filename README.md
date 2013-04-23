yellow-tree
===========
Yellow Tree is a Java debugger.

See documentation:
* [Java Bridge](java_bridge.md)

Status
======
As of April 2013, Yellow Tree is used on:
* Windows with GNU Make and the Visual Studio Express 2008 toolchain
* Linux

Limitations
===========
* Array types are universally unsupported in the JNI code

Installation
============
* Get Lua (Lua source can be built on Windows by writing a quick Makefile, see "misc" dir)
* Set paths JAVA_HOME and LUA_HOME appropriately
* Set PATH and LUA_PATH to Yellow Tree directory
* Set LUA_PATH and LUA_CPATH for LuaSocket (if using network io) e.g. LUA_PATH='/path/to/yellow-tree/?.lua'
* Build yt.dll
* Pass the -agentlib:yt argument to Java
