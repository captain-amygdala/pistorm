//
//  Gayle.c
//  Omega
//
//  Created by Matt Parsons on 06/03/2019.
//  Copyright © 2019 Matt Parsons. All rights reserved.
//

//Write Byte to Gayle Space 0xda9000 (0x0000c3)
//Read Byte From Gayle Space 0xda9000
//Read Byte From Gayle Space 0xdaa000


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "Gayle.h"
#include "ide.h"

#define CLOCKBASE 0xDC0000

#define GSTATUS 0xda201c 
#define GCLOW   0xda2010
#define GDH	0xda2018



//Write Byte to Gayle Space 0xda2018 (0x000000)
//Read Byte From Gayle Space 0xda2010
//Read Byte From Gayle Space 0xda201c
//Write Byte to Gayle Space 0xdaa000 (0x00002c)
//Write Byte to Gayle Space 0xda8000 (0x000000)
//Write Byte to Gayle Space 0xda2018 (0x000000)
//Write Byte to Gayle Space 0xda2010 (0x000012)



/*
Write Byte to Gayle Space 0xda3018 (0x000000)

Read Byte from Gayle Ident 0xde1000 (0x000004)
Write ide_dev_head: 0x0000a0
Write Byte to Gayle Space 0xda2018 (0x0000a0)
Write ide_cyl_low: 0x000012
Write Byte to Gayle Space 0xda2010 (0x000012)
Write ide_cyl_low: 0x000034
Write Byte to Gayle Space 0xda2010 (0x000034)
Write Byte to Gayle Space 0xda3018 (0x000000)
Write ide_status_r: 0x000010
Write Byte to Gayle Space 0xda201c (0x000010)
*/


int counter;
static struct ide_controller *ide0;
int fd;

void InitGayle(void){
   ide0 = ide_allocate("cf");
   fd = open("hd0.img", O_RDWR);
   if (fd == -1){
        printf("HDD Image hd0.image failed open\n");
   }else{
        ide_attach(ide0, 0, fd);
        ide_reset_begin(ide0);
        printf("HDD Image hd0.image attached\n");
   }
}

uint8_t CheckIrq(void){
uint8_t irq;

	irq = ide0->drive->intrq;
//	if (irq==0)
//	printf("IDE IRQ: 0\n");

return irq;
}

void writeGayleB(unsigned int address, unsigned int value){


    if (address == GSTATUS) {
	ide_write8(ide0, ide_status_r, value);
//	printf("Write ide_status_r: 0x%06x IRQ:0x%06x\n",value, ide0->drive->intrq);
	return;
	}

   if (address == GDH) {
        ide_write8(ide0, ide_dev_head, value);
//	printf("Write ide_dev_head: 0x%06x\n",value);
        return;
	}

    if (address == GCLOW) {
        ide_write8(ide0, ide_cyl_low, value);
//        printf("Write ide_cyl_low: 0x%06x\n",value);
	return;
        }


    if (address == 0xDE1000){
	 counter = 0;
//	 printf("Write Byte to Gayle Ident 0x%06x (0x%06x)\n",address,value);
	return;
	}

    if (address == 0xda9000){
	return;
	}

      if (address == 0xda8000){
        return;
        }

      if (address == 0xdaa000){
        return;
        }

      if (address == 0xdab000){
        return;
        }





    printf("Write Byte to Gayle Space 0x%06x (0x%06x)\n",address,value);
}

void writeGayle(unsigned int address, unsigned int value){
//    printf("Write to Gayle Space 0x%06x (0x%06x)\n",address,value);
}

void writeGayleL(unsigned int address, unsigned int value){
//    printf("Write Long to Gayle Space 0x%06x (0x%06x)\n",address,value);
}

uint8_t readGayleB(unsigned int address){

	if (address == GSTATUS) {
//	printf("Read ide_status_r\n");
        return ide_read8(ide0, ide_status_r);
        }

	if (address == GCLOW) {
//	printf("Read ide_cyl_low\n");
        return ide_read8(ide0, ide_cyl_low);
        }

 	if (address == GDH) {
//	printf("Read ide_dev_head\n");
        return ide_read8(ide0, ide_dev_head);
        }


    if (address == 0xDE1000){
        counter++;
//	printf("Read Byte from Gayle Ident 0x%06x (0x%06x)\n",address,counter);

	if (counter == 3){ 
//		printf("Gayle Ident cycle\n");
		return 0xFF;//7F; to enable gayle
	}else{
		return 0xFF;
	}

        }

      if (address == 0xda9000){
        return 0;
        }

      if (address == 0xdaa000){
        return 0;
	}

    printf("Read Byte From Gayle Space 0x%06x\n",address);
    return 0xFF;
}

uint16_t readGayle(unsigned int address){
    printf("Read From Gayle Space 0x%06x\n",address);
    return 0x8000;
}

uint32_t readGayleL(unsigned int address){
    printf("Read Long From Gayle Space 0x%06x\n",address);
    return 0x8000;
}
