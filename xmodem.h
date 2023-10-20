#ifndef XMODEM_H
#define XMODEM_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ACK 0x06
#define NAK 0x15
#define STX 0x02
#define EOT 0x04
#define CAN 0x18
#define BLOCKSIZE 1024 
#define XMODEM_ADD_SIZE	5

#define ERROR_XMODEM_TIMEOUT -201
#ifndef ERROR_SERIAL_WRITE
#define ERROR_SERIAL_WRITE	-202
#endif 
#define ERROR_XMODEM_ILLEGAL_CHAR -203
#define ERROR_XMODEM_CANCEL	-204

#define  TIMEOUT	10000	//timeout is 10s

unsigned short crc16_ccitt(const unsigned char *buf, int len);

typedef struct _xmodem_data{
	int frame_number; 	//xmodem send frame number
	void *serial_handler;	//xmodem send serial handler 
	void *file_handler; 	//xmodem send file handler (for get_file_data)
	unsigned int file_size;	//xmodme send file size
	int (*recv_data)(struct _xmodem_data *xm, char *data, int size, int timeout); //get data, timeout(100ms)
	int (*send_data)(struct _xmodem_data *xm, char *data, int size);	//put data 
	int (*get_file_data)(struct _xmodem_data *xm, char *data_buff, int fram_size);	//from file_handler get a frame data
}struct_xmodem_t;

int xmodem_send(struct_xmodem_t *xm);

#endif


