#
# SCCSid "$SunId$ LBL"
# Makefile for MGF parser library
#

OPT = -g
MACH = 
CFLAGS = $(MACH) $(OPT)
CC = cc

OBJS = parser.o context.o xf.o object.o lookup.o badarg.o words.o fvect.o

libmgf.a:	$(OBJS)
	ar rc libmgf.a $(OBJS)
	-ranlib libmgf.a

mgfilt:	libmgf.a mgfilt.o
	$(CC) $(CFLAGS) -o mgfilt mgfilt.o libmgf.a -lm

clean:
	rm -f *.o

parser.o context.o xf.o object.o fvect.o:	parser.h

parser.o context.o lookup.o:	lookup.h

parser.o:	messages.h

mgfilt.o:	parser.h
