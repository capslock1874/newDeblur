CC = gcc
CFLAGS = -pg -c -std=c99 -Wall 
LINK = gcc
LINKFLAGS = -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video -lopencv_calib3d -lpthread

OBJS = main.o homo.o blurfunc.o deblur.o luckiness.o
EXES = deblur.exe

everything : $(EXES)

all : clean everything

clean :
	rm -f $(EXES) $(OBJS)

deblur.exe : $(OBJS)
	$(LINK) $^ $(LINKFLAGS) -pg -o $@

main.o : main.c deblur.h
	$(CC) $(CFLAGS) -o $@ $<

homo.o : homo.c deblur.h
	$(CC) $(CFLAGS) -o $@ $<

blurfunc.o : blurfunc.c deblur.h
	$(CC) $(CFLAGS) -o $@ $<

deblur.o : deblur.c deblur.h
	$(CC) $(CFLAGS) -o $@ $<

luckiness.o : luckiness.c deblur.h
	$(CC) $(CFLAGS) -o $@ $<
