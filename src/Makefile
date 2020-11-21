all: mkusbhid event2usbhid

mkusbhid: mkusbhid.c gadget-hid.c gadget-hid.h 
	$(CC) -o mkusbhid mkusbhid.c gadget-hid.c -L../lib/libusbgx/lib -I../lib/libusbgx/include/ -lusbgx

event2usbhid: main.c keyboard.c mouse.c
	$(CC) -o event2usbhid main.c keyboard.c mouse.c
