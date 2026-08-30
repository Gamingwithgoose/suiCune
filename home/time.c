#include "../constants.h"
#include "time.h"
#include "sram.h"
#include "mobile.h"
#include "../engine/rtc/rtc.h"
#include "../util/rtc.h"

//  Functions relating to the timer interrupt and the real-time-clock.

void Timer(void){
    // PUSH_AF;
    // LDH_A_addr(hMobile);
    // AND_A_A;
    // IF_Z goto not_mobile;
#if FEATURE_MOBILE
    if(hram.hMobile) {
        // CALL(aMobileTimer);
        MobileTimer();
    }
#endif // FEATURE_MOBILE

// not_mobile:
    // POP_AF;
    // RET;
}

void UpdateTime(void) {
    GetClock();
    FixDays();
    FixTime();
    GetTimeOfDay();
}

//  store clock data in hRTCDayHi-hRTCSeconds
void GetClock(void){
    struct NativeRTCClock clock;
    if(!ReadNativeRTCClock(&clock))
        return;

    hram.hRTCSeconds = clock.seconds;
    hram.hRTCMinutes = clock.minutes;
    hram.hRTCHours = clock.hours;
    hram.hRTCDayLo = (uint8_t)clock.day;
    hram.hRTCDayHi = (uint8_t)(clock.day >> 8);
}

//  fix day count
uint8_t FixDays(void){
//  fix day count
//  mod by 140
    uint8_t result;
//  check if day count > 255 (bit 8 set)
    // LDH_A_addr(hRTCDayHi);  // DH
    // BIT_A(0);
    // IF_Z goto daylo;
    if(bit_test(hram.hRTCDayHi, 0)) {
    //  reset dh (bit 8)
        // RES_A(0);
        // LDH_addr_A(hRTCDayHi);
        bit_reset(hram.hRTCDayHi, 0);

    //  mod 140
    //  mod twice since bit 8 (DH) was set
        // LDH_A_addr(hRTCDayLo);
        uint8_t a = hram.hRTCDayLo;

        // uint8_t carry;
        // do {
        // modh:
            // SUB_A(140);
            // a = SubCarry8(a, 140, 0, &carry);
            // IF_NC goto modh;
        // } while(!carry);
        // do {
        // modl:
            // SUB_A(140);
            // IF_NC goto modl;
            // a = SubCarry8(a, 140, 0, &carry);
            // ADD_A(140);
        // } while(!carry);
        // a += 140;

    //  update dl
        // LDH_addr_A(hRTCDayLo);
        hram.hRTCDayLo = (a | 0x100) % 140;

    //  flag for sRTCStatusFlags
        // LD_A(0b01000000);
        // goto set;
        result = 0b01000000;
    }
    else {
    // daylo:
        //  quit if fewer than 140 days have passed
        // LDH_A_addr(hRTCDayLo);
        // CP_A(140);
        // IF_C goto quit;
        if(hram.hRTCDayLo < 140) {
            return 0;
        }

    //  mod 140
        // mod:
            // SUB_A(140);
            // IF_NC goto mod;
        // ADD_A(140);

    //  update dl
        // LDH_addr_A(hRTCDayLo);
        hram.hRTCDayLo %= 140;

    //  flag for sRTCStatusFlags
        // LD_A(0b00100000);
        result = 0b00100000;
    }

// set:
    //  update clock with modded day value
    // PUSH_AF;
    // CALL(aSetClock);
    SetClock();
    // POP_AF;
    // SCF;
    // RET;
    return result;

// quit:
    // XOR_A_A;
    // RET;
}

