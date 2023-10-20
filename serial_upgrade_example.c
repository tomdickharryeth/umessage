#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <sched.h>
#include <limits.h>
#include <errno.h>

#include "serial_upgrade_example.h"
#include "xmodem.h"
#include "upgrade.h"

#define DATABITS	8
#define PARITY		'N'
#define STOPBITS	1


static int get_args(int argc, const char *argv[], struct_cmd_args *args)
{
	int i = 0, index = 0;
	for (i = 1; i < argc; i++) //argv[0] is "./a.out"
	{
		if (!strcmp(argv[i], "-d")) //device 
		{
			strncpy(args->device, argv[i+1], LEN_DEVICE_PATH);
			i ++;
			index ++;
		}
		else if (!strcmp(argv[i], "-b")) //baudrate 
		{
			args->baudrate = atoi(argv[i+1]);
			i ++;
			index ++;
		}
		else if (!strcmp(argv[i], "-reset")) //auto_reset 
		{
			args->auto_reset = 1;
		}
		else if (!strcmp(argv[i], "-m")) //upgrade_mdoe 
		{
			memset(args->upgrade_mode, 0, sizeof(args->upgrade_mode));
			strncpy(args->upgrade_mode, argv[i+1], LEN_MODE_PATH);
			i ++;
			index ++;
		}
		else if (!strcmp(argv[i], "-p"))
		{
			strncpy(args->pkg_path, argv[i+1], LEN_PKG_PATH);
			i ++;
			index ++;
		}
		else if (!strcmp(argv[i], "-w")) //work_baudrate
		{
			args->work_baudrate = atoi(argv[i+1]);
			i ++;
		}
	}
	//check args
	if (index != 4)
		return ERROR_GET_ARGS;
	else 
		return SUCCESS;
}

static unsigned int checkSum32(unsigned int * pbuf, unsigned int len)
{
	unsigned int i, sumValue = 0;
	len >>= 2;	//len /=4
	for(i=0;i<len;i++)
	{
		sumValue += *pbuf++;
	}
	return sumValue;
}

//return read file len
int get_file_content(char *file_name, char *file_buff)
{
	struct_package_header header;
	int fd = 0, len_header = 0, ret = 0;
	unsigned int crc_value = 0;
	struct stat file_stat;

	memset(&header, 0, sizeof(header));

	//open file 
	fd = open(file_name, O_RDONLY);
	if (fd < 0)
	{
		printf("fail to open package file, please check.\n");
		ret = ERROR_OPEN;
		goto FAIL_0;
	}

	ret = fstat(fd, &file_stat);
	if (ret != 0)
	{
		printf("fail to fstat package file, please check.\n");
		ret = ERROR_STAT;
		goto FAIL_0;
	}

	ret = read(fd, file_buff, file_stat.st_size);
	if (file_stat.st_size != ret) //pkg not enough
	{
		printf("fail to read package body, please check.\n");
		ret = ERROR_READ_BODY;
		goto FAIL_0;
	}

FAIL_0: 	
	close(fd); 
	return ret;
}

#define UC_PKG_COUNT	10
static int check_package_file(const char *file_buff, int pkgsize)
{
	struct_package_header *header;
	int fd = 0, len_header = 0, ret = 0;
	unsigned int crc_value = 0, sum32_value = 0, offset = 0;
	char *buff = NULL, i = 0;

	len_header = sizeof(struct_package_header);

	//because firmware is multi pkg put together, so need check all package
	for (i = 0; i < UC_PKG_COUNT; i++) 
	{
		if (offset >= pkgsize) //TODO
			return SUCCESS;

		//get image header
		header = (struct_package_header *)(file_buff + offset);

		//caculate crc and check 
		crc_value = crc16_ccitt((void *)header, sizeof(struct_package_header) - sizeof(int));
		if (crc_value != header->header_crc)
		{
			printf("check crc error, please check. \n");
			return ERROR_CRC;
		}
	
		sum32_value = checkSum32((unsigned int *)((void *)header + len_header), header->firmware_size);
		if (sum32_value != header->firmware_sum32)
		{
			printf("check sum32 error, please check. \n");
			return ERROR_SUM32;
		}
		offset += len_header + header->firmware_size;
	}

	ret = SUCCESS;
	return ret;
}

