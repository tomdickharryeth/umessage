#include "upgrade.h"
#include "xmodem.h"
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <string.h>


#ifdef LINUX_DEBUG
#include "time.h"
#endif

static int check_serial_return(struct_xmodem_t *xm, char *keyword, int keyword_size, int check_time) //check time (1ms)
{
	char data_buff[256] = {0};
	int i = 0, ret = 0, j = 0;

	while (++i < BOARD_MAX_RETRY_TIME)
	{
		ret = xm->recv_data(xm, data_buff + keyword_size, sizeof(data_buff) - keyword_size, check_time); //timeout 1s
		if (ret < 1) //recv no data. no such case
		{
			return ERROR_CHECK_RETURN_EXCEPT;
		}
		ret += keyword_size;
		data_buff[ret] = 0; //for data_buff is a recv string
		for (j = 0; j < ret; j++)
		{
			if (data_buff[j] == 0)
			{
				data_buff[j] = 0xff; //filter 0	
			}
		}
		if (NULL != strstr(data_buff, keyword)) //first return ok
		{
			return SUCCESS;
		}
		strcpy(data_buff, data_buff+ret-keyword_size);
	}

	return ERROR_CHECK_RETURN_TIMEOUT;
}

static int do_auto_reset(struct_xmodem_t *xm, struct_upgrade_t *ug)
{
	int i = 0, ret = 0, j = 0, index = 0;
	char cmd_closelog_buff[] = "unlog\r\n";
	char cmd_reset_buff[] = "reset\r\n";
	char cmd_check[] = "$command,unlog,response: OK*";
	char cmd_rst_check[] = "$command,reset,response: OK*";
	char data_buff[256] = {0};

#ifdef LINUX_DEBUG
	printf("--> check boardcard runing state: ");
#endif
	//check boardcard normal and close boardcard log
	while (1) //board restart time 
	{
		ret = xm->recv_data(xm, data_buff, sizeof(data_buff), 10); 
		if (ret > 0)	//flush recv buff
		{
			continue; 
		}
		for(i=0;i!=5;++i)
		{
			ret = write(*(int *)(xm->serial_handler), cmd_closelog_buff, strlen(cmd_closelog_buff));
			if (ret != strlen(cmd_closelog_buff))
				return ERROR_AUTO_RESET_SERIAL;
			sleep(1);
			printf("-----send unlog\r\n");
		}
		ret = check_serial_return(xm, cmd_check, strlen(cmd_check), 1000);
		if (ret == SUCCESS)
		{
			//printf("check ok\n");
			break;
		}
		if (++index > SEND_CMD_RETRY)
		{
			//printf("timeout\n");
			return ERROR_AUTO_RESET_TIMEOUT;
		}
#ifdef LINUX_DEBUG
		printf("=");
		fflush(stdout);
#endif
	}

#ifdef LINUX_DEBUG
	printf("\n--> reset boardcard: ");
#endif
	i = 0;
	while (1) //board restart time 
	{

#ifdef LINUX_DEBUG
		printf("=");
		fflush(stdout);
#endif

		ret = xm->send_data(xm, cmd_reset_buff, strlen(cmd_reset_buff));		//send reset cmd
		if (ret != SUCCESS)
			return ERROR_AUTO_RESET_SERIAL;

		ret = check_serial_return(xm, cmd_rst_check, strlen(cmd_rst_check), 1000);
		if (ret == SUCCESS)
		{
			break;
		}
		if (i++ > SEND_CMD_RETRY)
			return ERROR_AUTO_RESET_TIMEOUT;
	}
#ifdef LINUX_DEBUG
	printf("\n");
#endif

	return SUCCESS;
}
static int do_manual_reset(void)
{
#ifdef LINUX_DEBUG
	printf("--> please reset your boardcard <--\n");
#endif
	return SUCCESS;
}


