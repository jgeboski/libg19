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

#include <assert.h>
#include <libusb.h>
#include <string.h>

#include "libg19.h"

/**
 * Initializes a #G19Device with the specified device.
 *
 * @param dev   The #G19Device.
 * @param devs  The list of #libusb_device.
 * @param index The device index.
 *
 * @return The #G19Device or NULL on error.
 **/
static int g19_device_init(G19Device *dev, libusb_device **devs, size_t index)
{
    int devc;
    int res;
    int i;
    int j;
    int k;

    libusb_device *udev;

    struct libusb_device_descriptor           desc;
    struct libusb_config_descriptor          *cfgd;
    const struct libusb_interface            *intf;
    const struct libusb_interface_descriptor *intfd;

    for (i = 0, devc = 0; devs[i] != NULL; i++) {
        res = libusb_get_device_descriptor(devs[i], &desc);

        if (res != LIBUSB_SUCCESS)
            return res;

        if ((desc.idVendor  == G19_VENDOR_ID) &&
            (desc.idProduct == G19_PRODUCT_ID) &&
            (devc++ == index))
        {
            res = libusb_open(devs[i], (libusb_device_handle**) &dev->hndl);

            if (res != LIBUSB_SUCCESS)
                return res;

            udev = devs[i];
            break;
        }
    }

    if (udev == NULL)
        return LIBUSB_ERROR_NO_DEVICE;

    for (i = 0; i < desc.bNumConfigurations; i++) {
        res = libusb_get_config_descriptor(udev, i, &cfgd);

        if (res != LIBUSB_SUCCESS)
            return res;

        for (j = 0; j < cfgd->bNumInterfaces; j++) {
            intf = &cfgd->interface[j];

            for (k = 0; k < intf->num_altsetting; k++) {
                intfd = &intf->altsetting[k];
                res   = libusb_set_auto_detach_kernel_driver(dev->hndl, 1);

                if (res != LIBUSB_SUCCESS)
                    return res;

                res = libusb_claim_interface(dev->hndl, intfd->bInterfaceNumber);

                if (res != LIBUSB_SUCCESS)
                    return res;
            }
        }
    }

    return LIBUSB_SUCCESS;
}

/**
 * Implemented #libusb_transfer_cb_fn for the G-keys.
 *
 * @param transfer The #libusb_transfer.
 **/
static void g19_device_gkey_cb(struct libusb_transfer *transfer)
{
    G19Device *dev = transfer->user_data;
    uint32_t   keys;

    memset(&keys, 0, sizeof keys);
    memcpy(&keys, transfer->buffer, 4);

    libusb_submit_transfer(dev->gkctrn);
    libusb_submit_transfer(dev->gktrn);

    if (dev->keys != NULL)
        dev->keys(dev, keys, dev->data);
}

/**
 * Implemented #libusb_transfer_cb_fn for the L-keys.
 *
 * @param transfer The #libusb_transfer.
 **/
static void g19_device_lkey_cb(struct libusb_transfer *transfer)
{
    G19Device *dev = transfer->user_data;
    uint32_t   keys;

    memset(&keys, 0, sizeof keys);
    memcpy(&keys, transfer->buffer, 2);
    libusb_submit_transfer(dev->lktrn);

    if (dev->keys != NULL)
        dev->keys(dev, keys, dev->data);
}

/**
 * Opens a new #G19Device. The returned #G19Device should be closed with
 * #g19_device_close() when no longer needed.
 *
 * @param index The device index.
 * @param error The return location for a #libusb_error or NULL.
 *
 * @return The #G19Device or NULL on error.
 **/