static int check_package(char *loader_path, char *firmware_path, char *savebuff)
{
	int ret = 0;

	ret = get_file_content(loader_path, savebuff); //SUCCESS return buff len
	if (ret < SUCCESS)
		return ret;
	ret = check_package_file(savebuff, ret);
	if (ret != SUCCESS)
		return ret;

	ret = get_file_content(firmware_path, savebuff); //SUCCESS return buff len
	if (ret < SUCCESS)
		return ret;
	ret = check_package_file(savebuff, ret);
	if (ret != SUCCESS)
		return ret;
	
	return SUCCESS;
}

static int get_file_header(char *pkg_path, struct_package_header *pheader)
{
	int len_header = 0, fd = 0;	

	//open file 
	fd = open(pkg_path, O_RDONLY);
	if (fd < 0)
	{
		printf("fail to open package file, please check.\n");
		return ERROR_OPEN;
	}
	
	//read image header
	len_header = sizeof(struct_package_header);
	if (len_header != read(fd, pheader, len_header))	//not read enough size 
	{
		printf("fail to read package header, please check.\n");
		return ERROR_READ_HEADER;
	}
	return SUCCESS;
}

static int serial_set_opt(int fd, int nBits, char nEvent, int nStop) //just like : 115200, 8, n, 1
{
	struct termios newtio, oldtio;
	if (tcgetattr(fd, &oldtio) != 0) 
	{ 
		perror("SetupSerial 1");
		return ERROR_TCGETATTR_ERROR;
	}

	memcpy(&newtio, &oldtio, sizeof(oldtio)); 

/*
	 * 8bit Data,no partity,1 stop bit...
 */
	newtio.c_cflag &= ~PARENB;//ÎÞÆæÅ¼Ð£Ñé
	newtio.c_cflag &= ~CSTOPB;//Í£Ö¹Î»£¬1Î»
	newtio.c_cflag &= ~CSIZE; //Êý¾ÝÎ»µÄÎ»ÑÚÂë
	newtio.c_cflag |= CS8;    //Êý¾ÝÎ»£¬8Î»
	cfmakeraw(&newtio);
	if((tcsetattr(fd,TCSANOW,&newtio))!=0)
	{
		perror("com set error");
		return ERROR_TCSETATTR_ERROR;
	}
	tcflush(fd,TCIFLUSH);

	return 0;
}

static int serial_set_baudrate(int fd, int speed)
{
	struct termios newtio, oldtio;
	if (tcgetattr(fd, &oldtio) != 0) 
	{ 
		perror("SetupSerial 1");
		return ERROR_TCGETATTR_ERROR;
	}
	memcpy(&newtio, &oldtio, sizeof(oldtio)); 
	switch(speed)
	{
		case 115200:
			cfsetispeed(&newtio, B115200);
			cfsetospeed(&newtio, B115200);
			break;
		case 230400:
			cfsetispeed(&newtio, B230400);
			cfsetospeed(&newtio, B230400);
			break;
		case 460800:
			cfsetispeed(&newtio, B460800);
			cfsetospeed(&newtio, B460800);
			break;
		case 921600:
			cfsetispeed(&newtio, B921600);
			cfsetospeed(&newtio, B921600);
			break;
		default:
			cfsetispeed(&newtio, B115200);
			cfsetospeed(&newtio, B115200);
			break;
	}

	if((tcsetattr(fd,TCSANOW,&newtio))!=0)
	{
		perror("com set error");
		return ERROR_TCSETATTR_ERROR;
	}
	return 0;
}