static int enter_download_modem(struct_xmodem_t *xm, unsigned char mode)
{
	int len, tret, ret = 0, cnt = 0, i = 0;
	char cmd_enter_buff[20] = {0};
	char data_buff[128] = {0}, save_buff[32] = {0};
	char cmd_check[20] = {0};
	char start_flag = 1, loc=0;
	struct timespec tp_goal, tp_currrent;

	if(mode == 1)
	{
		//strcpy(cmd_enter_buff, "M!TM!TM!T");
		memset(cmd_enter_buff, 0x08, sizeof(cmd_enter_buff)-1);
		cmd_enter_buff[sizeof(cmd_enter_buff)-1] = '\0';
		strcpy(cmd_check, "YC");
		loc = strlen("YC");
	}
	else if(mode == 2)
	{
		strcpy(cmd_enter_buff, "M!TM!TM!T");
		//memset(cmd_enter_buff, 0x08, sizeof(cmd_enter_buff)-1);
		cmd_enter_buff[sizeof(cmd_enter_buff)-1] = '\0';
		strcpy(cmd_check, "YC");
		loc = strlen("YC");
	}
	else
	{
		strcpy(cmd_enter_buff, "T@T@T@T@T@T@");
		cmd_enter_buff[sizeof(cmd_enter_buff)-1] = '\0';
		strcpy(cmd_check, "boot>");
		loc = strlen("boot>");
	}

	memset(data_buff, 0, sizeof(data_buff));

#ifdef LINUX_DEBUG
	printf("--> Enter download mode: ");
	fflush(stdout);

	clock_gettime(CLOCK_MONOTONIC, &tp_goal);
#endif
	while (start_flag)
	{
		//tcflush(*(int *)(xm->serial_handler),TCIOFLUSH);
		usleep(100);
		tret = xm->send_data(xm, cmd_enter_buff, strlen(cmd_enter_buff)); //send update cmd
		if (tret != SUCCESS)
			return ERROR_ENTER_DOWNLOAD_MODEM_SERIAL;
		memset(&data_buff[loc], 0, sizeof(data_buff)-loc);
		do{
			ret = xm->recv_data(xm, &data_buff[loc], sizeof(data_buff)-loc-1, 3);//rx timeout is 3 ms
			if (ret > 0)
			{
				len = (ret>(sizeof(data_buff)-loc-1)) ? sizeof(data_buff)-loc-1:ret;
				for (i = 0 ; i < len; i++)
				{
					if (data_buff[i] == 0)
						data_buff[i] = 0xFF;
				}
				data_buff[sizeof(data_buff) - 1 ] = 0;
				if (NULL != strstr(data_buff, cmd_check)) //first return ok
				{
					printf("---	check ok ---\n");
					start_flag = 0;
					break;
				} else {
					printf("fail ret %d, data_buff %s\n", ret, data_buff);
					memcpy(data_buff, data_buff+ret, loc);
				}
			}
		}while(ret > 0);
		cnt++; //1us
#ifdef LINUX_DEBUG
		if ((cnt % 3000) == 0)
		{
			printf("xxx");
			fflush(stdout);
		}
#endif

		if ((cnt > ENTER_DOWNLOAD_MODEM_TIMEOUT)) {//
			data_buff[31] = 0;
			for (i=0;i<32;i++) {
				printf(" %02x", data_buff[i]);	
				if(i%16 == 0) printf("\n");
			}
			return ERROR_ENTER_DOWNLOAD_MODEM_TIMEOUT;
		}
		
	}

END:
#ifdef LINUX_DEBUG
	printf("succeed!\n");
#endif
	return SUCCESS;
}

static int do_reset(struct_xmodem_t *xm, struct_upgrade_t *ug)
{
	int ret = SUCCESS;

	if (ug->auto_reset == 1) // 1 is auto_reset and 0 is manual reset
	{
		ug->set_baudrate(xm, ug->work_baudrate);
		ret = do_auto_reset(xm, ug);
		if (ret != SUCCESS)
		{
			printf("[ERROR]: auto reset fail !!!!!!!! \r\n");
			return ret;
		}
	}
	else 
	{
		//ug->set_baudrate(xm, ug->download_baudrate);
		do_manual_reset();
	}
	return ret;
}

