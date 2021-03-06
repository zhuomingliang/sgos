#include <stdio.h>
#include <string.h>
#include <api.h>
#include "hd.h"
#include "port.h"

typedef struct PARTITION_INFO{
	unsigned long start;	//其实扇区
	unsigned long sectors;	//扇区数
}PARTITION_INFO;

// 硬盘参数表
static HD_PARAMS hd_param[MAX_HD];
/* 记录硬盘分区信息
 * 其中 part[0]记录第一个硬盘的全部信息, part[1]是第一个硬盘第一个分区的信息。
 */
static PARTITION_INFO part[5*MAX_HD];


int lba_rw_sectors(t_8 dev, t_32 sec_start, t_32 sec_count, uchar *buf, int write)
{
	sec_start += part[dev].start;
//	printf("rwhd %x, %x, %x, 0x%x, %d\n",
//		dev, sec_start, sec_count, buf, write);
	/*采用LBA方式读写*/
	int drive;
	if( dev<5 )
		drive=0;
	else
		drive=1;
	outbyte(HD_CMD, hd_param[drive].controlByte);	/*输出控制字节*/
	outbyte(HD_DATA+2, sec_count);			/*设置扇区数*/
	outbyte(HD_DATA+3, sec_start);			/*lba低8位*/
	outbyte(HD_DATA+4, sec_start>>8);		/*lba次8位*/
	outbyte(HD_DATA+5, sec_start>>16);		/*lba中8位*/
	outbyte(HD_DATA+6,0xe0|(drive<<4)|(sec_start&0x0f000000)>>24);	/*驱动器号,lba高4位*/
	if(write){	/*写硬盘*/
		outbyte(HD_STATUS, 0x30);
	}else{		/*读硬盘*/
		outbyte(HD_STATUS, 0x20);
	}
	while(sec_count){
		while( ( inbyte(HD_STATUS) & 0xf ) != STAT_DRQ )
			SysSleepThread(1);
		if(write)
			WRITE_WORDS(HD_DATA, buf, 256);	/*写端口数据*/
		else
			READ_WORDS(HD_DATA, buf, 256);	/*读端口数据*/
		buf += 512;
		sec_count--;
	}
	return sec_count;
}

//从bios中读参数
static uchar readCMOS(uchar p)
{
	outbyte( 0x70, 0x80|p );
	return inbyte( 0x71 );
}

//向vfs注册设备
static void registerDevice( uint part )
{
	uint tid ;
	Message msg;
	while( !(tid= SmGetServiceThreadById( DeviceManagerId )) )
		SysSleepThread(200);
	msg.ThreadId = tid;
	msg.Command = Device_Register;
	msg.Arguments[0] = DEV_TYPE_HD;
	msg.Arguments[1] = SysGetCurrentThreadId();
	msg.Arguments[2] = part;
	strcpy( (char*)&msg.Arguments[3], "Partition" );
	Api_Send( &msg, 0 );
	Api_Receive( &msg, 3000 ); //wait for reply.
}

#define LPValue(m) ( *(size_t*)(m) )
//获取硬盘数据
int lba_init()
{
	int i, ret, sid = SysGetCurrentSpaceId();
	size_t p;
	uchar* boot = new uchar[512];
	memset( part, 0, sizeof(part) );
	p = (size_t)SysAllocateMemory( sid, 1<<20, MEMORY_ATTR_WRITE, ALLOC_VIRTUAL );
	if( p == 0 ){
		printf("[HD]Allocate failed.\n");
		return -ERR_NOMEM;
	}
	//请求内核映射前1MB内存
	ret = SysMapMemory( sid, p, 1<<20, 0, 0, MAP_ADDRESS );
	if( ret < 0 ){
		printf("[HD]lba_init failed to map memory: 0x%X\n", ret );
		delete[] boot;
		return ret;
	}
	//硬盘1数据
	memcpy(&hd_param[0], (void*)(p+R2L(LPValue(p+0x00000104))), sizeof(HD_PARAMS));
	memcpy(&hd_param[1], (void*)(p+R2L(LPValue(p+0x00000118))), sizeof(HD_PARAMS));
	//取消映射
	SysFreeMemory( sid, (void*)p );
	if( !readCMOS(0x12)&0xff )
	{
		printf("[HD]no AT hard disks.\n");
		delete[] boot;
		return -1;
	}

	for( i=0; i<MAX_HD; i++ )	//两个硬盘
	{
		if( hd_param[i].cyls==0 )
			continue;
		part[i*5].start = 0;	//起始扇区
		//硬盘总扇区数
		part[i*5].sectors = hd_param[i].heads * hd_param[i].SPT * hd_param[i].cyls;
		if(!hd_param[i].cyls) break;
		printf("[HD]hd_param(%d) Size: %d MB\n", i, part[i*5].sectors*512/1024/1024);
		//读分区表Partition Tables
		//Just for Real Hard Disk!!
		PARTITION* p;
		lba_rw_sectors( i*5, 0, 1, boot, 0 );
		if( boot[510]!=0x55||(uchar)boot[511]!=0xAA )
		{
			printf("[HD]bad partitiion table.\n");
			break;
		}
		p = (PARTITION*)(boot + 0x1BE);
		if( p->bootable )
		{
			int j;
			for(j=1; j<5; j++,p++ )
			{
				part[i*5+j].start = p->skipSectors;	//相对扇区起始位置
				part[i*5+j].sectors = p->allSectors;	//总扇区数
				if( p->allSectors){
					printf("[HD]Partition%d: start_sector:0x%X   total_sectors:0x%X\n", i*5+j, 
						part[i*5+j].start, part[i*5+j].sectors);
					registerDevice( i*5+j );
				}
			}
		}
	}
	delete[] boot;
	return 0;
}