//  add ingame time (set at newgame) to current time
//  store time in wCurDay, hHours, hMinutes, hSeconds
void FixTime(void){
//  second
    uint8_t carry = 0;
    uint8_t curr_sec = hram.hRTCSeconds;
    uint8_t start_sec = gPlayer.startSecond;
    uint8_t sec = curr_sec + start_sec;
    if(sec >= 60)
    {
        sec -= 60;
        carry = 1;
    }
    hram.hSeconds = sec;

//  minute
    // REG_F_C = 0;  // carry is set, so turn it off
    uint8_t curr_min = hram.hRTCMinutes;
    uint8_t start_min = gPlayer.startMinute;
    uint8_t min = curr_min + start_min + carry;
    carry = 0;
    if(min >= 60)
    {
        min -= 60;
        carry = 1;
    }
    hram.hMinutes = min;

//  hour
    // REG_F_C = 0;  // carry is set, so turn it off
    uint8_t curr_hr = hram.hRTCHours;
    uint8_t start_hr = gPlayer.startHour;
    uint8_t hr = curr_hr + start_hr + carry;
    carry = 0;
    if(hr >= 24)
    {
        hr -= 24;
        carry = 1;
    }
    hram.hHours = hr;

//  day
    // REG_F_C = 0;  // carry is set, so turn it off
    uint8_t curr_day = hram.hRTCDayLo;
    uint8_t start_day = gPlayer.startDay;
    uint8_t day = curr_day + start_day + carry;
    gPlayer.curDay = day;
}

void InitTimeOfDay(uint8_t hour, uint8_t min){
    // XOR_A_A;
    // LD_addr_A(wStringBuffer2);

    // LD_A(0);  // useless
    // LD_addr_A(wStringBuffer2 + 3);

    // JR(mInitTime);
    InitTime(0, hour, min, 0);
}

void InitDayOfWeek(uint8_t day){
    // CALL(aUpdateTime);
    UpdateTime();

    // LDH_A_addr(hHours);
    // LD_addr_A(wStringBuffer2 + 1);

    // LDH_A_addr(hMinutes);
    // LD_addr_A(wStringBuffer2 + 2);

    // LDH_A_addr(hSeconds);
    // LD_addr_A(wStringBuffer2 + 3);

    // JR(mInitTime);  // useless
    return InitTime(day, hram.hHours, hram.hMinutes, hram.hSeconds);
}

void InitTime(uint8_t days, uint8_t hours, uint8_t mins, uint8_t secs){
    // bank_push(BANK(av_InitTime));
    v_InitTime(days, hours, mins, secs);
    // bank_pop;
}

static void ClearClock_ClearhRTC() {
    hram.hRTCSeconds = 0;
    hram.hRTCMinutes = 0;
    hram.hRTCHours = 0;
    hram.hRTCDayLo = 0;
    hram.hRTCDayHi = 0;
}

void ClearClock(void){
    ClearClock_ClearhRTC();
    SetClock();
}

//  set clock data from hram
void SetClock(void){
    uint16_t day = hram.hRTCDayLo | ((uint16_t)(hram.hRTCDayHi & 1) << 8);
    SetNativeRTCClockDay(day);
}

void ClearRTCStatus(void){
    //  //  unreferenced
//  clear sRTCStatusFlags
    // XOR_A_A;
    // PUSH_AF;
    // LD_A(MBANK(asRTCStatusFlags));
    // CALL(aOpenSRAM);
    OpenSRAM(MBANK(asRTCStatusFlags));
    // POP_AF;
    // LD_addr_A(sRTCStatusFlags);
    gb_write(sRTCStatusFlags, 0);
    // CALL(aCloseSRAM);
    CloseSRAM();
    // RET;
}

//  append flags to sRTCStatusFlags
void RecordRTCStatus(uint8_t a){
    // LD_HL(sRTCStatusFlags);

    // PUSH_AF;
    // LD_A(BANK(sRTCStatusFlags));
    // CALL(aOpenSRAM);
    OpenSRAM(MBANK(asRTCStatusFlags));

    // POP_AF;
    // OR_A_hl;
    // LD_hl_A;
    gb_write(sRTCStatusFlags, gb_read(sRTCStatusFlags) | a);

    // CALL(aCloseSRAM);
    CloseSRAM();
}

//  check sRTCStatusFlags
uint8_t CheckRTCStatus(void){
    // LD_A(BANK(sRTCStatusFlags));
    // CALL(aOpenSRAM);
    OpenSRAM(MBANK(asRTCStatusFlags));

    // LD_A_addr(sRTCStatusFlags);
    uint8_t flags = gb_read(sRTCStatusFlags);
    
    // CALL(aCloseSRAM);
    CloseSRAM();

    // RET;
    return flags;
}
