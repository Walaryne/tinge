#include <stdio.h>
#include <hidapi/hidapi.h>
#include <string.h>
#include <zlib.h>
#include <unistd.h>
#include <glob.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

volatile sig_atomic_t running = 1;

#define FADETIME 5000

void interrupt_handler() {
	running = 0;
}

int sendcolour(const unsigned char *colours, hid_device *handle) {
	unsigned long crc = crc32(0L, Z_NULL, 0);
	unsigned char crcbuf[80] = {0xa2};
	unsigned char buf[78] = {0x11, 0xc4, 0x0, 0x03, 0x0, 0x0,
						  0x0, 0x0, colours[0], colours[1], colours[2]};

	//CRC buffer is slightly larger, since we need the a2 control header, which is not in our buffer.
	//Set that value manually, copy the rest of our normal buffer after it.
	memcpy(crcbuf + 1, buf, sizeof buf);

	//Calculate the packet CRC32; controller will not accept settings if this is calculated incorrectly.
	crc = crc32(crc, (const unsigned char *) crcbuf, 75);

	//Set the last bytes of the buffer to the calculated CRC32.
	*(unsigned long *)(buf + 74) = crc;

	//Write the HID packet out
	return hid_write(handle, buf, 78);
}

void fade(unsigned char *from, const unsigned char *to, hid_device *handle) {
	for(int i = 0; i < 3; ++i) {
		if(from[i] < to[i]) {
			while(from[i] < to[i]) {
				++from[i];
				sendcolour(from, handle);
				usleep(FADETIME);
			}
		} else {
			while(from[i] > to[i]) {
				--from[i];
				sendcolour(from, handle);
				usleep(FADETIME);
			}
		}
	}
}

unsigned char getbatterylevel(glob_t *globbuf) {
	char buf[5];
	int fd;

	fd = open(globbuf->gl_pathv[0], O_RDONLY);

	read(fd, buf, 5);

	close(fd);

	return strtol(buf, NULL, 10);
}

int main() {
	int res;
	unsigned char batterylevel = 101;
	hid_device *handle;
	glob_t globbuf;
	unsigned char curstate[3] = {0, 0, 0};
	unsigned char states[5][3] = {
			{0, 255, 0},
			{255, 255, 0},
			{255, 103, 0},
			{255, 0, 0},
			{0, 0, 0}
	};

	//Daemonize
	//daemon(1, 0);

	//Initialize HIDAPI
	res = hid_init();
	//Open up the handle for our DS4, using VID and PID.
	handle = hid_open(0x54c, 0x9cc, NULL);
	//Register signal handler
	signal(SIGINT, interrupt_handler);
	//Glob the batterystatus path
	glob("/sys/class/power_supply/sony_controller_battery*/capacity", 0, NULL, &globbuf);

	sendcolour(states[4], handle);
	sleep(1);
	fade(curstate, states[0], handle);
	sleep(1);

	while(running) {
		if(batterylevel != getbatterylevel(&globbuf) || batterylevel == 10) {
			batterylevel = getbatterylevel(&globbuf);
			if(batterylevel <= 100 && batterylevel > 70) {
				fade(curstate, states[0], handle);
			} else if(batterylevel <= 70 && batterylevel > 50) {
				fade(curstate, states[1], handle);
			} else if(batterylevel <= 50 && batterylevel > 30) {
				fade(curstate, states[2], handle);
			} else if(batterylevel <= 30 && batterylevel > 10) {
				fade(curstate, states[3], handle);
			} else if(batterylevel <= 10 && batterylevel > 0) {
				fade(curstate, states[4], handle);
				sleep(1);
				fade(curstate, states[3], handle);
			}
		}
		sleep(10);
	}

	//Close out the handle and exit, all finished.
	hid_close(handle);
	hid_exit();
	globfree(&globbuf);

	return res;
}
