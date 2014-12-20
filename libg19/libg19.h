/*
 * Copyright 2010-2014 James Geboski <jgeboski@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LIBG19_H
#define _LIBG19_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define LIBG19_VERCODE(m1, m2, m3) (((m1) << 16) | ((m2) << 8) | (m3))
#define LIBG19_VERSION LIBG19_VERCODE(1, 1, 1)

#define G19_BMP_SIZE      154112
#define G19_BMP_DSIZE     153600
#define G19_VENDOR_ID     0x046d
#define G19_PRODUCT_ID    0xc229
#define G19_DATA_HDR_SIZE 512

#ifdef __cplusplus
extern "C"
{
#endif

/** The special keys of the keyboard. **/
typedef enum _G19Keys  G19Keys;

/** The formatting of the LCD data. **/
typedef enum _G19UpdateType G19UpdateType;

/** The structure for a G19 device. **/
typedef struct _G19Device G19Device;

/** The structure for a file descriptor to poll. **/
typedef struct _G19PollFD G19PollFD;


/**
 * The special keys of the keyboard.
 **/
enum _G19Keys
{
    G19_KEY_LHOME   = 1 << 0,
    G19_KEY_LCANCEL = 1 << 1,
    G19_KEY_LMENU   = 1 << 2,
    G19_KEY_LOK     = 1 << 3,
    G19_KEY_LRIGHT  = 1 << 4,
    G19_KEY_LLEFT   = 1 << 5,
    G19_KEY_LDOWN   = 1 << 6,
    G19_KEY_LUP     = 1 << 7,

    G19_KEY_G1      = 1 << 8,
    G19_KEY_G2      = 1 << 9,
    G19_KEY_G3      = 1 << 10,
    G19_KEY_G4      = 1 << 11,
    G19_KEY_G5      = 1 << 12,
    G19_KEY_G6      = 1 << 13,
    G19_KEY_G7      = 1 << 14,
    G19_KEY_G8      = 1 << 15,
    G19_KEY_G9      = 1 << 16,
    G19_KEY_G10     = 1 << 17,
    G19_KEY_G11     = 1 << 18,
    G19_KEY_G12     = 1 << 19,

    G19_KEY_M1      = 1 << 20,
    G19_KEY_M2      = 1 << 21,
    G19_KEY_M3      = 1 << 22,
    G19_KEY_MR      = 1 << 23
};


/**
 * The formatting of the LCD data.
 **/
enum _G19UpdateType
{
    G19_UPDATE_TYPE_BMP  = 1 << 0,
    G19_UPDATE_TYPE_RAW  = 1 << 1
};

/**
 * The structure for a G19 device.
 **/
struct _G19Device
{
    void *ctx;    /** The #libusb_context. **/
    void *hndl;   /** The #libusb_device_handle. **/

    void *lktrn;  /** The L-key #libusb_transfer. **/
    void *gktrn;  /** The G-key #libusb_transfer. **/
    void *gkctrn; /** The G-key control #libusb_transfer. **/

    void *data;   /** The user-defined callback data. **/

    /** The callback for G and M keys. **/
    void (*keys) (G19Device *dev, uint32_t keys, void *data);
};

/**
 * The structure for a file descriptor to poll.
 **/
struct _G19PollFD
{
    int   fd;     /* The file descriptor. */
    short events; /* The event flags. */
};


extern const uint8_t g19_data_hdr[G19_DATA_HDR_SIZE];

ssize_t g19_device_count(void);

G19Device *g19_device_open(size_t index, int *error);

void g19_device_close(G19Device *dev);

G19PollFD *g19_device_pollfds(G19Device *dev, size_t *size);

int g19_device_pollto(G19Device *dev, struct timeval *tv);

int g19_device_pollev(G19Device *dev);

int g19_device_lcd(G19Device *dev, uint8_t *data, size_t size,
                    G19UpdateType type);

int g19_device_brightness(G19Device *dev, uint8_t brightness);

int g19_device_backlight(G19Device *dev, uint8_t r, uint8_t g, uint8_t b);

int g19_device_mkeys(G19Device *dev, uint32_t keys);

#ifdef __cplusplus
}
#endif

#endif /* _LIBG19_H */
