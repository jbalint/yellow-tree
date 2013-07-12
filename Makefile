CDIR = ../c
CFLAGS = -ggdb3 -O0 -Wall -fPIC
CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CFLAGS += -I$(LUA_HOME)/include
CFLAGS += -I`pwd` -I`pwd`/lua_java
LDFLAGS += -L$(LUA_HOME)/lib

LJ_OBJS = lua_java.o lua_jvmti_event.o \
	lua_java/lj_class.o \
	lua_java/lj_field.o \
	lua_java/lj_force_early_return.o \
	lua_java/lj_method.o \
	lua_java/lj_raw_monitor.o \
	lua_java/lj_stack_frame.o

libyt.so: yt.o lua_interface.o jni_util.o $(LJ_OBJS)
	gcc -shared -Wl,-soname,libyt.so.1 -o libyt.so.1.0.0 $(LDFLAGS) $^ -llua
	ln -sf libyt.so.1.0.0 libyt.so

clean:
	@rm -f *.o lua_java/*.o libyt.so.1.0.0 libyt.so
