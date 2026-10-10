/*
 * port/platform/diag_vita.c
 *
 * Vita diagnostics backend. This intentionally provides logging, monotonic
 * timestamps, progress/fault counters and the game-side diagnostic hooks.
 * The desktop backend's POSIX signal handlers, stack inspection and
 * background watchdog are not available here; those capabilities are
 * reported as unavailable instead of being silently claimed.
 */
#include "diag_host.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <psp2/kernel/threadmgr.h>

#define LOG_PATH_MAX 1024
#define FLOAT_FAULT_SITES 32
#define FUNC_NAMES 64

static FILE *log_file;
static int inited;
static unsigned long long start_us;
static IcoDiagStatusFn status_fn;
static unsigned int (*ticks_fn)(void);
static unsigned int (*vsyncs_fn)(void);
static unsigned int progress;
static unsigned int float_fault_count;
static int movie_playing;
static int effects_program;
static unsigned long long present_since_ns;
static char log_path[LOG_PATH_MAX];
static char failure[512];
static struct { const void *caller; const char *site; } float_fault_seen[FLOAT_FAULT_SITES];
static unsigned int float_fault_sites;
static struct { void *func; const char *name; } func_names[FUNC_NAMES];
static unsigned int n_func_names;
static int kanban_boot = -1000;
static int kanban_mc = -1000;

unsigned long long ico_diag_now_ns(void)
{
    /* sceKernelGetSystemTimeWide returns microseconds since boot. */
    return (unsigned long long)sceKernelGetSystemTimeWide() * 1000ull;
}

double ico_diag_uptime(void)
{
    const unsigned long long now = (unsigned long long)sceKernelGetSystemTimeWide();
    return (double)(now - start_us) / 1000000.0;
}

const char *ico_diag_log_path(void)
{
    return log_path[0] != '\0' ? log_path : NULL;
}

void ico_diag_init(const char *path)
{
    if (inited) return;
    start_us = (unsigned long long)sceKernelGetSystemTimeWide();
    if (path != NULL && path[0] != '\0') {
        size_t n = strlen(path);
        if (n >= sizeof log_path) n = sizeof log_path - 1;
        memcpy(log_path, path, n);
        log_path[n] = '\0';
        log_file = fopen(log_path, "ab");
    }
    if (log_file == NULL) log_file = stderr;
    inited = 1;
    ico_diag_log("diagnostics: Vita logging backend; crash handlers, stack unwinding, process-memory sampling and watchdog are unavailable");
}

void ico_diag_log(const char *fmt, ...)
{
    char message[1024];
    va_list ap;
    FILE *out = log_file != NULL ? log_file : stderr;
    va_start(ap, fmt);
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    fprintf(out, "[%10.3f] ico_pc: %s\n", ico_diag_uptime(), message);
    fflush(out);
}

void ico_diag_milestone(const char *fmt, ...)
{
    char message[768];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    char status[256] = "";
    unsigned int ticks = ticks_fn != NULL ? ticks_fn() : 0;
    unsigned int vsyncs = vsyncs_fn != NULL ? vsyncs_fn() : 0;
    if (status_fn != NULL) status_fn(status, sizeof status);
    ico_diag_log("milestone: ticks=%u vsyncs=%u movie=%d status=%s: %s",
                 ticks, vsyncs, movie_playing, status, message);
}

void ico_diag_set_failure(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(failure, sizeof failure, fmt, ap);
    va_end(ap);
    ico_diag_log("last failure: %s", failure);
}

void ico_diag_set_sources(IcoDiagStatusFn status, unsigned int (*main_ticks)(void),
                          unsigned int (*vsyncs)(void))
{
    status_fn = status;
    ticks_fn = main_ticks;
    vsyncs_fn = vsyncs;
}

void ico_diag_note_progress(void) { progress++; }
unsigned int ico_diag_progress(void) { return progress; }
void ico_diag_set_movie(int playing) { movie_playing = playing != 0; }
void ico_diag_set_exit_hook(void (*fn)(const char *reason))
{
    /* Vita crash dispatch is not installed, so do not imply this hook fires. */
    if (fn != NULL) {
        ico_diag_log("fatal exit hook registered but not armed: Vita crash dispatch is outstanding");
    }
}

