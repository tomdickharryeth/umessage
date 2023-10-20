#ifndef CHECK_ARGS_H
#define CHECK_ARGS_H

#include <stdio.h>
#include <linux/limits.h>

#define LEN_DEVICE_PATH		PATH_MAX
#define LEN_MODE_PATH	8
#define LEN_BAUDRATE	32
#define LEN_PKG_PATH		PATH_MAX

#define SUCCESS	0
#define ERROR_OPEN	-1
#define ERROR_READ_HEADER	-2
#define ERROR_READ_BODY	-3
#define ERROR_CRC	-4
#define ERROR_MALLOC	-5
#define ERROR_STAT		-6
#define ERROR_SUM32		-7
#define ERROR_GET_ARGS		-8

#define ERROR_OPEN_SERIAL	-101
#define ERROR_BAUDRATE_NOT_SUPPORT	-102
#define ERROR_TCGETATTR_ERROR	-103
#define ERROR_TCSETATTR_ERROR	-104

typedef struct {
	char device[LEN_DEVICE_PATH];
	char upgrade_mode[LEN_MODE_PATH];
	int dual_backup;
	int baudrate;
	int work_baudrate;
	int auto_reset;
	char pkg_path[LEN_PKG_PATH];
} struct_cmd_args;

//N4 header
typedef struct {
	unsigned int reserved1;
	unsigned int reserved2;
	unsigned int reserved3;
	unsigned int firmware_size;
	unsigned int reserved4;
	unsigned int reserved5;
	unsigned int firmware_sum32;
	unsigned short int header_crc;
	unsigned short int reserved6;
} struct_package_header;


int get_file_content(char *file_name, char *file_buff);

#endif 

