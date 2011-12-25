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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libg19.h"
#include "hdata.h"

static G19Device g19_devices[] = {
    {"Logitech G19 LCD", 0x046d, 0xc229}
};

static libusb_context * usb_ctx;
static libusb_device_handle * g19_devh;

static ssize_t devc;
static libusb_device ** dlist;

static unsigned char quit;
static pthread_t usb_et;

static struct libusb_transfer * gkeys_transfer  = NULL;
static struct libusb_transfer * gkeysc_transfer = NULL;
static struct libusb_transfer * lkeys_transfer  = NULL;

static G19GKeysFunc gkeys_func;
static G19LKeysFunc lkeys_func;

static void usb_event_thread(void * data)
{
    struct timeval tv;
    
    while(!quit) {
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        
        libusb_handle_events_timeout(usb_ctx, &tv);
    }
}

static int g19_device_proc(G19Device * g19dev)
{
    struct libusb_device_descriptor devd;
    struct libusb_config_descriptor * cfgd;
    
    const struct libusb_interface * intf;
    const struct libusb_interface_descriptor * intfd;
    
    int m, c, i, d;
    int fail;
    
    /* This needs cleaning up; all these nested statements are ugly */
    
    for(m = 0, fail = 1; m < devc; m++) {
        if(libusb_get_device_descriptor(dlist[m], &devd))
            continue;
        
        if((devd.idVendor != g19dev->vendor_id) || (devd.idProduct != g19dev->product_id))
            continue;
        
        if(libusb_open(dlist[m], &g19_devh))
            continue;
        
        for(c = 0; c < devd.bNumConfigurations; c++) {
            if(libusb_get_config_descriptor(dlist[m], c, &cfgd))
                continue;
            
            for(i = 0; i < cfgd->bNumInterfaces; i++) {
                intf = &cfgd->interface[i];
                
                for(d = 0; d < intf->num_altsetting; d++) {
                    intfd = &intf->altsetting[d];
                    
                    if(libusb_kernel_driver_active(g19_devh, intfd->bInterfaceNumber))
                        libusb_detach_kernel_driver(
                            g19_devh, intfd->bInterfaceNumber);
                    
                    libusb_set_configuration(
                        g19_devh, cfgd->bConfigurationValue);
                    
                    fail = libusb_claim_interface(
                        g19_devh, intfd->bInterfaceNumber);
                    
                    if(fail)
                        fail = 0;
                }
            }
            
            libusb_free_config_descriptor(cfgd);
        }
        
        if(!fail)
            return LIBUSB_SUCCESS;
        
        libusb_close(g19_devh);
    }
    
    g19_devh = NULL;
    return LIBUSB_ERROR_NO_DEVICE;
}

/**
 * Initializes the g19 library
 * 
 * @level   the debug level of libusb (0 - 3)
 * 
 * @return  0 on success, or non-zero on error.  Refer to the libusb
 *          error messages.
 **/
int g19_init(int level)
{
    int res, size, i;
    
    if(usb_ctx != NULL)
        return LIBUSB_ERROR_BUSY;
    
    res = libusb_init(&usb_ctx);
    
    if(res)
        return res;
    
    libusb_set_debug(usb_ctx, level);
    
    devc = libusb_get_device_list(usb_ctx, &dlist);
    
    if(devc < 1)
        return LIBUSB_ERROR_NO_DEVICE;
    
    size = sizeof g19_devices / sizeof (G19Device);
    
    for(i = 0; i < size; i++) {
        if(!g19_device_proc(&g19_devices[i]))
            break;
    }
    
    quit = 0;
    pthread_create(&usb_et, NULL, (void *) usb_event_thread, NULL);
    
    return 0;
}

/**
 * Deinitializes the g19 library
 **/
void g19_deinit(void)
{
    if(g19_devh != NULL) {
        libusb_release_interface(g19_devh, 0);
        libusb_reset_device(g19_devh);
        libusb_close(g19_devh);
    }
    
    quit = 1;
    
    if(gkeysc_transfer != NULL)
        libusb_free_transfer(gkeysc_transfer);
    
    if(gkeys_transfer != NULL)
        libusb_free_transfer(gkeys_transfer);
    
    if(lkeys_transfer != NULL)
        libusb_free_transfer(lkeys_transfer);
    
    if(dlist != NULL)
        libusb_free_device_list(dlist, 1);
    
    if(usb_ctx != NULL);
        libusb_exit(usb_ctx);
    
    pthread_join(usb_et, NULL);
    pthread_exit(NULL);
}

static void g19_gkey_cb(struct libusb_transfer * transfer)
{
    unsigned int keys;
    
    memcpy(&keys, transfer->buffer, 4);
    gkeys_func(keys);
    
    libusb_submit_transfer(gkeysc_transfer);
    
    /* Give the control packet time to do it's job */
    usleep(12000);
    
    libusb_submit_transfer(gkeys_transfer);
}

static void g19_lkey_cb(struct libusb_transfer * transfer)
{
    unsigned short keys;
    
    memcpy(&keys, transfer->buffer, 2);
    lkeys_func(keys);
    
    libusb_submit_transfer(lkeys_transfer);
}

/**
 * Sets a callback that is called upon keypress event(s) from the
 * G-Keys and M-Keys being triggered
 * 
 * @func  a pointer to a G19GKeysFunc
 **/
