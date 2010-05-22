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
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LIBG19_VERSION_MAJOR	1
#define LIBG19_VERSION_MINOR	2
#define LIBG19_VERSION_MICRO	0

#define LIBG19_VERSION			"1.2.0"

#define G19_BMP_SIZE	154112
#define G19_BMP_DSIZE	153600

typedef struct
{
	char * name;
	uint16_t vendor_id;
	uint16_t product_id;
}
G19Device;

enum G19UpdateFlags
{
	/*
	 * Flags the screen to be
	 * used and as of right now
	 * there is only one which is
	 * the G19.
	 */
	G19_SCREEN_DEFAULT	= 1 << 0,
	
	/* Flags to prepend header data */
	G19_PREPEND_HDATA	= 1 << 1,
	
	/* Flags not to parse the data
	 * and format it
	 */
	G19_DATA_TYPE_RAW	= 1 << 2,
	
	/* Flags to format a bitmap with
	 * with just bitmap data that has
	 * 4 bytes per pixel. (RGBA)
	 */
	G19_DATA_TYPE_BMP	= 1 << 3
};

enum G19Keys
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

typedef void(* g19_keys_cb)(unsigned int keys);

/**
 * Initializes the g19 library
 * 
 * @param level sets the debug level of libusb (0 - 3)
 * 
 * @return non zero on error
 **/
int g19_init(int level);

/**
 * Deinitializes the g19 library
 **/
void g19_deinit(void);

/**
 * Sets a callback for the G-Key and M-Key keypresses
 * Initializes listening for the G-Keys and M-Keys
 * 
 * @param cb callback
 **/
void g19_set_gkeys_cb(g19_keys_cb cb);

/**
 * Sets a callback for the L-Key keypresses
 * Initializes listening for the L-Keys
 * 
 * @param cb callback
 **/
void g19_set_lkeys_cb(g19_keys_cb cb);

/**
 * Sends raw data to the lcd without formatting
 * 
 * @param data pointer to the screen data
 * @param size size of the data to be written in bytes
 **/
void g19_update_lcd(unsigned char * data, size_t size, unsigned int flags);

/**
 * Sets the backlighting color
 * 
 * @param r amount of red (0 - 255)
 * @param g amount of green (0 - 255)
 * @param b amount of blue (0 - 255)
 * 
 * @return non zero on error
 **/
int g19_set_backlight(int r, int g, int b);

/**
 * Sets the M-Key lights
 * 
 * @param keys can be any of: G19_KEY_M1, G19_KEY_M2, G19_KEY_M3, G19_KEY_MR; For more than one use bitwise or
 * 
 * @return non zero on error
 **/
int g19_set_mkey_led(unsigned int keys);

#ifdef __cplusplus
}
#endif

#endif
