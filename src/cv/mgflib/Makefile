#
# SCCSid "$SunId$ LBL"
# Makefile for MGF parser library
#
# Use -DNOPROTO flag to get back K&R compatibility.
# Use -DBSD flag if your UNIX has a strong BSD flavor.
# Use '-DMEM_PTR=char *' if your malloc return type is not 'void *'.
MACH = 
OPT = -O
CFLAGS = $(MACH) $(OPT)
CC = cc

OBJS = parser.o context.o xf.o object.o lookup.o badarg.o \
words.o fvect.o cvrgb.o

all:	libmgf.a mgfilt mgf2inv

mgfilt:	libmgf.a mgfilt.o
	$(CC) $(CFLAGS) -o mgfilt mgfilt.o libmgf.a -lm

libmgf.a:	$(OBJS)
	ar rc libmgf.a $(OBJS)
	-ranlib libmgf.a

mgf2inv:	libmgf.a mgf2inv.o
	$(CC) $(CFLAGS) -o mgf2inv mgf2inv.o libmgf.a -lm

clean:
	set nonomatch ; rm -f *.o libmgf.a mgfilt

parser.o context.o xf.o object.o fvect.o mgf2inv.o cvrgb.o:	parser.h

parser.o context.o lookup.o:	lookup.h

parser.o:	messages.h

mgfilt.o:	parser.h