void ico_diag_set_fatal_ui(void (*box)(const char *text), void (*flush)(void))
{
    (void)box;
    (void)flush;
}

int ico_diag_process_memory(long *rss_kb, long *peak_kb)
{
    if (rss_kb != NULL) *rss_kb = -1;
    if (peak_kb != NULL) *peak_kb = -1;
    return -1;
}

void ico_diag_name_func(void *func, const char *name)
{
    unsigned int i;
    for (i = 0; i < n_func_names; i++) {
        if (func_names[i].func == func) {
            func_names[i].name = name;
            return;
        }
    }
    if (n_func_names < FUNC_NAMES) {
        func_names[n_func_names].func = func;
        func_names[n_func_names].name = name;
        n_func_names++;
    }
}

void ico_host_milestone(const char *what)
{
    ico_diag_milestone("%s", what != NULL ? what : "(null)");
}

void ico_host_name_func(void *func, const char *name)
{
    ico_diag_name_func(func, name);
}

void ico_host_thread_func(int id, void *func, int priority)
{
    const char *name = "(unnamed)";
    unsigned int i;
    if (id <= 0) return;
    for (i = 0; i < n_func_names; i++) {
        if (func_names[i].func == func) {
            name = func_names[i].name;
            break;
        }
    }
    ico_diag_log("game thread: id=%d priority=%d function=%s", id, priority, name);
}

void ico_host_kanban_step(int boot_step, int mc_check_step)
{
    if (boot_step == kanban_boot && mc_check_step == kanban_mc) return;
    kanban_boot = boot_step;
    kanban_mc = mc_check_step;
    ico_diag_milestone("kanbanBoot: bootStep %d, mcCheckStep %d", boot_step, mc_check_step);
}

void ico_diag_set_effects_program(int loaded) { effects_program = loaded != 0; }
void ico_diag_present_enter(void) { present_since_ns = ico_diag_now_ns(); }
void ico_diag_present_leave(void)
{
    if (present_since_ns != 0) {
        unsigned long long elapsed = ico_diag_now_ns() - present_since_ns;
        if (elapsed >= 100000000ull) {
            ico_diag_log("slow present: %.1f ms effects_program=%d",
                         (double)elapsed / 1000000.0, effects_program);
        }
    }
    present_since_ns = 0;
}

void ico_diag_watchdog_pause(int paused)
{
    ico_diag_log("watchdog pause request=%d (no Vita watchdog thread is installed)", paused != 0);
}

void ico_diag_main_thread_end(void)
{
    ico_diag_log("main thread ending");
    if (log_file != NULL && log_file != stderr) {
        fclose(log_file);
        log_file = stderr;
    }
}

void ico_diag_start(unsigned int first_s, unsigned int later_s)
{
    if (first_s != 0 || later_s != 0) {
        ico_diag_log("watchdog requested (first=%u s, later=%u s) but not started: Vita watchdog implementation is outstanding",
                     first_s, later_s);
    }
}

int ico_diag_float_fault(const char *site, const void *caller)
{
    unsigned int i;
    float_fault_count++;
    for (i = 0; i < float_fault_sites; i++) {
        if (float_fault_seen[i].caller == caller &&
            (float_fault_seen[i].site == site ||
             (float_fault_seen[i].site != NULL && site != NULL &&
              strcmp(float_fault_seen[i].site, site) == 0))) {
            return 0;
        }
    }
    if (float_fault_sites < FLOAT_FAULT_SITES) {
        float_fault_seen[float_fault_sites].caller = caller;
        float_fault_seen[float_fault_sites].site = site;
        float_fault_sites++;
    }
    ico_diag_log("floating-point fault #%u: site=%s caller=%p",
                 float_fault_count, site != NULL ? site : "(unknown)", caller);
    return 1;
}

unsigned int ico_diag_float_faults(void) { return float_fault_count; }
