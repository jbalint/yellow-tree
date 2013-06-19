CDIR = ../c
CFLAGS = -ggdb3 -O0 -Wall -fPIC
CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CFLAGS += -I$(LUA_HOME)/include
LDFLAGS += -L$(LUA_HOME)/lib

libyt.so: yt.o lua_interface.o lua_java.o jni_util.o lua_jvmti_event.o
	gcc -shared -Wl,-soname,libyt.so.1 -o libyt.so.1.0.0 $(LDFLAGS) $^ -llua
