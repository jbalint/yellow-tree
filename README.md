yellow-tree
===========
Yellow Tree is a Java debugger.

Status
======
As of February 2013, Yellow Tree is currently built on Windows with GNU Make and the Visual Studio Express 2008 toolchain. It has in the past ran on Linux, but is not currently run on Linux.

Limitations
===========
* Array types are universally unsupported in the JNI code

Installation
============
* Get Lua (Lua source can be built on Windows by writing a quick Makefile, see "misc" dir)
* Set paths JAVA_HOME and LUA_HOME appropriately
* Set PATH and LUA_PATH to Yellow Tree directory
* Set LUA_PATH and LUA_CPATH for LuaSocket (if using network io)
* Build yt.dll
* Pass the -agentlib:yt argument to Java
