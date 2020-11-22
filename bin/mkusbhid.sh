#! /bin/bash

declare -A g_attrs
g_attrs["bcdUSB"]="0x0200"
g_attrs["bDeviceClass"]="0x00"
g_attrs["bDeviceSubClass"]="0x00"
g_attrs["bDeviceProtocol"]="0x00"
g_attrs["bMaxPacketSize0"]="64"
g_attrs["idVendor"]="0x1d6b"
g_attrs["idProduct"]="0x0105"
g_attrs["bcdDevice"]="0x0001"

declare -A g_strs
g_strs["serialnumber"]="0123456789"
g_strs["manufacturer"]="moguno"
g_strs["product"]="HID Converter"

declare -A c_strs
c_strs["configuration"]="1xHID"

declare -A report_desc_keyboard

desc=""

desc+="\x05""\x01" # USAGE_PAGE (Generic Desktop)
desc+="\x09""\x06" # USAGE (Keyboard)
desc+="\xA1""\x01" # COLLECTION (Application)

desc+="\x85""\x01" # REPORT_ID (1)

    #             Bitmapped Modifiers

desc+="\x05""\x07" # USAGE_PAGE (Key Codes)
desc+="\x19""\xE0" # USAGE_MINIMUM (224)
desc+="\x29""\xE7" # USAGE_MAXIMUM (231)
desc+="\x15""\x00" # LOGICAL_MINIMUM (0)
desc+="\x25""\x01" # LOGICAL_MAXIMUM (1)
desc+="\x75""\x01" # REPORT_SIZE (1)
desc+="\x95""\x08" # REPORT_COUNT (8)
desc+="\x81""\x02" # INPUT (Data Variable Absolute)

    #             Reserved

desc+="\x95""\x01" # REPORT_COUNT (1)
desc+="\x75""\x08" # REPORT_SIZE (8)
desc+="\x81""\x01" # INPUT (Constant)

    #             LEDs

desc+="\x05""\x08" # USAGE_PAGE (LED)
desc+="\x19""\x01" # USAGE_MINIMUM (1)
desc+="\x29""\x05" # USAGE_MAXIMUM (5)
desc+="\x95""\x05" # REPORT_COUNT (5)
desc+="\x75""\x01" # REPORT_SIZE (1)
desc+="\x91""\x02" # OUTPUT (Data Variable Absolute)

    #             Padding

desc+="\x95""\x01" # REPORT_COUNT (1)
desc+="\x75""\x03" # REPORT_SIZE (3)
desc+="\x91""\x03" # OUTPUT (Constant)

    #             Keyboard Keys

desc+="\x15""\x00" # LOGICAL_MINIMUM (0)
desc+="\x25""\xFF" # LOGICAL_MAXIMUM (255)
desc+="\x05""\x07" # USAGE_PAGE (Key Codes)
desc+="\x19""\x00" # USAGE_MINIMUM (0)
desc+="\x29""\xFF" # USAGE_MAXIMUM (255)
desc+="\x95""\x0D" # REPORT_COUNT (13)
desc+="\x75""\x08" # REPORT_SIZE (8)
desc+="\x81""\x00" # INPUT (Data Array Absolute)

desc+="\xC0"       # END_COLLECTION

    #             Media Keys

desc+="\x05""\x0C" # USAGE_PAGE (Conumer)
desc+="\x09""\x01" # USAGE (Consumer Control)
desc+="\xA1""\x01" # Collection (Application)

desc+="\x85""\x02" #   Report ID (2)

desc+="\x05""\x0C" #   Usage Page (Consumer)
desc+="\x15""\x00" #   Logical Minimum (0)
desc+="\x25""\x01" #   Logical Maximum (1)
desc+="\x75""\x01" #   Report Size (1)

desc+="\x95""\x06" #   Report Count (6)
desc+="\x09""\xB5" #   Usage (Scan Next Track)
desc+="\x09""\xB6" #   Usage (Scan Previous Track)
desc+="\x09""\xB7" #   Usage (Stop)
desc+="\x09""\xB8" #   Usage (Eject)
desc+="\x09""\xCD" #   Usage (Play/Pause)
desc+="\x09""\xE2" #   Usage (Mute)
desc+="\x81""\x06" #   Input (DataVarRel)

desc+="\x95""\x02" #   Report Count (2)
desc+="\x09""\xE9" #   Usage (Volume Increment)
desc+="\x09""\xEA" #   Usage (Volume Decrement)
desc+="\x81""\x02" #   Input (DataVarAbsNo WrapLinearPreferred StateNo Null Position)

desc+="\xC0"       # END_COLLECTION

declare -A f_attrs_keyboard
f_attrs_keyboard["protocol"]="1"
f_attrs_keyboard["report_desc"]=$desc
f_attrs_keyboard["report_length"]="16"
f_attrs_keyboard["subclass"]="0"

BASE=/sys/kernel/config/usb_gadget/
GADGET=$BASE/g9

mkdir -p $GADGET
cd $GADGET

for key in "${!g_attrs[@]}"; do
	echo -ne ${g_attrs[$key]} > $key
done

GADGET_STRING=$GADGET/strings/0x409

mkdir $GADGET_STRING
cd $GADGET_STRING

for key in "${!g_strs[@]}"; do
	echo -ne ${g_strs[$key]} > $key
done

FUNCTION_BASE=$GADGET/functions
FUNCTION=$FUNCTION_BASE/hid.usb0
CONFIG_BASE=$GADGET/configs
CONFIG=$CONFIG_BASE/config.1
CONFIG_STRING=$CONFIG/strings/0x409

mkdir -p $FUNCTION
cd $FUNCTION

for key in "${!f_attrs_keyboard[@]}"; do
	echo -ne ${f_attrs_keyboard[$key]} > $key
done

mkdir -p $CONFIG_STRING
cd $CONFIG_STRING

for key in "${!c_strs[@]}"; do
	echo -ne ${c_strs[$key]} > $key
done

ln -s $FUNCTION $CONFIG/keyboard
