#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include "rtc.h"

static unsigned char rtc_mystery_reg[3];
unsigned char ricoh_memory[0x0F];
unsigned char ricoh_alarm[0x0F];

void put_rtc_byte(uint32_t address_, uint8_t value_, uint8_t rtc_type) {
  uint32_t address = address_ & 0x3F;
  uint8_t value = (value_ & 0x0F);

  address >>= 2;

  //printf("Wrote byte %.2X.\n", address);

  if (rtc_type == RTC_TYPE_MSM) {
    switch(address) {
      case 0x0D:
        rtc_mystery_reg[address - 0x0D] = value & (0x01 | 0x08);
        break;
      case 0x0E:
      case 0x0F:
        rtc_mystery_reg[address - 0x0D] = value;
        break;
      default:
        return;
    }
  }
  else {
    int rtc_bank = (rtc_mystery_reg[0] & 0x03);
    if ((rtc_bank & 0x02) && address < 0x0D) {
      if (rtc_bank & 0x01) {
        // Low nibble of value -> high nibble in RTC memory
        ricoh_memory[address] &= 0x0F;
        ricoh_memory[address] |= ((value & 0x0F) << 4);
      }
      else {
        // Low nibble of value -> low nibble in RTC memory
        ricoh_memory[address] &= 0xF0;
        ricoh_memory[address] |= (value & 0x0F);
      }
      return;
    }
    else if ((rtc_bank & 0x01) && address < 0x0D) {
      // RTC alarm stuff, no idea what this is supposed to be for.
      switch(address) {
        case 0x00:
        case 0x01:
        case 0x09:
        case 0x0C:
          ricoh_alarm[address] = 0;
          break;
        case 0x03:
        case 0x06:
          ricoh_alarm[address] &= (value & (0x08 ^ 0xFF));
          break;
        case 0x05:
        case 0x08:
        case 0x0B:
          ricoh_alarm[address] = (value & (0x0C ^ 0xFF));
          break;
        case 0x0A:
          ricoh_alarm[address] = (value & (0x0E ^ 0xFF));
          break;
        default:
          ricoh_alarm[address] = value;
          break;
      }
      //printf("Write to Ricoh alarm @%.2X: %.2X -> %.2X\n", address, value, ricoh_alarm[address]);
      return;
    }
    else if (address >= 0x0D) {
      rtc_mystery_reg[address - 0x0D] = value;
      return;
    }
  }
}

uint8_t get_rtc_byte(uint32_t address_, uint8_t rtc_type) {
  uint32_t address = address_ & 0x3F;

	if ((address & 3) == 2 || (address & 3) == 0) {
    //printf("Garbage byte read.\n");
		return 0;
	}

  address >>= 2;
  time_t t;
  time(&t);
  struct tm *rtc_time = localtime(&t);

  if (rtc_type == RTC_TYPE_RICOH) {
    int rtc_bank = (rtc_mystery_reg[0] & 0x03);
    if ((rtc_bank & 0x02) && address < 0x0D) {
      // Get low/high nibble from memory (bank 2/3)
      return ((ricoh_memory[address] >> (rtc_bank & 0x01) ? 4 : 0) & 0x0F);
    }
    else if ((rtc_bank & 0x01) && address < 0x0D) {
      // Get byte from alarm
      return ricoh_alarm[address];
    }
  }

  //printf("Read byte %.2X.\n", address);

  switch (address) {
    case 0x00: // Seconds low?
      return rtc_time->tm_sec % 10;
    case 0x01: // Seconds high?
      return rtc_time->tm_sec / 10;
    case 0x02: // Minutes low?
      return rtc_time->tm_min % 10;
    case 0x03: // Minutes high?
      return rtc_time->tm_min / 10;
    case 0x04: // Hours low?
      return rtc_time->tm_hour % 10;
    case 0x05: // Hours high?
      if (rtc_type == RTC_TYPE_MSM) {
        if (rtc_mystery_reg[2] & 4) {
          return (((rtc_time->tm_hour % 12) / 10) | (rtc_time->tm_hour >= 12) ? 0x04 : 0x00);
        }
        else
          return rtc_time->tm_hour / 10;
      }
      else {
        if (ricoh_alarm[10] & 0x01) {
          return rtc_time->tm_hour / 10;
        }
        else {
          return (((rtc_time->tm_hour % 12) / 10) | (rtc_time->tm_hour >= 12) ? 0x02 : 0x00);
        }
        break;
      }
    case 0x06: // Day low?
      if (rtc_type == RTC_TYPE_MSM)
        return rtc_time->tm_mday % 10;
      else
        return rtc_time->tm_wday;
    case 0x07: // Day high?
      if (rtc_type == RTC_TYPE_MSM)
        return rtc_time->tm_mday / 10;
      else
        return rtc_time->tm_mday % 10;
    case 0x08: // Month low?
      if (rtc_type == RTC_TYPE_MSM)
        return (rtc_time->tm_mon + 1) % 10;
      else
        return rtc_time->tm_mday / 10;
    case 0x09: // Month high?
      if (rtc_type == RTC_TYPE_MSM)
        return (rtc_time->tm_mon + 1) / 10;
      else
        return (rtc_time->tm_mon + 1) % 10;
    case 0x0A: // Year low?
      if (rtc_type == RTC_TYPE_MSM)
        return rtc_time->tm_year % 10;
      else
        return (rtc_time->tm_mon + 1) / 10;
    case 0x0B: // Year high?
      if (rtc_type == RTC_TYPE_MSM)
        return rtc_time->tm_year / 10;
      else
        return rtc_time->tm_year % 10;
    case 0x0C: // Day of week?
      if (rtc_type == RTC_TYPE_MSM)
        return rtc_time->tm_wday;
      else
        return rtc_time->tm_year / 10;
    case 0x0D: // Mystery register D-F?
    case 0x0E:
    case 0x0F:
      if (rtc_type == RTC_TYPE_MSM) {
        return rtc_mystery_reg[address - 0x0D];
      }
      else {
        if (address == 0x0D) return rtc_mystery_reg[address - 0x0D];
        return 0;
      }
    default:
      break;
  }

  return 0x00;
}