//#define TEST_UPGRADE_LOADER		//test upgrade mode
static int communicate_with_loader(struct_xmodem_t *xm, char *behavior)
{
	int ret = 0, i = 0, offset = 0, cnt = 0;
	char buff[256] = {0};
	do
	{
		ret = xm->recv_data(xm, buff, sizeof(buff), 100); //flush serial rx
	} while (ret > 0);

	while (1)
	{
		ret = xm->recv_data(xm, buff, sizeof(buff), 100);
		if (ret < 0)
			continue;
		for (i = 0; i < ret; i++)
		{
			if (buff[i] == '>')
			{
				ret = xm->send_data(xm, behavior, 3); //behavior size is fixed: 3
				return SUCCESS;
			}
		}
		cnt += 1;
		
		if (cnt > 2000) 
			return ERROR_ENTER_DOWNLOAD_FIRMWARE_TIMEOUT;
	}
	return SUCCESS;
}



static int write_cmd(struct_xmodem_t *xm, char cmd[])
{
	printf(cmd);
	int ret = 0;
	ret = write(*(int *)(xm->serial_handler), cmd, strlen(cmd));
	if (ret != strlen(cmd))
	{
		printf("onocoy cmd failed\r\n");
		return ERROR_ONOCOY;
	}
	sleep(1);
	return SUCCESS;
}


/**
 * 
mode base time 60 2 2.5

CONFIG SIGNALGROUP 2
Config ppp enable E6-HAS
Config ppp datum wgs84
config RTCMB1CB2a enable

# ONLY CHANGE IF YOU WANT TO IMPROVE THE BAUDRATE
# config com1 921600

rtcm1006 30
rtcm1033 30
rtcm1077 1
rtcm1087 1
rtcm1097 1
rtcm1107 1
rtcm1117 1
rtcm1127 1
rtcm1137 1
Saveconfig

 * 
 * 
*/
static int set_onocoy_config(struct_xmodem_t *xm)
{
	int i = 0, ret = 0, j = 0, index = 0;

	char cmd_1[] = "mode base time 60 2 2.5\r\n";
	char cmd_2[] = "CONFIG SIGNALGROUP 2\r\n";
	char cmd_3[] = "Config ppp enable E6-HAS\r\n";
	char cmd_4[] = "Config ppp datum wgs84\r\n";
	char cmd_5[] = "config RTCMB1CB2a enable\r\n";
	char cmd_6[] = "rtcm1006 30\r\n";
	char cmd_7[] = "rtcm1033 30\r\n";
	char cmd_8[] = "rtcm1077 1\r\n";
	char cmd_9[] = "rtcm1087 1\r\n";
	char cmd_10[] = "rtcm1097 1\r\n";
	char cmd_11[] = "rtcm1107 1\r\n";
	char cmd_12[] = "rtcm1117 1\r\n";
	char cmd_13[] = "rtcm1127 1\r\n";
	char cmd_14[] = "rtcm1137 1\r\n";
	char cmd_15[] = "Saveconfig\r\n";

	char data_buff[256] = {0};

	printf("set_onocoy_config ");

	ret = xm->recv_data(xm, data_buff, sizeof(data_buff), 10); 

	write_cmd(xm, cmd_1);
	write_cmd(xm, cmd_2);
	write_cmd(xm, cmd_3);
	write_cmd(xm, cmd_4);
	write_cmd(xm, cmd_5);
	write_cmd(xm, cmd_6);
	write_cmd(xm, cmd_7);
	write_cmd(xm, cmd_8);
	write_cmd(xm, cmd_9);
	write_cmd(xm, cmd_10);
	write_cmd(xm, cmd_11);
	write_cmd(xm, cmd_12);
	write_cmd(xm, cmd_13);
	write_cmd(xm, cmd_14);
	write_cmd(xm, cmd_15);
	
	
	// ret = check_serial_return(xm, cmd_check, strlen(cmd_check), 1000);
	// if (ret == SUCCESS)
	// {
	// 	//printf("check ok\n");
	// 	break;
	// }
	// if (++index > SEND_CMD_RETRY)
	// {
	// 	//printf("timeout\n");
	// 	return ERROR_AUTO_RESET_TIMEOUT;
	// }
	return SUCCESS;
}



