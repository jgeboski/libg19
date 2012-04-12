/*
 * Copyright 2010-2012 James Geboski <jgeboski@gmail.com>
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

static libusb_context * usb_ctx       = NULL;
static libusb_device_handle * dhandle = NULL;

static ssize_t devc;
static libusb_device ** dlist;

static uint8_t quit;
static pthread_t usb_et;

static struct libusb_transfer * gkeys_transfer  = NULL;
static struct libusb_transfer * gkeysc_transfer = NULL;
static struct libusb_transfer * lkeys_transfer  = NULL;

static G19GKeysFunc gkeys_func = NULL;
static G19LKeysFunc lkeys_func = NULL;

static void usb_event_thread(void * data)
{
    struct timeval tv;
    
    while(!quit) {
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        
        libusb_handle_events_timeout(usb_ctx, &tv);
    }
    
    pthread_exit(NULL);
}

static int g19_device_proc()
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
        
        if((devd.idVendor != G19_VENDOR_ID) || (devd.idProduct != G19_PRODUCT_ID))
            continue;
        
        if(libusb_open(dlist[m], &dhandle))
            continue;
        
        for(c = 0; c < devd.bNumConfigurations; c++) {
            if(libusb_get_config_descriptor(dlist[m], c, &cfgd))
                continue;
            
            for(i = 0; i < cfgd->bNumInterfaces; i++) {
                intf = &cfgd->interface[i];
                
                for(d = 0; d < intf->num_altsetting; d++) {
                    intfd = &intf->altsetting[d];
                    
                    if(libusb_kernel_driver_active(dhandle, intfd->bInterfaceNumber))
                        libusb_detach_kernel_driver(
                            dhandle, intfd->bInterfaceNumber);
                    
                    libusb_set_configuration(
                        dhandle, cfgd->bConfigurationValue);
                    
                    fail = libusb_claim_interface(
                        dhandle, intfd->bInterfaceNumber);
                    
                    if(fail)
                        fail = 0;
                }
            }
            
            libusb_free_config_descriptor(cfgd);
        }
        
        if(fail != 0)
            return LIBUSB_SUCCESS;
        
        libusb_close(dhandle);
    }
    
    dhandle = NULL;
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
    int res;
    
    if(usb_ctx != NULL)
        return LIBUSB_ERROR_BUSY;
    
    res = libusb_init(&usb_ctx);
    libusb_set_debug(usb_ctx, level);
    
    if(res != 0)
        return res;
    
    devc = libusb_get_device_list(usb_ctx, &dlist);
    if(devc < 1)
        return LIBUSB_ERROR_NO_DEVICE;
    
    res = g19_device_proc();
    if(res != 0) {
        g19_deinit();
        return res;
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
    if(dhandle != NULL) {
        libusb_release_interface(dhandle, 0);
        libusb_reset_device(dhandle);
        libusb_close(dhandle);
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
    
    if(usb_ctx != NULL)
        libusb_exit(usb_ctx);
    
    pthread_join(usb_et, NULL);
}

static void g19_gkey_cb(struct libusb_transfer * transfer)
{
    uint32_t keys;
    
    memcpy(&keys, transfer->buffer, 4);
    gkeys_func(keys);
    
    libusb_submit_transfer(gkeysc_transfer);
    
    /* Give the control packet time to do it's job */
    usleep(12000);
    
    libusb_submit_transfer(gkeys_transfer);
}

