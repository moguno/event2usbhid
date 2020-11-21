#include "gadget-hid.h"
#include <errno.h>
#include <stdio.h>
#include <linux/usb/ch9.h>
#include <usbg/usbg.h>
#include <usbg/function/hid.h>
#include <usbg/function/midi.h>

static char report_desc_keyboard[] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x06, // USAGE (Keyboard)
    0xA1, 0x01, // COLLECTION (Application)

    0x85, 0x01, // REPORT_ID (1)

    //             Bitmapped Modifiers

    0x05, 0x07, // USAGE_PAGE (Key Codes)
    0x19, 0xE0, // USAGE_MINIMUM (224)
    0x29, 0xE7, // USAGE_MAXIMUM (231)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x08, // REPORT_COUNT (8)
    0x81, 0x02, // INPUT (Data, Variable, Absolute)

    //             Reserved

    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x08, // REPORT_SIZE (8)
    0x81, 0x01, // INPUT (Constant)

    //             LEDs

    0x05, 0x08, // USAGE_PAGE (LED)
    0x19, 0x01, // USAGE_MINIMUM (1)
    0x29, 0x05, // USAGE_MAXIMUM (5)
    0x95, 0x05, // REPORT_COUNT (5)
    0x75, 0x01, // REPORT_SIZE (1)
    0x91, 0x02, // OUTPUT (Data, Variable, Absolute)

    //             Padding

    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x03, // REPORT_SIZE (3)
    0x91, 0x03, // OUTPUT (Constant)

    //             Keyboard Keys

    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0xFF, // LOGICAL_MAXIMUM (255)
    0x05, 0x07, // USAGE_PAGE (Key Codes)
    0x19, 0x00, // USAGE_MINIMUM (0)
    0x29, 0xFF, // USAGE_MAXIMUM (255)
    0x95, 13,   // REPORT_COUNT (12)
    0x75, 0x08, // REPORT_SIZE (8)
    0x81, 0x00, // INPUT (Data, Array, Absolute)

    0xC0,       // END_COLLECTION

    //             Media Keys

    0x05, 0x0C, // USAGE_PAGE (Conumer)
    0x09, 0x01, // USAGE (Consumer Control)
    0xA1, 0x01, // Collection (Application)

    0x85, 0x02, //   Report ID (2)

    0x05, 0x0C, //   Usage Page (Consumer)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)

    0x95, 0x06, //   Report Count (6)
    0x09, 0xB5, //   Usage (Scan Next Track)
    0x09, 0xB6, //   Usage (Scan Previous Track)
    0x09, 0xB7, //   Usage (Stop)
    0x09, 0xB8, //   Usage (Eject)
    0x09, 0xCD, //   Usage (Play/Pause)
    0x09, 0xE2, //   Usage (Mute)
    0x81, 0x06, //   Input (Data,Var,Rel)

    0x95, 0x02, //   Report Count (2)
    0x09, 0xE9, //   Usage (Volume Increment)
    0x09, 0xEA, //   Usage (Volume Decrement)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0xC0,       // END_COLLECTION
};


static char report_desc2[] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x02, // USAGE (Mouse)
    0xa1, 0x01, // COLLECTION (Application)
    0x09, 0x01, //   USAGE (Pointer)
    0xa1, 0x00, //   COLLECTION (Physical)
    0x85, 0x03, //     Report ID (3)
    0x05, 0x09, //     USAGE_PAGE (Button)
    0x19, 0x01, //     USAGE_MINIMUM (Button 1)
    0x29, 0x03, //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00, //     LOGICAL_MINIMUM (0)
    0x25, 0x01, //     LOGICAL_MAXIMUM (1)
    0x95, 0x03, //     REPORT_COUNT (3)
    0x75, 0x01, //     REPORT_SIZE (1)
    0x81, 0x02, //     INPUT (Data,Var,Abs)
    0x95, 0x01, //     REPORT_COUNT (1)
    0x75, 0x05, //     REPORT_SIZE (5)
    0x81, 0x03, //     INPUT (Cnst,Var,Abs)
    0x05, 0x01, //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30, //     USAGE (X)
    0x09, 0x31, //     USAGE (Y)
    0x09, 0x38, //     USAGE (WHEEL)
    0x15, 0x81, //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f, //     LOGICAL_MAXIMUM (127)
    0x75, 0x08, //     REPORT_SIZE (8)
    0x95, 0x03, //     REPORT_COUNT (3)
    0x81, 0x06, //     INPUT (Data,Var,Rel)
    0xc0,       //   END_COLLECTION
    0xc0        // END_COLLECTION
};

int initUSB() {
    int ret = -EINVAL;
    int usbg_ret;
    usbg_config *config;
    usbg_function *f_hid_keyboard;
    usbg_function *f_hid_mouse;

    struct usbg_gadget_attrs g_attrs = {
        .bcdUSB = 0x0200,
        .bDeviceClass = USB_CLASS_PER_INTERFACE,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 64, /* Max allowed ep0 packet size */
        .idVendor = VENDOR,
        .idProduct = PRODUCT,
        .bcdDevice = 0x0001, /* Verson of device */
    };

    struct usbg_gadget_strs g_strs = {
        .serial = "0123456789", /* Serial number */
        .manufacturer = "moguno", /* Manufacturer */
        .product = "もぐのの謎デバイス" /* Product string */
    };

    struct usbg_config_strs c_strs = {
        .configuration = "1xHID"
    };

    struct usbg_f_hid_attrs f_attrs_keyboard = {
        .protocol = 1,
        .report_desc = {
            .desc = report_desc_keyboard,
            .len = sizeof(report_desc_keyboard),
        },
        .report_length = 16,
        .subclass = 0,
    };

    struct usbg_f_hid_attrs f_attrs_mouse = {
        .protocol = 2,
        .report_desc = {
            .desc = report_desc2,
            .len = sizeof(report_desc2),
        },
        .report_length = 16,
        .subclass = 0,
    };

    usbg_ret = usbg_init("/sys/kernel/config", &state);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error on usbg init\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out1;
    }

    usbg_ret = usbg_create_gadget(state, "g1", &g_attrs, &g_strs, &gadget);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error creating gadget\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }

    usbg_ret = usbg_create_function(gadget, USBG_F_HID, "usb0", &f_attrs_keyboard, &f_hid_keyboard);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error creating function: USBG_F_HID\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }

    usbg_ret = usbg_create_function(gadget, USBG_F_HID, "usb1", &f_attrs_mouse, &f_hid_mouse);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error creating function: USBG_F_HID\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }


    usbg_ret = usbg_create_config(gadget, 1, "config", NULL, &c_strs, &config);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error creating config\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }

    usbg_ret = usbg_add_config_function(config, "keyboard", f_hid_keyboard);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error adding function: keyboard\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }

    usbg_ret = usbg_add_config_function(config, "mouse", f_hid_mouse);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error adding function: keyboard\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }

    usbg_ret = usbg_enable_gadget(gadget, DEFAULT_UDC);
    if (usbg_ret != USBG_SUCCESS) {
        fprintf(stderr, "Error enabling gadget\n");
        fprintf(stderr, "Error: %s : %s\n", usbg_error_name(usbg_ret),
                usbg_strerror(usbg_ret));
        goto out2;
    }

    ret = 0;

out2:
    usbg_cleanup(state);

out1:
    return ret;
}

int cleanupUSB(){
    if(gadget){
        usbg_disable_gadget(gadget);
        usbg_rm_gadget(gadget, USBG_RM_RECURSE);
    }
    if(state){
        usbg_cleanup(state);
    }
    return 0;
}
