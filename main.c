#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <linux/usb/tmc.h>
#include <err.h>
#include <getopt.h>
#include <time.h>
#ifndef NO_LIBNOTIFY
#include "libnotify/notify.h"
#endif

/*
scsd (scope screen dumper)

A small and simple Linux only screen dumper for Rigol MSO5000 series (and possibly other) oscilloscopes connected via USB. Saves the current screen content as BMP or PNG or the output of the integrated protocol decoder as CSV. 

Uses the Kernel-provided USBTMC interface. Needs only libc and (optionally) libnotify. Avoids (sometimes bloated) third-party abstraction layers.

(c) 2024 by kittennbfive - https://github.com/kittennbfive/

AGPLv3+ and NO WARRANTY!

version 0.2

Please read the fine manual.
*/

#define SZ_BUF_DEVICENAME 20
#define SZ_BUF_FOLDER 100
#define SZ_BUF_FILENAME 50
#define ERR_MSG_BUF_SIZE 100
#define ERR_NOTIF_TIME_SEC 60
#define DEFAULT_DEVICE "/dev/usbtmc0"

#define SZ_BUF_OK_MESSAGE (SZ_BUF_DEVICENAME+SZ_BUF_FILENAME+27)
#define SZ_BUF_FULL_PATH (SZ_BUF_FOLDER+SZ_BUF_FILENAME+1+1)

#ifndef NO_LIBNOTIFY
static bool no_notif=false;
#endif

typedef enum
{
	GET_BMP=0,
	GET_PNG,
	GET_CSV
} get_type_t;

static void cleanup(void)
{
#ifndef NO_LIBNOTIFY
	notify_uninit();
#endif
}

static void errmsg_exit(char const * const msg, ...)
{
	char buf[ERR_MSG_BUF_SIZE+1];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, ERR_MSG_BUF_SIZE, msg, ap);
	buf[ERR_MSG_BUF_SIZE]='\0';
	va_end(ap);
#ifndef NO_LIBNOTIFY
	if(!no_notif)
	{
		NotifyNotification * notification=notify_notification_new("scsd: Error", buf, "dialog-error");
		notify_notification_set_timeout(notification, ERR_NOTIF_TIME_SEC*1000);
		notify_notification_show(notification, NULL);
		g_object_unref(G_OBJECT(notification));
	}
#endif
	err(1, buf);
}

static void errxmsg_exit(char const * const msg, ...)
{
	char buf[ERR_MSG_BUF_SIZE+1];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, ERR_MSG_BUF_SIZE, msg, ap);
	buf[ERR_MSG_BUF_SIZE]='\0';
	va_end(ap);
#ifndef NO_LIBNOTIFY
	if(!no_notif)
	{
		NotifyNotification * notification=notify_notification_new("scsd: Error", buf, "dialog-error");
		notify_notification_set_timeout(notification, ERR_NOTIF_TIME_SEC*1000);
		notify_notification_show(notification, NULL);
		g_object_unref(G_OBJECT(notification));
	}
#endif
	errx(1, buf);
}

static void print_usage_version_and_exit(void)
{
	printf("This is scsd (scope screen dumper) version 0.2\nThis tool is made for Rigol MSO5000 series and for Linux only.\n(c) 2024 by kittennbfive - AGPLv3+ and NO WARRANTY!\n\nPlease read the fine manual.\n\n");
	printf("usage: scsd [--device $usbtmc_device] [--folder $path] [--filename $name] [--png] [--csv [--decoder $nb]] [--no-notif]\n\n");
	exit(0);
}