int serial_upgrade_pkg(struct_xmodem_t *xm, struct_upgrade_t *ug)
{
	int ret = 0;

// 	//check reset style (auto/manual) and reset
// 	ret = do_reset(xm, ug);
// 	if (ret != SUCCESS)
// 		return ret;

// 	//enter down mode
// 	if(strcmp(ug->upgrade_mode, "0x08") == 0)
// 	{
// 		ug->set_baudrate(xm, ug->download_baudrate);
// 		ret = enter_download_modem(xm, 1);
		
// 	}
// 	else if(strcmp(ug->upgrade_mode, "M!T") == 0)
// 	{
// 		ug->set_baudrate(xm, ug->download_baudrate);
// 		ret = enter_download_modem(xm, 2);
		
// 	}
// 	else if(strcmp(ug->upgrade_mode, "T@T@") == 0)
// 	{
// 		ug->set_baudrate(xm, ug->download_baudrate);
// 		ret = enter_download_modem(xm, 3);
		
// 		if (ret != SUCCESS)
// 			return ret;
// 		else
// 		{
// 			ret = communicate_with_loader(xm, CLEAN_LINE);
// 			if (ret != SUCCESS)
// 			{
// #ifdef LINUX_DEBUG
// 				printf("enter_download_firmware Error, please check.\n");
// #endif
// 				return ret;
// 			}
// 			goto DOWNLOAD_FW;
// 		}
// 	}
// 	else
// 	{
// 		printf("[EEOR]: upgrade mode error, please set 0x08/M!T or T@T@. \r\n");
// 		ret = ERROR_CHECK_UPGRADE_MODE;
// 	}
// 	if (ret != SUCCESS)
// 		return ret;

// #ifdef LINUX_DEBUG
// 	printf("--> start xmodem to send loader:\n");
// #endif
// 	xm->file_handler = ug->loader_file_handler;
// 	xm->file_size = ug->loader_file_size;
// 	ret = xmodem_send(xm);
// 	if (ret != SUCCESS)
// 		return ret;
// 	xm->frame_number = 0;
// #ifdef LINUX_DEBUG
// 	printf("\n");
// 	printf("download loader ok\n");
// #endif

// DOWNLOAD_FW:
// 	//ug->set_baudrate(xm, ug->download_baudrate); //move to 280 lines
// 	//enter download firmware mode
// 	ret = communicate_with_loader(xm, UPDATE_IMAGE);

// 	if (ret != SUCCESS)
// 	{
// #ifdef LINUX_DEBUG
// 		printf("enter_download_firmware Error, please check.\n");
// #endif
// 		return ret;
// 	}
	
// #ifdef LINUX_DEBUG
// 	printf("download firmware: \n");
// #endif

// 	xm->file_handler = ug->firmware_file_handler;
// 	xm->file_size = ug->firmware_file_size;
// 	ret = xmodem_send(xm);
// 	if (ret != SUCCESS)
// 		return ret;
// 	xm->frame_number = 0;

	//update onocoy commands

	set_onocoy_config(xm);


// 	//reset boardcard
// #ifdef LINUX_DEBUG
// 	printf("\nreset boardcard:\n");
// #endif


// 	ret = communicate_with_loader(xm, RESET_BOARD);

// 	//Description: double backup mode print menu tardiness, lead to reset fail, but return success.
// 	if (ret != SUCCESS)
// 		return ret;

	return SUCCESS;
}

