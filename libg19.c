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

#include <errno.h>
#include <libusb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libg19.h"
#include "hdata.h"

static G19Device g19_devices[] = {
	{"Logitech G19 LCD", 0x046d, 0xc229}
};

static libusb_context * usb_ctx;

static ssize_t devc;
static libusb_device ** dlist;

static int lib_deinit = 0;
static pthread_t event_thd;

static struct libusb_transfer * gkeys_transfer;
static struct libusb_transfer * gkeysc_transfer;
static struct libusb_transfer * lkeys_transfer;

static g19_keys_cb gkeys_cb = 0;
static g19_keys_cb lkeys_cb = 0;

static void g19_event_thread(void * data)
{
	struct timeval tv;
	
	while(!lib_deinit)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		libusb_handle_events_timeout(NULL, &tv);
	}
}

static int g19_device_proc(G19Device * g19dev)
{
	struct libusb_device_descriptor dev_desc;
	struct libusb_config_descriptor * cfg_desc;
	
	const struct libusb_interface * intf;
	const struct libusb_interface_descriptor * intfd;
	
	int m, c, i, d, e;
	
	for(m = 0; m < devc; m++)
	{
		if(libusb_get_device_descriptor(dlist[m], &dev_desc))
			continue;
		
		if((dev_desc.idVendor != g19dev -> vendor_id) || (dev_desc.idProduct != g19dev -> product_id))
			continue;
		
		if(libusb_open(dlist[m], &g19dev -> handle))
			continue;
		
		for(c = 0; c < dev_desc.bNumConfigurations; c++)
		{
			if(libusb_get_config_descriptor(dlist[m], c, &cfg_desc))
				continue;
			
			for(i = 0; i < cfg_desc -> bNumInterfaces; i++)
			{
				intf = &cfg_desc -> interface[i];
				
				for(d = 0; d < intf -> num_altsetting; d++)
				{
					intfd = &intf -> altsetting[d];
					
					if(libusb_kernel_driver_active(g19dev -> handle, intfd -> bInterfaceNumber))
						libusb_detach_kernel_driver(g19dev -> handle, intfd -> bInterfaceNumber);
					
					libusb_set_configuration(g19dev -> handle, cfg_desc -> bConfigurationValue);
					libusb_claim_interface(g19dev -> handle, intfd -> bInterfaceNumber);
					
					e = 0;
					while(libusb_claim_interface(g19dev -> handle, intfd -> bInterfaceNumber) && (e < 10))
					{
						sleep(1);
						e++;
					}
				}
			}
			
			libusb_free_config_descriptor(cfg_desc);
		}
		
		return 0;
	}
	
	g19dev -> handle = NULL;
	return 1;
}

int g19_init(int level)
{
	if(usb_ctx != NULL)
		return-1;
	
	int res = libusb_init(&usb_ctx);
	
	if(res)
		return res;
	
	libusb_set_debug(usb_ctx, level);
	
	devc = libusb_get_device_list(usb_ctx, &dlist);
	
	if(devc < 1)
		return -1;
	
	int i, d = 0, size = sizeof(g19_devices) / sizeof(G19Device);
	
	for(i = 0; i < size; i++)
	{
		if(!g19_device_proc(&g19_devices[i]))
			d++;
	}
	
	pthread_create(&event_thd, NULL, (void *) g19_event_thread, NULL);
	
	return d;
}

void g19_deinit(void)
{
	int i, size = sizeof(g19_devices) / sizeof(G19Device);
	
	for(i = 0; i < size; i++)
	{
		if(g19_devices[i].handle == NULL)
			continue;
		
		libusb_release_interface(g19_devices[i].handle, 0);
		libusb_reset_device(g19_devices[i].handle);
		libusb_close(g19_devices[i].handle);
	}
	
	lib_deinit = 1;
	
	if(gkeysc_transfer != NULL)
		libusb_free_transfer(gkeysc_transfer);
	
	if(gkeys_transfer != NULL)
		libusb_free_transfer(gkeys_transfer);
	
	if(lkeys_transfer != NULL)
		libusb_free_transfer(lkeys_transfer);
	
	libusb_free_device_list(dlist, 1);
	libusb_exit(usb_ctx);
	
	pthread_join(event_thd, NULL);
	pthread_exit(NULL);
}

static void g19_lkey_data_keys(unsigned int * keys, unsigned char * data)
{
	*keys = 0x00;
	
	if(data[0] & 0x1)
		*keys |= G19_KEY_LHOME;
	
	if(data[0] & 0x2)
		*keys |= G19_KEY_LCANCEL;
	
	if(data[0] & 0x4)
		*keys |= G19_KEY_LMENU;
	
	if(data[0] & 0x8)
		*keys |= G19_KEY_LOK;
	
	if(data[0] & 0x10)
		*keys |= G19_KEY_LRIGHT;
	
	if(data[0] & 0x20)
		*keys |= G19_KEY_LLEFT;
	
	if(data[0] & 0x40)
		*keys |= G19_KEY_LDOWN;
	
	if(data[0] & 0x80)
		*keys |= G19_KEY_LUP;
}