int main(int argc, char ** argv)
{
#ifndef NO_LIBNOTIFY
	notify_init("scsd");
#endif

	atexit(&cleanup);
	
	const struct option optiontable[]=
	{
		{ "device",		required_argument,	NULL,	0 },
		{ "folder",		required_argument,	NULL,	1 },
		{ "filename",	required_argument,	NULL,	2 },
		{ "png",		no_argument,		NULL,	3 },
		{ "csv",		no_argument,		NULL,	4 },
		{ "decoder",	required_argument,	NULL,	5 }, //use with --csv, default is 1
#ifndef NO_LIBNOTIFY
		{ "no-notif",	no_argument,		NULL,	6 },
#endif
		
		{ "version",	no_argument,		NULL, 	100 },
		{ "help",		no_argument,		NULL, 	100 },
		{ "usage",		no_argument,		NULL, 	100 },
		
		{ NULL, 0, NULL, 0 }
	};
	
	char device[SZ_BUF_DEVICENAME+1]={'\0'};
	char folder[SZ_BUF_FOLDER+1]={'\0'};
	char filename[SZ_BUF_FILENAME+1]={'\0'};
	get_type_t get_type=GET_BMP;
	uint_fast8_t decoder=1;
	
	int optionindex;
	int opt;
	
	while((opt=getopt_long(argc, argv, "", optiontable, &optionindex))!=-1)
	{
		switch(opt)
		{
			case '?': print_usage_version_and_exit(); break;
			case 0: strncpy(device, optarg, SZ_BUF_DEVICENAME); device[SZ_BUF_DEVICENAME]='\0'; break;
			case 1: strncpy(folder, optarg, SZ_BUF_FOLDER); folder[SZ_BUF_FOLDER]='\0'; break;
			case 2: strncpy(filename, optarg, SZ_BUF_FILENAME); filename[SZ_BUF_FILENAME]='\0'; break;
			case 3: get_type=GET_PNG; break;
			case 4: get_type=GET_CSV; break;
			case 5: decoder=atoi(optarg); break;
#ifndef NO_LIBNOTIFY
			case 6: no_notif=true; break;
#endif
			
			case 100: print_usage_version_and_exit(); break;
		}
	}
	
	if(decoder<1 || decoder>4)
		errxmsg_exit("invalid value %u for decoder", decoder);
	
	if(strlen(device)==0) //use default device if none provided
	{
		printf("no device specified, using default %s\n", DEFAULT_DEVICE);
		strncpy(device, DEFAULT_DEVICE, SZ_BUF_DEVICENAME); device[SZ_BUF_DEVICENAME]='\0';
	}
		
	char full_path[SZ_BUF_FULL_PATH+1];
	
	if(strlen(filename)==0) //make default file name if none provided
	{
		//variable "device" contains a full path (and so '/' which are not allowed inside a *file*name), we only want the last part
		uint_fast16_t pos;
		for(pos=strlen(device); pos && device[pos]!='/'; pos--);
		if((pos+1)<strlen(device))
			pos++;
		time_t t=time(NULL);
		struct tm * tm=localtime(&t);
		char * ext[3]={"bmp", "png", "csv"};
		snprintf(filename, SZ_BUF_FILENAME, "%s_%02d.%02d_%02d%02d%02d.%s", &device[pos], tm->tm_mday, tm->tm_mon+1, tm->tm_hour, tm->tm_min, tm->tm_sec, ext[(uint)get_type]); //don't use ':' inside filenames to maintain compatibility with FAT32
		filename[SZ_BUF_FILENAME]='\0';
	}
	
	if(strlen(folder))
		snprintf(full_path, SZ_BUF_FULL_PATH, "%s/%s", folder, filename);
	else
		strncpy(full_path, filename, SZ_BUF_FULL_PATH);
	full_path[SZ_BUF_FULL_PATH]='\0';

	char cmd[25];
	switch(get_type)
	{
		case GET_BMP: strcpy(cmd, ":DISP:DATA?"); break; //returns 1,8MB (for MSO5000 series) BMP
		case GET_PNG: strcpy(cmd, ":DISP:SNAP? PNG"); break; //UNDOCUMENTED! returns PNG (much smaller obviously)
		case GET_CSV: sprintf(cmd, ":BUS%u:DATA?", decoder); break;
	}
	
	int fd=open(device, O_RDWR);
	if(fd<0)
		errmsg_exit("opening device %s failed", device);
	
	FILE * out=fopen(full_path, "wb");
	if(out==NULL)
		errmsg_exit("creating output file %s failed", full_path);
	
	ssize_t bytes_written=write(fd, cmd, strlen(cmd));
	if(bytes_written<0)
		errmsg_exit("write failed");
	
	uint8_t buf[4096];
	ssize_t bytes_read;
	bytes_read=read(fd, buf, 4096);
	
	if(bytes_read<0 || bytes_read<12) //USBTMC-header is 12 bytes
		errmsg_exit("first read failed");
		
	if(buf[0]!='#')
		errxmsg_exit("invalid header in response");
	uint_fast8_t nb_digits=buf[1]-'0';
	if(nb_digits==0)
		errxmsg_exit("invalid number of digits in response");
	
	char size[10];
	memcpy(size, &buf[2], nb_digits*sizeof(char));
	size[2+nb_digits+1]='\0';
	
	uint_fast32_t sz_payload=atoi(size);
	if(sz_payload==0) //just in case atoi() gets accidentally fed with non-numerical characters...
		errxmsg_exit("invalid payload size in response");
	
	fwrite(&buf[11], sizeof(uint8_t), bytes_read-11, out);
	
	ssize_t bytes_total;
	if(sz_payload<4096)
		bytes_total=0;
	else
		bytes_total=sz_payload-(4096-12); //4096 bytes already read including 12 bytes USBTMC-header
	
	while(bytes_total>0)
	{
		bytes_read=read(fd, buf, MIN(bytes_total, 4096));
		if(bytes_read<0)
			err(1, "read in loop failed");
		
		bytes_total-=bytes_read;
		
		if(bytes_total==0 && bytes_read>0) //last read?
			fwrite(buf, sizeof(uint8_t), bytes_read-1, out); //ignore USBTMC-footer byte
		else
			fwrite(buf, sizeof(uint8_t), bytes_read, out);
	}
	
	close(fd);
	
	fclose(out);
	
	char message[SZ_BUF_OK_MESSAGE+1];
	snprintf(message, SZ_BUF_OK_MESSAGE, "%s from %s saved as %s", (get_type!=GET_CSV)?"screenshot":"data", device, filename);
	message[SZ_BUF_OK_MESSAGE]='\0';
	
	printf("%s\n", message);
	
#ifndef NO_LIBNOTIFY
	if(!no_notif)
	{
		NotifyNotification * notification=notify_notification_new("scsd", message, "dialog-information");
		notify_notification_show(notification, NULL);
		g_object_unref(G_OBJECT(notification));
	}
#endif
	
	return 0;
}