G19Device *g19_device_open(size_t index, int *error)
{
    G19Device      *dev;
    libusb_device **devs;
    size_t          devc;
    uint8_t         data[7];
    int             res;

    dev = calloc(sizeof *dev, 1);
    assert(dev != NULL);
    memset(dev, 0, sizeof *dev);
    res = libusb_init((libusb_context**) &dev->ctx);

    if (res != LIBUSB_SUCCESS)
        goto error;

#ifdef DEBUG
    libusb_set_debug(dev->ctx, LIBUSB_LOG_LEVEL_DEBUG);
#endif

    devc = libusb_get_device_list(dev->ctx, &devs);

    if (devc < LIBUSB_SUCCESS)
        goto error;

    res = g19_device_init(dev, devs, index);
    libusb_free_device_list(devs, 1);

    if (res != LIBUSB_SUCCESS)
        goto error;

    dev->lktrn  = libusb_alloc_transfer(0);
    dev->gktrn  = libusb_alloc_transfer(0);
    dev->gkctrn = libusb_alloc_transfer(0);

    libusb_fill_interrupt_transfer(dev->lktrn, dev->hndl, 0x81, data, 2,
                                   g19_device_lkey_cb, dev, 0);
    libusb_fill_interrupt_transfer(dev->gktrn, dev->hndl, 0x83, data, 4,
                                   g19_device_gkey_cb, dev, 0);
    libusb_fill_interrupt_transfer(dev->gkctrn, dev->hndl, 0x83, data, 7,
                                   NULL, NULL, 7);

    res = libusb_submit_transfer(dev->lktrn);

    if (res != LIBUSB_SUCCESS)
        goto error;

    res = libusb_submit_transfer(dev->gktrn);

    if (res != LIBUSB_SUCCESS)
        goto error;

    if (error != NULL)
        *error = LIBUSB_SUCCESS;

    libusb_lock_events(dev->ctx);
    return dev;

error:
    if (error != NULL)
        *error = res;

    g19_device_close(dev);
    return NULL;
}

/**
 * Closes and frees all memory used by a #G19Device.
 *
 * @param dev The #G19Device.
 **/
void g19_device_close(G19Device *dev)
{
    if (dev == NULL)
        return;

    if (dev->gkctrn == NULL)
        libusb_free_transfer(dev->gkctrn);

    if (dev->gktrn == NULL)
        libusb_free_transfer(dev->gktrn);

    if (dev->lktrn == NULL)
        libusb_free_transfer(dev->lktrn);

    if (dev->hndl != NULL) {
        //libusb_reset_device(dev->hndl);
        libusb_close(dev->hndl);
    }

    if (dev->ctx != NULL)
        libusb_exit(dev->ctx);

    free(dev);
}

/**
 * Gets a NULL-terminated list of file descriptors for polling. The
 * returned list should be freed with #free() when no longer needed.
 *
 * @param dev  The #G19Device.
 * @param size The return location for the size or NULL.
 *
 * @return The list of #G19PollFDs or NULL on error.
 **/
G19PollFD *g19_device_pollfds(G19Device *dev, size_t *size)
{
    const struct libusb_pollfd **fds;
    G19PollFD *ret;
    int        i;

    if (dev == NULL)
        return NULL;

    fds = libusb_get_pollfds(dev->ctx);

    if (fds == NULL)
        return NULL;

    for (i = 0; fds[i] != NULL; i++);
    ret = calloc(sizeof *ret, i + 1);
    assert(ret != NULL);
    memset(ret, 0, (sizeof (*ret)) * (i + 1));

    for (i = 0; fds[i] != NULL; i++) {
        ret[i].fd     = fds[i]->fd;
        ret[i].events = fds[i]->events;
    }

    if (size != NULL)
        *size = i;

    return ret;
}

/**
 * Gets the poll timeout. If this timeout threshold is reached, and no
 * events were caught, #g19_device_pollev() must be called.
 *
 * @param dev The #G19Device.
 * @param tv  The return location for a #timeval.
 *
 * @return 0 if no timeout, 1 if there was a timeout, or the
 *         #libusb_error.
 **/
int g19_device_pollto(G19Device *dev, struct timeval *tv)
{
    if ((dev == NULL) || (tv == NULL))
        return LIBUSB_ERROR_INVALID_PARAM;

    return libusb_get_next_timeout(dev->ctx, tv);
}

/**
 * Handles polled events.
 *
 * @param dev The #G19Device.
 *
 * @return The #libusb_error (0 on success).
 **/
int g19_device_pollev(G19Device *dev)
{
    static struct timeval tv = {0, 0};

    if (dev == NULL)
        return LIBUSB_ERROR_INVALID_PARAM;

    return libusb_handle_events_locked(dev->ctx, &tv);
}

/**
 * Gets number of G19 devices.
 *
 * @return The #libusb_error (0 on success).
 **/
ssize_t g19_device_count(void)
{
    libusb_context  *ctx;
    libusb_device  **devs;
    ssize_t          devc;
    int              res;
    int              i;

    struct libusb_device_descriptor desc;

    res = libusb_init(&ctx);

    if (res != LIBUSB_SUCCESS)
        return res;

    devc = libusb_get_device_list(ctx, &devs);

    if (devc < LIBUSB_SUCCESS) {
        libusb_exit(ctx);
        return devc;
    }

    for (i = 0, devc = 0; devs[i] != NULL; i++) {
        res = libusb_get_device_descriptor(devs[i], &desc);

        if ((res == LIBUSB_SUCCESS) &&
            (desc.idVendor  == G19_VENDOR_ID) &&
            (desc.idProduct == G19_PRODUCT_ID))
        {
            devc++;
        }
    }

    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);

    return devc;
}

