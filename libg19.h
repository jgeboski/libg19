/*
 * Copyright 2010-2011 James Geboski <jgeboski@gmail.com>
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
 *
 */

#ifndef _G19_H_
#define _G19_H_

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LIBG19_VERSION_MAJOR  1
#define LIBG19_VERSION_MINOR  2
#define LIBG19_VERSION_MICRO  0

#define LIBG19_VERSION        "1.2.0"

#define G19_BMP_SIZE    154112
#define G19_BMP_DSIZE   153600

#define G19_VENDOR_ID   0x046d
#define G19_PRODUCT_ID  0xc229

typedef enum _G19UpdateType G19UpdateType;
typedef enum _G19GKeys G19GKeys;

enum _G19UpdateType
{
    G19_UPDATE_TYPE_BMP  = 1 << 0,
    G19_UPDATE_TYPE_RAW  = 1 << 1
};

enum _G19GKeys
{
    G19_KEY_LHOME   = 1 << 0,
    G19_KEY_LCANCEL = 1 << 1,
    G19_KEY_LMENU   = 1 << 2,
    G19_KEY_LOK     = 1 << 3,
    G19_KEY_LRIGHT  = 1 << 4,
    G19_KEY_LLEFT   = 1 << 5,
    G19_KEY_LDOWN   = 1 << 6,
    G19_KEY_LUP     = 1 << 7,
    
    G19_KEY_G1      = 256 << 0,
    G19_KEY_G2      = 256 << 1,
    G19_KEY_G3      = 256 << 2,
    G19_KEY_G4      = 256 << 3,
    G19_KEY_G5      = 256 << 4,
    G19_KEY_G6      = 256 << 5,
    G19_KEY_G7      = 256 << 6,
    G19_KEY_G8      = 256 << 7,
    G19_KEY_G9      = 256 << 8,
    G19_KEY_G10     = 256 << 9,
    G19_KEY_G11     = 256 << 10,
    G19_KEY_G12     = 256 << 11,
    
    G19_KEY_M1      = 256 << 12,
    G19_KEY_M2      = 256 << 13,
    G19_KEY_M3      = 256 << 14,
    G19_KEY_MR      = 256 << 15
};

typedef void(* G19GKeysFunc)(unsigned int keys);
typedef void(* G19LKeysFunc)(unsigned short keys);

int g19_init(int level);

void g19_deinit(void);


void g19_set_gkeys_cb(G19GKeysFunc cb);

void g19_set_lkeys_cb(G19LKeysFunc cb);


void g19_update_lcd(unsigned char * data, size_t size, G19UpdateType type);

int g19_set_backlight(unsigned char r, unsigned char g, unsigned char b);

int g19_set_brightness(unsigned char level);

int g19_set_mkey_led(unsigned int keys);

#ifdef __cplusplus
}
#endif

#endif
