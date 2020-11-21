#include <linux/usb/ch9.h>
#include <usbg/usbg.h>
#include <usbg/function/hid.h>

#define VENDOR          0x1d6b
#define PRODUCT         0x0105
#define HID_REPORT_SIZE 16
#define MOUSE_REPORT_SIZE 4

static usbg_state *state;
static usbg_gadget *gadget;
static usbg_config *config;
static usbg_function *f_hid;

int initUSB();
int cleanupUSB();
