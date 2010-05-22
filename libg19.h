/*
 * Copyright 2010 James Geboski <jgeboski@users.sourceforge.net>
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

#ifndef G19_H
#define G19_H

#include <stdint.h>
#include <libusb.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LIBG19_VERSION "1.0.0"

typedef enum LibG19LogType
{
	G19_LOG_INFO = 0,
	G19_LOG_WARN,
	G19_LOG_ERROR,
	G19_LOG_DEBUG
}
G19LogType;

typedef struct
{
	char * name;
	uint16_t vendor_id;
	uint16_t product_id;
	libusb_device_handle * handle;
}
G19Device;

enum
{
	G19_KEY_G1			= 1 << 0,
	G19_KEY_G2			= 1 << 1,
	G19_KEY_G3			= 1 << 2,
	G19_KEY_G4			= 1 << 3,
	G19_KEY_G5			= 1 << 4,
	G19_KEY_G6			= 1 << 5,
	G19_KEY_G7			= 1 << 6,
	G19_KEY_G8			= 1 << 7,
	G19_KEY_G9			= 1 << 8,
	G19_KEY_G10			= 1 << 9,
	G19_KEY_G11			= 1 << 10,
	G19_KEY_G12			= 1 << 11,
	
	G19_KEY_M1			= 1 << 12,
	G19_KEY_M2			= 1 << 13,
	G19_KEY_M3			= 1 << 14,
	G19_KEY_MR			= 1 << 15,
	
	G19_KEY_LHOME		= 1 << 16,
	G19_KEY_LCANCEL		= 1 << 17,
	G19_KEY_LMENU		= 1 << 18,
	G19_KEY_LOK			= 1 << 19,
	G19_KEY_LRIGHT		= 1 << 20,
	G19_KEY_LLEFT		= 1 << 21,
	G19_KEY_LDOWN		= 1 << 22,
	G19_KEY_LUP			= 1 << 23
};

//Callback for the gkeys or lkeys and passed to g19_set_gkeys_cb() or g19_set_lkeys_cb()
typedef void(* g19_keys_cb)(unsigned int keys);

//Initializes the g19 library; level is to be 0 to 3 which is the debug level to be passed to libusb
int g19_init(int level);

//Clean up and close the g19 library
void g19_deinit(void);

//Initializes key capture for the g-keys and m-keys and also sets a callback for key events
void g19_set_gkeys_cb(g19_keys_cb cb);

//Initializes key capture for the l-keys and also sets a callback for key events
void g19_set_lkeys_cb(g19_keys_cb cb);

//data has a max size of 153,600.  len is the amount to be written to the screen.
//Keep in mind each pixel is 2 bytes. If you divide the max size 153,600 in half
//you 76,800 which is the same as 320x240 which is the screens size.
void g19_update_lcd(unsigned char * data, int len);

//Sets the backlight color; r is the red, g is the green, and b is the blue if you didn't guess
int g19_set_backlight(int r, int g, int b);

//This sets the m-key lights such as turning the m1 light on and off; You just use the m-key values set in the enum above. G19_KEY_M1 to G19_KEY_MR; If you want more than one key just use a bitwise or
int g19_set_mkey_led(unsigned int keys);

#ifdef __cplusplus
}
#endif

#endif