static void g19_lkey_cb(struct libusb_transfer * transfer)
{
    uint16_t keys;
    
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
    uint8_t data[4];
    uint8_t cdata[7];
    
    if(dhandle == NULL)
        return;
    
    if(gkeys_transfer != NULL)
        libusb_free_transfer(gkeys_transfer);
    
    if(gkeysc_transfer != NULL)
        libusb_free_transfer(gkeysc_transfer);
    
    gkeys_func = func;
    
    gkeys_transfer  = libusb_alloc_transfer(0);
    gkeysc_transfer = libusb_alloc_transfer(0);
    
    libusb_fill_interrupt_transfer(gkeys_transfer, dhandle,
        0x83, data, 4, g19_gkey_cb, NULL, 0);
    
    libusb_fill_interrupt_transfer(gkeysc_transfer, dhandle,
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
    uint8_t data[2];
    
    if(dhandle == NULL)
        return;
    
    if(lkeys_transfer != NULL)
        libusb_free_transfer(lkeys_transfer);
    
    lkeys_func = func;
    
    lkeys_transfer = libusb_alloc_transfer(0);
    
    libusb_fill_interrupt_transfer(lkeys_transfer, dhandle,
        0x81, data, 2, g19_lkey_cb, NULL, 0);
    
    libusb_submit_transfer(lkeys_transfer);
}

/**
 * Sends raw data or a bitmap to the LCD screen
 * 
 * @data  pointer to the LCD screen data
 * @size  size in bytes of @data
 * @type  the G19UpdateType to be used
 **/
void g19_update_lcd(uint8_t * data, size_t size, G19UpdateType type)
{
    struct libusb_transfer * transfer;
    uint8_t * bits;
    uint16_t color;
    int i, d;
    
    if((dhandle == NULL) || (size < 1))
        return;
    
    transfer        = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
    
    bits = malloc(G19_BMP_SIZE);
    
    memset(bits, 0x00, G19_BMP_SIZE);
    memcpy(bits, hdata, HDATA_SIZE);
    
    if(!(type & G19_UPDATE_TYPE_RAW)) {
        i = HDATA_SIZE;
        d = 0;
        
        /* Convert from 32-bit bitmap to 16-bit while ignoring
         * the alpha pixel
         */
        for(; (i < G19_BMP_DSIZE) && (d < size); i += 2, d += 4) {
            color  = (data[d] / 8) << 11;
            color |= (data[d + 1] / 4) << 5;
            color |= data[d + 2] / 8;
            
            memcpy(bits + i, &color, 2);
        }
    } else {
        if(size > G19_BMP_DSIZE)
            size = G19_BMP_DSIZE;
        
        memcpy(bits + HDATA_SIZE, data, size);
    }
    
    libusb_fill_bulk_transfer(transfer, dhandle, 0x02,
            bits, G19_BMP_SIZE, NULL, NULL, 0);
    
    libusb_submit_transfer(transfer);
}

/**
 * Sets the LCDs brightness level
 * 
 * @level  the brightness level (0 - 100)
 * 
 * return  0 on success, or non-zero on error.  Refer to the libusb
 *         error messages.
 **/
int g19_set_brightness(uint8_t level)
{
    struct libusb_transfer * transfer;
    uint8_t data[9];
    
    if(dhandle == NULL)
        return LIBUSB_ERROR_NO_DEVICE;
    
    transfer        = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
    
    data[8] = level;
    
    libusb_fill_control_setup(data, 0x41, 10, 0, 0, 1);
    libusb_fill_control_transfer(transfer, dhandle, data, NULL, NULL, 0);
    
    return libusb_submit_transfer(transfer);
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
int g19_set_backlight(uint8_t r, uint8_t g, uint8_t b)
{
    struct libusb_transfer * transfer;
    uint8_t data[12];
    
    if(dhandle == NULL)
        return LIBUSB_ERROR_NO_DEVICE;
    
    transfer        = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
    
    data[8]  = 255;
    data[9]  = r;
    data[10] = g;
    data[11] = b;
    
    libusb_fill_control_setup(data, 0x21, 9, 0x307, 1, 4);
    libusb_fill_control_transfer(transfer, dhandle, data, NULL, NULL, 0);
    
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
int g19_set_mkey_led(uint32_t keys)
{
    struct libusb_transfer * transfer;
    uint8_t data[10];
    
    if(dhandle == NULL)
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
    libusb_fill_control_transfer(transfer, dhandle, data, NULL, NULL, 0);
    
    return libusb_submit_transfer(transfer);
}
