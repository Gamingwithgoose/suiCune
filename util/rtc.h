#pragma once

struct NativeRTCClock {
    uint16_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

int CheckRTCSupport(void);
int ReadNativeRTCClock(struct NativeRTCClock* dest);
int SetNativeRTCClockDay(uint16_t day);
void SetStartTimeToSystemTime(void);
void LoadRTCStartTime(void);
void RTCSyncWithSystemTime(void);
void RTCInitTimeWithSystemTime(void);
uint8_t RTCGetCurrentWeekday(void);