static int serial_get_data(struct_xmodem_t *xm, char *data, int size, int timeout) //timeout * 1ms
{
	int ret = 0;
	fd_set fds;
	struct timeval tv;
	int fd = 0;

	fd = *(int *)(xm->serial_handler);

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	ret = select(fd+1, &fds, NULL, NULL, &tv);
	if (ret > 0)
	{
		if (FD_ISSET(fd, &fds))
			ret = read(fd, data, size);
		else 
			ret = 0;
	}

	return ret; 
}

static int serial_put_data(struct_xmodem_t *xm, char *data, int size)
{
	int ret = 0;
	int data_size = 0, send_size = 0;
	int show_progress = 0;

	data_size = (xm->file_size / BLOCKSIZE + 1)*BLOCKSIZE; //after align size
	if (xm->frame_number > 0) //enter xmodem, send data
	{
		send_size = xm->frame_number * BLOCKSIZE;
		show_progress = (long double)send_size / (long double)(data_size) * 100;
		printf("\rdata_size: %8d==========>send_size:%8d========>progress: %d%%", data_size, send_size, show_progress);
		fflush(stdout);
	}
	ret = write(*(int *)(xm->serial_handler), data, size); //fd is a gloabl var(TTY device)
	if (ret != size) {//serial Error
		printf("ret =%x\n", ret); 
		return ERROR_SERIAL_WRITE;
	}
	return SUCCESS;
}

static int serial_set_baudrate_ext(struct_xmodem_t *xm, int baudrate)
{
	serial_set_baudrate(*(int *)(xm->serial_handler), baudrate);

	return SUCCESS;
}

static int serial_init(char *device, int nBits, char nEvent, int nStop)
{
	int fd_serial = 0, ret = 0;

	fd_serial = open(device, O_RDWR);
	if (fd_serial < 0)
	{
		printf("fail to open serial\n");
		return ERROR_OPEN_SERIAL;
	}
	ret = serial_set_opt(fd_serial, nBits, nEvent, nStop); //set serial args
	if (ret < SUCCESS)
	{
		printf("fail to serial_set_opt, please check serial.\n");
		exit(-1);
	}
	return fd_serial;
}

static int read_frame_data(struct_xmodem_t *xm, char *buffer, int frame_size)
{
	int ret = 0;

	ret = fread(buffer, 1, frame_size, xm->file_handler);
	return ret;
}

