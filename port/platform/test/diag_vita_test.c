#include "diag_host.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long fake_time_us = 1000000ull;
unsigned long long sceKernelGetSystemTimeWide(void) { return fake_time_us; }

static unsigned int ticks(void) { return 42; }
static unsigned int vsyncs(void) { return 99; }
static void status(char *out, size_t size) { snprintf(out, size, "mock-status"); }

static int check(int condition, const char *message)
{
    if (!condition) fprintf(stderr, "diag_vita_test: %s\n", message);
    return condition ? 0 : 1;
}

int main(void)
{
    const char *path = "diag_vita_test.log";
    const char *site = "nan-input";
    char log[8192] = "";
    char line[512];
    FILE *f;
    long rss = 0, peak = 0;
    int failures = 0;
    remove(path);
    ico_diag_init(path);
    failures += check(ico_diag_progress() == 0, "initial progress must be zero");
    ico_diag_note_progress();
    ico_diag_note_progress();
    failures += check(ico_diag_progress() == 2, "progress counter must increment");
    ico_diag_set_sources(status, ticks, vsyncs);
    ico_diag_set_movie(1);
    ico_diag_milestone("test milestone");
    fake_time_us += 250000ull;
    failures += check(ico_diag_now_ns() == 1250000000ull, "monotonic time must convert microseconds to nanoseconds");
    failures += check(fabs(ico_diag_uptime() - 0.25) < 0.001, "uptime must be relative to initialization");
    failures += check(ico_diag_float_fault(site, (const void *)0x1234) == 1, "first float fault site must log");
    failures += check(ico_diag_float_fault(site, (const void *)0x1234) == 0, "duplicate site/caller must be deduplicated");
    failures += check(ico_diag_float_faults() == 2, "fault count must count every occurrence");
    failures += check(ico_diag_process_memory(&rss, &peak) == -1 && rss == -1 && peak == -1,
                      "unsupported process memory must be reported as unavailable");
    ico_diag_start(5, 10);
    ico_diag_main_thread_end();
    f = fopen(path, "rb");
    failures += check(f != NULL, "diagnostic log file must exist");
    if (f != NULL) {
        while (fgets(line, sizeof line, f) != NULL) {
            if (strlen(log) + strlen(line) + 1 < sizeof log) strcat(log, line);
        }
        fclose(f);
    }
    failures += check(strstr(log, "crash handlers") != NULL, "startup must disclose unavailable crash handling");
    failures += check(strstr(log, "ticks=42 vsyncs=99 movie=1 status=mock-status") != NULL,
                      "milestones must include supplied runtime context");
    failures += check(strstr(log, "watchdog implementation is outstanding") != NULL,
                      "watchdog request must not be reported as started");
    failures += check(strstr(log, "floating-point fault #1") != NULL, "first floating-point fault must be logged");
    remove(path);
    if (failures != 0) return 1;
    puts("Vita diagnostics mock regression passed");
    return 0;
}
