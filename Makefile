CC = gcc

# LIBS = -lresolv -lsocket -lnsl -lpthread
LIBS = -lresolv -lnsl -lpthread\
	   /home/courses/cse533/Stevens/unpv13e/libunp.a

FLAGS = -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e/lib

SOURCES = api.c get_hw_addrs.c
OBJS = $(SOURCES:.c=.o)

all: ODR_yfei client_yfei server_yfei

print: $(OBJS) prhwaddrs.o
	${CC} -o $@ $^ ${LIBS}

ODR_yfei: $(OBJS) odr.o
	${CC} -o $@ $^ ${LIBS}

server_yfei: $(OBJS) server.o
	${CC} -o $@ $^ ${LIBS}

client_yfei: $(OBJS) client.o
	${CC} -o $@ $^ ${LIBS}

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm *.o
	rm client_yfei
	rm server_yfei
	rm ODR_yfei
