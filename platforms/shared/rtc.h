// SPDX-License-Identifier: MIT

void put_rtc_byte(uint32_t address_, uint8_t value, uint8_t rtc_type);
uint8_t get_rtc_byte(uint32_t address_, uint8_t rtc_type);

enum rtc_types {
    RTC_TYPE_MSM,
    RTC_TYPE_RICOH,
    RTC_TYPE_NONE,
};
