#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include "log.h"


static log_level_t s_loglevel = LOG_INFO;
static FILE *s_logdest = NULL;
static FILE *s_runtimeLog = NULL;
static clock_t s_runtimeStart;
static uint64_t s_runtimeSequence;
static uint64_t s_runtimeFrame;
static uint32_t s_runtimeBattleTurn;
static char s_runtimeBattlePhase[48];
static char s_runtimeBoundary[128];
static bool s_runtimeFatal;
static bool s_runtimeClosed;

struct Logger {
    log_level_t level;
    FILE* dest;
};

void log_set_level(log_level_t level) {
    s_loglevel = level;
}

void log_set_dest(FILE* f) {
    s_logdest = f;
}

static double RuntimeElapsedSeconds(void) {
    if(s_runtimeStart == (clock_t)-1)
        return 0.0;
    return (double)(clock() - s_runtimeStart) / (double)CLOCKS_PER_SEC;
}

bool log_runtime_begin(const char* applicationName) {
#ifdef _WIN32
    if(_mkdir("logs") != 0 && errno != EEXIST) {
#else
    if(mkdir("logs", 0777) != 0 && errno != EEXIST) {
#endif
        fprintf(stderr, "Unable to create runtime log directory: %s\n", strerror(errno));
        return false;
    }
    s_runtimeLog = fopen("logs/suiCune_runtime.log", "w");
    if(s_runtimeLog == NULL) {
        fprintf(stderr, "Unable to open runtime log: %s\n", strerror(errno));
        return false;
    }
    s_runtimeStart = clock();
    s_runtimeSequence = 0;
    s_runtimeFrame = 0;
    s_runtimeBattleTurn = 0;
    s_runtimeBattlePhase[0] = '\0';
    s_runtimeBoundary[0] = '\0';
    s_runtimeFatal = false;
    s_runtimeClosed = false;
    log_set_dest(s_runtimeLog);
    fprintf(s_runtimeLog, "suiCune runtime diagnostic log\n");
    fprintf(s_runtimeLog, "application=%s platform=%s architecture=%zu-bit build=%s\n",
        applicationName == NULL ? "unknown" : applicationName,
#ifdef _WIN32
        "Windows",
#else
        "unknown",
#endif
        sizeof(void*) * 8,
#if DEBUG
        "debug"
#else
        "release"
#endif
    );
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    if(local != NULL)
        fprintf(s_runtimeLog, "startup=%04d-%02d-%02d %02d:%02d:%02d\n",
            local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
            local->tm_hour, local->tm_min, local->tm_sec);
    fprintf(s_runtimeLog, "commit=unknown\n");
    fflush(s_runtimeLog);
    return true;
}

void log_runtime_event(const char* category, const char* fmt, ...) {
    if(s_runtimeLog == NULL || s_runtimeClosed)
        return;
    fprintf(s_runtimeLog, "[%06llu][%8.3fs][frame=%llu][%s][turn=%u phase=%s] ",
        (unsigned long long)++s_runtimeSequence, RuntimeElapsedSeconds(),
        (unsigned long long)s_runtimeFrame, category == NULL ? "RUNTIME" : category,
        s_runtimeBattleTurn, s_runtimeBattlePhase[0] == '\0' ? "none" : s_runtimeBattlePhase);
    va_list args;
    va_start(args, fmt);
    vfprintf(s_runtimeLog, fmt, args);
    va_end(args);
    fputc('\n', s_runtimeLog);
    fflush(s_runtimeLog);
}

void log_runtime_mark_fatal(const char* reason) {
    s_runtimeFatal = true;
    log_runtime_event("FATAL", "%s", reason == NULL ? "unspecified fatal termination" : reason);
}

void log_runtime_frame_advance(void) {
    s_runtimeFrame++;
}

void log_runtime_set_battle_context(uint32_t turn, const char* phase) {
    s_runtimeBattleTurn = turn;
    if(phase == NULL)
        s_runtimeBattlePhase[0] = '\0';
    else {
        strncpy(s_runtimeBattlePhase, phase, sizeof(s_runtimeBattlePhase) - 1);
        s_runtimeBattlePhase[sizeof(s_runtimeBattlePhase) - 1] = '\0';
    }
}

void log_runtime_set_boundary(const char* boundary) {
    if(boundary == NULL)
        s_runtimeBoundary[0] = '\0';
    else {
        strncpy(s_runtimeBoundary, boundary, sizeof(s_runtimeBoundary) - 1);
        s_runtimeBoundary[sizeof(s_runtimeBoundary) - 1] = '\0';
    }
}

void log_runtime_windows_exception(uint32_t exceptionCode, const void* faultAddress) {
#ifdef _WIN32
    if(s_runtimeLog == NULL || s_runtimeClosed)
        return;
    s_runtimeFatal = true;
    fprintf(s_runtimeLog, "[CRASH][sequence=%llu][frame=%llu][turn=%u phase=%s] windows exception=0x%08lx address=%p boundary=%s\n",
        (unsigned long long)s_runtimeSequence, (unsigned long long)s_runtimeFrame,
        s_runtimeBattleTurn, s_runtimeBattlePhase[0] == '\0' ? "none" : s_runtimeBattlePhase,
        (unsigned long)exceptionCode, faultAddress,
        s_runtimeBoundary[0] == '\0' ? "none" : s_runtimeBoundary);
    fflush(s_runtimeLog);
#else
    (void)exceptionCode;
    (void)faultAddress;
#endif
}

void log_runtime_end(void) {
    if(s_runtimeLog == NULL || s_runtimeClosed)
        return;
    log_runtime_event("RUNTIME", "shutdown=%s", s_runtimeFatal ? "abnormal" : "normal");
    fflush(s_runtimeLog);
    if(s_logdest == s_runtimeLog)
        s_logdest = NULL;
    fclose(s_runtimeLog);
    s_runtimeLog = NULL;
    s_runtimeClosed = true;
}

log_level_t log_get_level(void) {
    return s_loglevel;
}

void logger_set_level(struct Logger *l, log_level_t level) {
    l->level = level;
}

void logger_set_dest(struct Logger *l, FILE* f) {
    l->dest = f;
}

int log_msg_impl(log_level_t level, const char* file, int line, const char* fmt, ...) {
    int written = 0;
    if(s_loglevel == LOG_NONE || s_loglevel < level)
        return written;

    va_list v;
    va_start(v, fmt);
    FILE* f = (s_logdest)? s_logdest: stderr;
    switch(level)
    {
        default: break;
        case LOG_ERROR: written += fprintf(f, "[ERROR %s:%d]: ", file, line); break;
        case LOG_WARN: written += fprintf(f, "[WARN %s:%d]: ", file, line); break;
        case LOG_INFO: written += fprintf(f, "[INFO %s:%d]: ", file, line); break;
        case LOG_DEBUG: written += fprintf(f, "[DEBUG %s:%d]: ", file, line); break;
    }
    written += vfprintf(f, fmt, v);
    va_end(v);
    if(level <= LOG_WARN)
        fflush(f);
    return written;
}

int loga_msg_impl(log_level_t level, const char* fmt, ...) {
    int written = 0;
    if(s_loglevel == LOG_NONE || s_loglevel < level)
        return written;

    va_list v;
    va_start(v, fmt);
    FILE* f = (s_logdest)? s_logdest: stderr;
    written += vfprintf(f, fmt, v);
    va_end(v);
    if(level <= LOG_WARN)
        fflush(f);
    return written;
}
