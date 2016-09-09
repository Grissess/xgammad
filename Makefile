CFLAGS=-g
LDFLAGS=-lX11 -lXxf86vm -lm
OBJS=main.o icct.o coltbl.c

all: xgammad

xgammad: $(OBJS)
	gcc $(CFLAGS) -o $@ $? $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -o $@ -c $?