void g19_set_gkeys_cb(G19GKeysFunc func)
{
    unsigned char data[4];
    unsigned char cdata[7];
    
    if(g19_devh == NULL)
        return;
    
    if(gkeys_transfer)
        libusb_free_transfer(gkeys_transfer);
    
    if(gkeysc_transfer)
        libusb_free_transfer(gkeysc_transfer);
    
    gkeys_func = func;
    
    gkeys_transfer  = libusb_alloc_transfer(0);
    gkeysc_transfer = libusb_alloc_transfer(0);
    
    libusb_fill_interrupt_transfer(gkeys_transfer, g19_devh,
        0x83, data, 4, g19_gkey_cb, NULL, 0);
    
    libusb_fill_interrupt_transfer(gkeysc_transfer, g19_devh,
        0x83, cdata, 7, NULL, NULL, 7);
    
    libusb_submit_transfer(gkeys_transfer);
    
}

/**
 * Sets a callback that is called upon keypress event(s) from the
 * L-Keys being triggered
 * 
 * @cb  a pointer to a G19LKeysFunc
 **/
void g19_set_lkeys_cb(G19LKeysFunc func)
{
    unsigned char data[2];
    
    if(g19_devh == NULL)
        return;
    
    if(lkeys_transfer)
        libusb_free_transfer(lkeys_transfer);
    
    lkeys_func = func;
    
    lkeys_transfer = libusb_alloc_transfer(0);
    
    libusb_fill_interrupt_transfer(lkeys_transfer, g19_devh,
        0x81, data, 2, g19_lkey_cb, NULL, 0);
    
    libusb_submit_transfer(lkeys_transfer);
}

/**
 * Sends raw data or a bitmap to the LCD screen
 * 
 * @data   pointer to the screen data
 * @size   size of the data to be written in bytes
 * @flags  G19UpdateFlags describing the data
 **/
void g19_update_lcd(unsigned char * data, size_t size, unsigned int flags)
{
    struct libusb_transfer * transfer;
    unsigned char * bits;
    
    if((g19_devh == NULL) || (size < 1))
        return;
    
    transfer        = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
    
    bits = malloc(G19_BMP_SIZE);
    memset(bits, 0x00, G19_BMP_SIZE);
    
    if((flags & G19_PREPEND_HDATA) || (flags & G19_DATA_TYPE_BMP)) {
        memcpy(bits, hdata, HDATA_SIZE);
        
        if(flags & G19_DATA_TYPE_BMP) {
            unsigned int color;
            int i, d;
            
            i = HDATA_SIZE;
            d = 0;
            
            for(; (i < G19_BMP_DSIZE) && (d < size); i += 2, d += 4) {
                color  = (data[d] / 8) << 11;
                color |= (data[d + 1] / 4) << 5;
                color |= data[d + 2] / 8;
                
                memcpy(bits + i, &color, 2);
            }
        } else if(flags & G19_PREPEND_HDATA) {
            memcpy(bits + HDATA_SIZE, data,
                (size > G19_BMP_SIZE) ? G19_BMP_DSIZE : size);
        }
        
        libusb_fill_bulk_transfer(transfer, g19_devh, 0x02,
            bits, G19_BMP_SIZE, NULL, NULL, 0);
    } else {
        libusb_fill_bulk_transfer(transfer, g19_devh, 0x02,
            data, size, NULL, NULL, 0);
    }
    
    libusb_submit_transfer(transfer);
}

/**
 * Sets the backlight color
 * 
 * @r       amount of red (0 - 255)
 * @g       amount of green (0 - 255)
 * @b       amount of blue (0 - 255)
 * 
 * @return  0 on success, or non-zero on error.  Refer to the libusb
 *          error messages.
 **/
int g19_set_backlight(unsigned char r, unsigned char g, unsigned char b)
{
    struct libusb_transfer * transfer;
    unsigned char data[12];
    
    if(g19_devh == NULL)
        return LIBUSB_ERROR_NO_DEVICE;
    
    transfer        = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
    
    data[8]  = 255;
    data[9]  = r;
    data[10] = g;
    data[11] = b;
    
    libusb_fill_control_setup(data, 0x21, 9, 0x307, 1, 4);
    libusb_fill_control_transfer(transfer, g19_devh, data, NULL, NULL, 0);
    
    return libusb_submit_transfer(transfer);
}

/**
 * Sets the M-Key LEDs. To turn of an M-Key LED, call this function
 * with a @keys being 0, and then set the keys that needs to be on by
 * calling this function again.
 * 
 * @keys    Can be any of the following keys: G19_KEY_M1, G19_KEY_M2,
 *          G19_KEY_M3, G19_KEY_MR.  Additionally, more than one can
 *          be set at a time with a bitwise OR.
 * 
 * @return  0 on success, or non-zero on error.  Refer to the libusb
 *          error messages.
 **/
int g19_set_mkey_led(unsigned int keys)
{
    struct libusb_transfer * transfer;
    unsigned char data[10];
    
    if(g19_devh == NULL)
        return LIBUSB_ERROR_NO_DEVICE;
    
    transfer        = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
    
    data[8] = 0x10;
    data[9] = 0x00;
    
    if(keys & G19_KEY_M1)
        data[9] |= 0x10 << 3;
    
    if(keys & G19_KEY_M2)
        data[9] |= 0x10 << 2;
    
    if(keys & G19_KEY_M3)
        data[9] |= 0x10 << 1;
    
    if(keys & G19_KEY_MR)
        data[9] |= 0x10 << 0;
    
    libusb_fill_control_setup(data, 0x21, 9, 0x305, 1, 2);
    libusb_fill_control_transfer(transfer, g19_devh, data, NULL, NULL, 0);
    
    return libusb_submit_transfer(transfer);
}
