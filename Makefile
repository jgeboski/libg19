LIBNAM=libg19.so

LIBDIR:=/usr/lib

CFLAGS=-Wall -O2 -fPIC $(shell pkg-config --cflags libusb-1.0)
LDFLAGS=$(shell pkg-config --libs libusb-1.0)

SRC=libg19.c
OBJ=$(SRC:.c=.o)

all: $(LIBNAM)

$(LIBNAM): $(OBJ)
	$(CC) -shared $(CFLAGS) $(LDFLAGS) $(OBJ) -o $(LIBNAM)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	install -m 755 $(LIBNAM) $(LIBDIR)

clean:
	$(RM) $(OBJ) $(LIBNAM)
