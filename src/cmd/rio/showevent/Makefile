CFLAGS = -g 
INCLUDE = -I/global/include
LFLAGS = -L/global/lib
OBJS =	ShowEvent.o sample.o
LIBS = -lX11

all: sample

.c.o:
	$(CC) $(INCLUDE) $(CFLAGS) -c $<

sample: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) $(LIBS) -o sample
