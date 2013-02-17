CDIR = ../c
CFLAGS = -ggdb3 -O0 -Wall -fPIC
CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CFLAGS += -I$(CDIR)
CFLAGS += -DLIST_HAS_ID

libyt.so: yt.o list.o
	ld -shared -o libyt.so yt.o list.o

list.c: $(CDIR)/list.c
	cp $< $@