/**
 * Sets LCD display via a bitmap.
 *
 * @param dev  The #G19Device.
 * @param data The LCD data.
 * @param size The size of the data.
 * @param type The #G19UpdateType.
 *
 * @return The #libusb_error (0 on success).
 **/
int g19_device_lcd(G19Device *dev, uint8_t *data, size_t size,
                    G19UpdateType type)
{
    struct libusb_transfer *transfer;
    uint8_t  *bits;
    uint16_t  color;
    int       i;
    int       d;

    if ((dev == NULL) || (data == NULL) || (size < 0))
        return LIBUSB_ERROR_INVALID_PARAM;

    transfer = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;

    bits = malloc(G19_BMP_SIZE);

    memset(bits, 0x00, G19_BMP_SIZE);
    memcpy(bits, g19_data_hdr, G19_DATA_HDR_SIZE);

    if (!(type & G19_UPDATE_TYPE_RAW)) {
        i = G19_DATA_HDR_SIZE;
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
        if (size > G19_BMP_DSIZE)
            size = G19_BMP_DSIZE;

        memcpy(bits + G19_DATA_HDR_SIZE, data, size);
    }

    libusb_fill_bulk_transfer(transfer, dev->hndl, 0x02, bits, G19_BMP_SIZE,
                              NULL, NULL, 0);
    return libusb_submit_transfer(transfer);
}

/**
 * Sets the LCDs brightness level.
 *
 * @param dev   The #G19Device.
 * @param level The brightness (0 - 100).
 *
 * @return The #libusb_error (0 on success).
 **/
int g19_device_brightness(G19Device *dev, uint8_t brightness)
{
    struct libusb_transfer *transfer;
    uint8_t data[9];

    if (dev == NULL)
        return LIBUSB_ERROR_INVALID_PARAM;

    transfer = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;

    data[8] = brightness;

    libusb_fill_control_setup(data, 0x41, 0x10, 0x00, 0x00, 1);
    libusb_fill_control_transfer(transfer, dev->hndl, data, NULL, NULL, 0);

    return libusb_submit_transfer(transfer);
}

/**
 * Sets the backlight color.
 *
 * @param dev The #G19Device.
 * @param r   The red (0 - 255).
 * @param g   The green (0 - 255).
 * @param b   The blue (0 - 255).
 *
 * @return The #libusb_error (0 on success).
 **/
int g19_device_backlight(G19Device *dev, uint8_t r, uint8_t g, uint8_t b)
{
    struct libusb_transfer *transfer;
    uint8_t data[12];

    if (dev == NULL)
        return LIBUSB_ERROR_INVALID_PARAM;

    transfer = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;

    data[8]  = 255;
    data[9]  = r;
    data[10] = g;
    data[11] = b;

    libusb_fill_control_setup(data, 0x21, 0x09, 0x0307, 0x01, 4);
    libusb_fill_control_transfer(transfer, dev->hndl, data, NULL, NULL, 0);

    return libusb_submit_transfer(transfer);
}

/**
 * Sets the state of the M-Key LEDs.
 *
 * @param dev  The #G19Device.
 * @param keys The #G19GKeys.
 *
 * @return The #libusb_error (0 on success).
 **/
int g19_device_mkeys(G19Device *dev, uint32_t keys)
{
    struct libusb_transfer *transfer;
    uint8_t data[10];

    if (dev == NULL)
        return LIBUSB_ERROR_INVALID_PARAM;

    transfer = libusb_alloc_transfer(0);
    transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;

    data[8] = 0x10;
    data[9] = 0x00;

    if (keys & G19_KEY_M1)
        data[9] |= 0x10 << 3;

    if (keys & G19_KEY_M2)
        data[9] |= 0x10 << 2;

    if (keys & G19_KEY_M3)
        data[9] |= 0x10 << 1;

    if (keys & G19_KEY_MR)
        data[9] |= 0x10 << 0;

    libusb_fill_control_setup(data, 0x21, 0x09, 0x0305, 0x01, 2);
    libusb_fill_control_transfer(transfer, dev->hndl, data, NULL, NULL, 0);

    return libusb_submit_transfer(transfer);
}
