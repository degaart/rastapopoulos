.DEFAULT_GOAL := all
PROGRAM := rpctest
CFLAGS := -g
LDFLAGS := -g

ADD_SRCS := obj/tst_client.c obj/tst_common.c obj/tst_server.c
ADD_OBJS := obj/tst_client.c.o obj/tst_common.c.o obj/tst_server.c.o

obj/tst_client.h obj/tst_client.c obj/tst_common.h obj/tst_common.c obj/tst_server.h obj/tst_server.c: rpctest.rpc
	@../rpcgen/obj/rpcgen \
		rpctest.rpc \
		obj/tst_common.h \
		obj/tst_common.c \
		obj/tst_server.h \
		obj/tst_server.c \
		obj/tst_client.h \
		obj/tst_client.c

include ../tools.mk


