#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory.h>


//! eventファイル→USB HIDのキーコード変換表
unsigned short linux_to_usb[] = {
	0x00, 0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
	0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x2B, 0x14, 0x1A, 0x08, 0x15,
	0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30, 0x28, 0xe0,
	0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33,
	0x34, 0x35, 0xE1, 0x32, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11,
	0x10, 0x36, 0x37, 0x38, 0xE5, 0x55, 0xE2, 0x2C, 0x39, 0x3A,
	0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x53,
	0x47, 0x5F, 0x60, 0x61, 0x56, 0x5C, 0x5D, 0x5E, 0x57, 0x59,
	0x5A, 0x5B, 0x62, 0x63, 0x00, 0x00, 0x00, 0x44, 0x45, 0x87,
	0x6A, 0x00, 0x8A, 0x88, 0x8B, 0x00, 0x58, 0xE4, 0x54, 0x46,
	0xE6, 0x00, 0x4A, 0x52, 0x4B, 0x5C, 0x4F, 0x4D, 0x51, 0x4E,
	0x49, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x89, 0xE3, 0xE7, 0x65
};


/**
 * @fn
 * /dev/input/eventNからイベントを読み込む
 *
 * @param 読み込んだイベントを書き込むポインタ
 */
void getKeyboardEvent(struct input_event *event) {

	// 有効なイベントが取れるまでブロックする
	while(1) {
		read(0, event, sizeof(*event));

		// キーイベント以外は無視する
		if (event->type != EV_KEY) {
			continue;
		}

		// キーリピートは無視する
		if (event->value == 2) {
			continue;
		}

		break;
	}
}


/**
 * @fn
 * キーを離した時の処理
 *
 * @param hid_code 離したUSB HIDのキーコード
 * @param pressed_keys 押しているキーの配列
 *
 * @return 指定したキーが押されていた:1、押されていなかった:0
 */
int releaseKey(unsigned short hid_code, unsigned short *pressed_keys){
	int x;

	for(x = 0; x < 14; x++){
		if(pressed_keys[x] == hid_code){
			pressed_keys[x] = 0;
			return 1;
		}
	}

	return 0;
}


/**
 * @fn
 * キーを押したときの処理
 *
 * @param hid_code 押したUSB HIDのキーコード
 * @param pressed_keys 押しているキーの配列
 */
void pressKey(unsigned short hid_code, unsigned short *pressed_keys){
	int x;

	for(x = 0; x < 14; x++){
		if(pressed_keys[x] == 0){
			pressed_keys[x] = hid_code;
			return;
		}
	}
}


/**
 * @fn
 * libcompositeにキーボードイベントを書き込む
 * 
 * @param modifiers CTRLとかSHIFTとかの押され具合
 * @param pressed_keys 押しているキーの配列
 */
void sendKeyboardHIDReport(char modifiers, unsigned short *pressed_keys){
	int x;
	unsigned char buf[16];

	memset(buf, 0, sizeof(buf));

	buf[0] = 1; // report ID
	buf[1] = modifiers;
	buf[2] = 0; // padding

	for(x = 3; x < 16; x++){
		buf[x] = pressed_keys[x - 3];
	}

	write(1, buf, sizeof(buf));
}


/**
 * @fn
 * キーボード変換処理
 */
void keyboard() {
	unsigned short pressed_keys[14];

	memset(pressed_keys, 0, sizeof(pressed_keys));

	while(1) {
		struct input_event event;

		getKeyboardEvent(&event);

		unsigned short usb_keycode = linux_to_usb[event.code];

		if (event.value == 1) {
			pressKey(usb_keycode, pressed_keys);
		} else {
			releaseKey(usb_keycode, pressed_keys);
		}

		sendKeyboardHIDReport(0, pressed_keys);
	}
}