static void g19_gkey_data_keys(unsigned int * keys, unsigned char * data)
{
	*keys = 0x00;
	
	if(data[0] < 1)
		return;
	
	if((data[1] & 0x0) && (data[2] & 0x0))
		return;
	
	if(data[1] & 0x1)
		*keys |= G19_KEY_G1;
	
	if(data[1] & 0x2)
		*keys |= G19_KEY_G2;
	
	if(data[1] & 0x4)
		*keys |= G19_KEY_G3;
	
	if(data[1] & 0x8)
		*keys |= G19_KEY_G4;
	
	if(data[1] & 0x10)
		*keys |= G19_KEY_G5;
	
	if(data[1] & 0x20)
		*keys |= G19_KEY_G6;
	
	if(data[1] & 0x40)
		*keys |= G19_KEY_G7;
	
	if(data[1] & 0x80)
		*keys |= G19_KEY_G8;
	
	if(data[2] & 0x1)
		*keys |= G19_KEY_G9;
	
	if(data[2] & 0x2)
		*keys |= G19_KEY_G10;
	
	if(data[2] & 0x4)
		*keys |= G19_KEY_G11;
	
	if(data[2] & 0x8)
		*keys |= G19_KEY_G12;
	
	if(data[2] & 0x10)
		*keys |= G19_KEY_M1;
	
	if(data[2] & 0x20)
		*keys |= G19_KEY_M2;
	
	if(data[2] & 0x40)
		*keys |= G19_KEY_M3;
	
	if(data[2] & 0x80)
		*keys |= G19_KEY_MR;
}

static void g19_gkey_cb(struct libusb_transfer * transfer)
{
	unsigned int keys;
	g19_gkey_data_keys(&keys, transfer -> buffer);
	
	(gkeys_cb)(keys);
	
	libusb_submit_transfer(gkeysc_transfer);
	usleep(12000);
	
	libusb_submit_transfer(gkeys_transfer);
}

static void g19_lkey_cb(struct libusb_transfer * transfer)
{
	unsigned int keys;
	g19_lkey_data_keys(&keys, transfer -> buffer);
	
	(lkeys_cb)(keys);
	
	libusb_submit_transfer(lkeys_transfer);
}

void g19_set_gkeys_cb(g19_keys_cb cb)
{
	gkeys_cb = cb;
	
	gkeys_transfer = libusb_alloc_transfer(0);
	
	unsigned char data[4];
	libusb_fill_interrupt_transfer(gkeys_transfer, g19_devices[0].handle,
								   LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_OTHER,
								   data, 4, g19_gkey_cb, NULL, 0);
	
	gkeysc_transfer = libusb_alloc_transfer(0);
	
	unsigned char cdata[7];
	libusb_fill_interrupt_transfer(gkeysc_transfer, g19_devices[0].handle,
								   LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_OTHER,
								   cdata, 7, NULL, NULL, 7);
	
	libusb_submit_transfer(gkeys_transfer);
}

void g19_set_lkeys_cb(g19_keys_cb cb)
{
	lkeys_cb = cb;
	
	lkeys_transfer = libusb_alloc_transfer(0);
	
	unsigned char data[2];
	libusb_fill_interrupt_transfer(lkeys_transfer, g19_devices[0].handle,
								   LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE,
								   data, 2, g19_lkey_cb, NULL, 0);
	
	libusb_submit_transfer(lkeys_transfer);
}

void g19_update_lcd(unsigned char * data, int len)
{
	unsigned char bits[154112];
	int dsize = 154112 - sizeof(hdata);
	
	memset(&bits, 0x00, sizeof(bits));
	memcpy(&bits, &hdata, sizeof(hdata));
	memcpy(bits + 512, data, (len <= dsize) ? len : dsize);
	
	struct libusb_transfer * lcd_transfer = libusb_alloc_transfer(0);
	lcd_transfer -> flags = LIBUSB_TRANSFER_FREE_BUFFER;
	
	libusb_fill_bulk_transfer(lcd_transfer, g19_devices[0].handle, 0x02, bits, 154112, NULL, NULL, 0);
	libusb_submit_transfer(lcd_transfer);
}

int g19_set_backlight(int r, int g, int b)
{
	if(g19_devices[0].handle == NULL)
		return -1;
	
	unsigned char data[4] = {255, r, g, b};
	int res = libusb_control_transfer(g19_devices[0].handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 9, 0x307, 1, data, 4, 7);
	
	if(res < 0)
		return res;
	
	return 0;
}

int g19_set_mkey_led(unsigned int keys)
{
	unsigned char data[2] = {0x10, 0x00};
	
	if(g19_devices[0].handle == NULL)
		return -1;
	
	if(keys & G19_KEY_M1)
		data[1] |= 0x10 << 3;
	
	if(keys & G19_KEY_M2)
		data[1] |= 0x10 << 2;
	
	if(keys & G19_KEY_M3)
		data[1] |= 0x10 << 1;
	
	if(keys & G19_KEY_MR)
		data[1] |= 0x10 << 0;
	
	int res = libusb_control_transfer(g19_devices[0].handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 9, 0x305, 1, data, 2, 7);
	
	if(res)
		return res;
	
	return 0;
}
