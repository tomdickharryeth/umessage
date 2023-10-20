#ifndef UPGRADE_H
#define UPGRADE_H 

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>

#include "xmodem.h"

#define ERROR_AUTO_RESET_SERIAL		-301
#define ERROR_AUTO_RESET_TIMEOUT	-302
#define ERROR_ENTER_DOWNLOAD_MODEM_SERIAL	-303
#define ERROR_ENTER_DOWNLOAD_MODEM_TIMEOUT	-304
#define ERROR_WAIT_UPGRADE_FINISH 	-305
#define ERROR_ENTER_DOWNLOAD_FIRMWARE_TIMEOUT	-306
#define ERROR_CHECK_RETURN_TIMEOUT	-307
#define ERROR_CHECK_RETURN_EXCEPT	-308
#define ERROR_CHECK_UPGRADE_MODE	-309
#define ERROR_ONOCOY	-310


#define BOARD_WORK_BAUDRATE	115200
#define BOARD_MAX_RETRY_TIME	8192 //serial buff is 2M, so max_value = 2M / buff_size

#ifndef SUCCESS
#define SUCCESS		0
#endif

#define AUTO_RESET_TIMEOUT	1000	
#define SEND_CMD_RETRY		10
#define ENTER_DOWNLOAD_MODEM_TIMEOUT	30000
#define WAIT_UPGRADE_TIMEOUT	100000 

#define MAX_IMAGE_SIZE		10000000 	//10M

#ifndef LINUX_DEBUG
#define LINUX_DEBUG
#endif

#define ROM_BAUDRATE	115200
#define CLEAN_LINE		"\r\r\n"
#define UPDATE_IMAGE	"2\r\n"
#define RESET_BOARD		"6\r\n"

typedef struct _upgrade_data{
	char upgrade_mode[8];	//M!T or T@T@ upgrade mode
	int auto_reset; 		//auto reset sign
	int download_baudrate;	//download firmware baudrate 
	int work_baudrate;	//communicate with boardcard for reset command

	void *loader_file_handler;		//bootloader handler (for xmodem read a frame)
	unsigned int loader_file_size;	//bootloader size
	void *firmware_file_handler;	//firmware file handler (for xmodem read a frame)
	unsigned int firmware_file_size;	//firmware size
	int (*set_baudrate)(struct _xmodem_data *xm, int baudrate);	//set serial baudrate
}struct_upgrade_t;

int get_file_content(char *filename, char *file_buff);
int serial_upgrade_pkg(struct_xmodem_t *xm, struct_upgrade_t *ug);

#endif
