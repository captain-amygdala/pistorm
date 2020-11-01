//
//  Gayle.h
//  Omega
//
//  Created by Matt Parsons on 06/03/2019.
//  Copyright © 2019 Matt Parsons. All rights reserved.
//

#ifndef Gayle_h
#define Gayle_h

#include <stdio.h>
#include <stdint.h>




uint8_t CheckIrq(void);
void InitGayle(void);
void writeGayleB(unsigned int address, unsigned value);
void writeGayle(unsigned int address, unsigned value);
void writeGayleL(unsigned int address, unsigned value);
uint8_t readGayleB(unsigned int address);
uint16_t readGayle(unsigned int address);
uint32_t readGayleL(unsigned int address);
#endif /* Gayle_h */
