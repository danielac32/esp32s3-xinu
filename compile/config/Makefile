#
# Make the Xinu configuration program
#

CC      = gcc
LEX     = flex
YACC    = bison -y    # Flag enables yacc-compatible filenames
YFLAGS  = -d
CFLAGS  = -std=gnu89 -Wno-error=implicit-function-declaration
LDFLAGS =

CONFIG  = config
OBJECTS = scan.o parse.o config.o

$(CONFIG): $(OBJECTS)

scan.o: scan.l parse.c

parse.o: parse.y

y.tab.h: parse.o

config.o: config.c y.tab.h

clean:
	rm -f $(CONFIG) *.o y.tab.h parse.c