int main(int argc, const char *argv[])
{
	int ret = 0, file_length = 0;
	int fd_serial = 0; 
	char *file_buff = NULL;
	struct_cmd_args args;
	struct_package_header pheader;
	struct_xmodem_t xm;
	struct_upgrade_t ug;
	char loader_path[256];		
	FILE *fp = NULL;

	int prio = 50;
	if(prio) {
		struct sched_param sch;
		sch.sched_priority = prio;
		if(sched_setscheduler(0, SCHED_RR, &sch) != 0)
			printf("warning: sched_setscheduler failed: %s\n", strerror(errno));
		else
			printf("sched_setscheduler with prio %d\n", prio);
	}

	memset(&args, 0, sizeof(struct_cmd_args));
	memset(&pheader, 0, sizeof(struct_package_header));
	memset(&xm, 0, sizeof(struct_xmodem_t));
	memset(&ug, 0, sizeof(struct_upgrade_t));

	if (argc == 1) //print help
	{
		printf("usage: ./winconfig -d device_path -b download_firmware_baudrate  -w work_baudrate -reset -p package_path -m upgrade_mode\n"); 
		printf("example: ./winconfig -d /dev/ttyUSB3 -b 460800 -w 460800 -reset -p image.pkg -m 0x08\n");
		printf("please check serial port line suport 921600 !");
		exit(0);
	}

	//get args
	ret = get_args(argc, argv, &args);
	if (ret != SUCCESS)
	{
		printf("args error, please check.\n");
		exit(0);
	}
	
	//input args follow down
	printf("\n---------upgrade info----------\n");
	printf("# reset: 	%s\n", args.auto_reset==1?"auto":"manual");
	printf("# device: 	%s\n", args.device);
	printf("# upgrade_mode: 	%s\n", args.upgrade_mode);
	printf("# baudrate: 	%d\n", args.baudrate);
	printf("# pkg_path: 	%s\n", args.pkg_path);
	printf("-------------------------------\n");

	// file_buff = malloc(MAX_IMAGE_SIZE);
	// DIR *dir;
	// struct dirent *ptr;
	// if(NULL == (dir=opendir("./loader")))
	// {
	// 	printf("open loader path error!\r\n");
	// 	exit(1);
	// }
	// char szLoaderWord[128]="";
	// if (args.baudrate == 115200)
	// {
	// 	strcpy(szLoaderWord, "N4_bootloader_update_115200.pkg");
	// }
	// else if (args.baudrate == 460800)
	// {
	// 	strcpy(szLoaderWord, "N4_bootloader_update_460800.pkg");
	// }
	// else if (args.baudrate == 921600)
	// {
	// 	strcpy(szLoaderWord, "N4_bootloader_update_921600.pkg");
	// }
	// else 
	// {
	// 	printf("[ERROR]: Please set baudrate 115200 or 460800 or 921600\n");
	// 	exit(0);
	// }
	// memset(loader_path,0,sizeof(loader_path));
	// while(NULL != (ptr=readdir(dir)))
	// {
	// 	if(!strcmp(ptr->d_name,szLoaderWord))
	// 	{
	// 		snprintf(loader_path, sizeof(loader_path), "./loader/%s",ptr->d_name);	//modify safety function
	// 		break;
	// 	}
	// }
	// if (0 == strlen(loader_path))
	// {
	// 	printf("[ERROR]: Get loader path error!\r\n");
	// 	exit(1);
	// }
	// printf("loader path is %s\r\n",loader_path);
	// //check file legal
	// ret = check_package(loader_path, args.pkg_path, file_buff);
	// if (ret != SUCCESS) 
	// {
	// 	printf("check package file error\n");
	// 	free(file_buff);
	// 	exit(-1);
	// }
	// free(file_buff);

	//init uart 
	fd_serial = serial_init(args.device, DATABITS, PARITY, STOPBITS);
	if (fd_serial < 0)
	{
		printf("fail to serial init, please check.\n");
		exit(-1);
	}
	
	//init xmodem struct
	xm.send_data = serial_put_data;
	xm.recv_data = serial_get_data;
	xm.serial_handler = &fd_serial;
	xm.get_file_data = read_frame_data;

	// //init upgrade struct 
	// memcpy(ug.upgrade_mode, args.upgrade_mode, sizeof(args.upgrade_mode));
	// ug.download_baudrate = args.baudrate;
	// ug.work_baudrate = args.work_baudrate;
	// ug.set_baudrate = serial_set_baudrate_ext;
	// ug.auto_reset = args.auto_reset;
	
	// fp = fopen(loader_path, "r");
	// if (fp == NULL)
	// {
	// 	perror("fail to open laoder,");
	// 	exit(-1);
	// }
	// ug.loader_file_handler = fp;
	// fseek(fp, 0L, SEEK_END);
	// ug.loader_file_size = ftell(fp);
	// printf("file size = %d\n", ug.loader_file_size);
	// fseek(fp, 0L, SEEK_SET);

	// fp = fopen(args.pkg_path, "r");
	// if (fp == NULL)
	// {
	// 	perror("fail to open firmware,");
	// 	exit(-1);
	// }
	// ug.firmware_file_handler = fp;
	// fseek(fp, 0, SEEK_END);
	// ug.firmware_file_size = ftell(fp);
	// fseek(fp, 0, SEEK_SET);

	ret = serial_upgrade_pkg(&xm, &ug);
	if (ret == SUCCESS)
	{
		printf("upgrade success.\n");
	}
	else
	{
		printf("upgrade fail. ret=%d\n", ret);
	}
	
	// fclose(ug.loader_file_handler);
	// fclose(ug.firmware_file_handler);
	file_buff = NULL;

	return ret;
}
