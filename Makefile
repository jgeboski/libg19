LIBNAM=libg19.so
LIBDIR=/usr/lib

LIBUSB_CFLAGS=$(shell pkg-config --cflags libusb-1.0)
LIBUSB_LDFLAGS=$(shell pkg-config --libs libusb-1.0)

CFLAGS=-Wall -O2 -fPIC $(LIBUSB_CFLAGS)
LDFLAGS=$(LIBUSB_LDFLAGS) -lpthread

LIBSRC=libg19.c
LIBOBJ=$(LIBSRC:.c=.o)

all: $(LIBNAM)

$(LIBNAM): $(LIBOBJ)
	$(CC) -shared $(CFLAGS) $(LDFLAGS) $(LIBOBJ) -o $(LIBNAM)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	install -m 755 $(LIBNAM) $(LIBDIR)

clean:
	$(RM) $(LIBOBJ) $(LIBNAM)
