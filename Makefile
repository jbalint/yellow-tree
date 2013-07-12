CDIR = ../c
CFLAGS = -ggdb3 -O0 -Wall -fPIC
CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CFLAGS += -I$(LUA_HOME)/include
CFLAGS += -I`pwd` -I`pwd`/lua_java
LDFLAGS += -L$(LUA_HOME)/lib

libyt.so: yt.o lua_interface.o lua_java.o jni_util.o lua_jvmti_event.o lua_java/lj_raw_monitor.o
	gcc -shared -Wl,-soname,libyt.so.1 -o libyt.so.1.0.0 $(LDFLAGS) $^ -llua
