#include "anti_tamper.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#endif
#include "php.h"
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <stdio.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#endif

static uint64_t phpshield_rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;
#endif
}

static int phpshield_check_timing_attack(void) {
    // Multiple measurements to detect debugger slowdown
    uint64_t times[3];
    for (int i = 0; i < 3; i++) {
        uint64_t start = phpshield_rdtsc();
        volatile int dummy = 0;
        for (int j = 0; j < 100; j++) dummy++;
        times[i] = phpshield_rdtsc() - start;
    }
    
    // Average should be low, variance should be low
    uint64_t avg = (times[0] + times[1] + times[2]) / 3;
    if (avg > 50000) return 1; // Too slow
    
    // Check variance (high variance = stepping)
    uint64_t variance = 0;
    for (int i = 0; i < 3; i++) {
        uint64_t diff = times[i] > avg ? times[i] - avg : avg - times[i];
        variance += diff;
    }
    if (variance > 100000) return 1; // Too much variance
    
    return 0;
}

static int phpshield_check_single_step(void) {
#ifdef _WIN32
    CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(GetCurrentThread(), &ctx)) {
        // Check trap flag (TF) in EFLAGS
        if (ctx.EFlags & 0x100) return 1;
    }
#elif defined(__x86_64__) || defined(__i386__)
    unsigned long flags = 0;
    __asm__ volatile(
        "pushf\n\t"
        "pop %0"
        : "=r"(flags)
    );
    // Check trap flag
    if (flags & 0x100) return 1;
#endif
    return 0;
}

static int phpshield_check_extension_integrity(void) {
#ifndef _WIN32
    // Read own binary and compute hash
    FILE *fp = fopen("/proc/self/exe", "rb");
    if (!fp) return 0; // Can't verify, don't fail
    
    // Simple checksum - in production use SHA256
    unsigned long checksum = 0;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        for (size_t i = 0; i < n; i++) {
            checksum = ((checksum << 5) + checksum) + buf[i];
        }
    }
    fclose(fp);
    
    // This checksum changes if extension is patched
    // In production: embed expected hash at build time
    // For now, just detect if it's suspiciously small (patched to NOP)
    if (checksum < 1000) return 1;
#endif
    return 0;
}

static int phpshield_check_vm_container(void) {
#ifdef _WIN32
    // Check for VM/container artifacts
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\VBoxGuest", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return 1;
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\vmci", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return 1;
    }
#else
    // Check for Docker/container
    if (access("/.dockerenv", F_OK) == 0) return 1;
    
    FILE *fp = fopen("/proc/1/cgroup", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "docker") || strstr(line, "lxc") || strstr(line, "kubepods")) {
                fclose(fp);
                return 1;
            }
        }
        fclose(fp);
    }
#endif
    return 0;
}

static int phpshield_check_hardware_breakpoints(void) {
#ifdef _WIN32
    CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(GetCurrentThread(), &ctx)) {
        // Check all debug registers and flags
        if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) return 1;
        if (ctx.Dr6 || ctx.Dr7) return 1; // Breakpoint status/control
    }
#elif defined(__x86_64__) || defined(__i386__)
    unsigned long dr0 = 0, dr1 = 0, dr2 = 0, dr3 = 0, dr6 = 0, dr7 = 0;
    __asm__ volatile("mov %%dr0, %0" : "=r"(dr0));
    __asm__ volatile("mov %%dr1, %0" : "=r"(dr1));
    __asm__ volatile("mov %%dr2, %0" : "=r"(dr2));
    __asm__ volatile("mov %%dr3, %0" : "=r"(dr3));
    __asm__ volatile("mov %%dr6, %0" : "=r"(dr6));
    __asm__ volatile("mov %%dr7, %0" : "=r"(dr7));
    if (dr0 || dr1 || dr2 || dr3 || dr6 || dr7) return 1;
#endif
    return 0;
}

static int phpshield_check_remote_debugger(void) {
#ifdef _WIN32
    BOOL isRemote = FALSE;
    typedef BOOL (WINAPI *CheckRemoteDebuggerPresent_t)(HANDLE, PBOOL);
    CheckRemoteDebuggerPresent_t pCheckRemoteDebuggerPresent = 
        (CheckRemoteDebuggerPresent_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CheckRemoteDebuggerPresent");
    if (pCheckRemoteDebuggerPresent && pCheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemote)) {
        return isRemote;
    }
#endif
    return 0;
}

static int phpshield_check_process_name(void) {
#ifndef _WIN32
    // Check if process name contains debugger strings
    FILE *fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
        char cmdline[1024];
        size_t len = fread(cmdline, 1, sizeof(cmdline)-1, fp);
        fclose(fp);
        cmdline[len] = '\0';
        
        const char *debuggers[] = {"gdb", "lldb", "strace", "ltrace", "valgrind", "radare2", NULL};
        for (int i = 0; debuggers[i]; i++) {
            if (strstr(cmdline, debuggers[i])) return 1;
        }
    }
    
    // Check parent process
    fp = fopen("/proc/self/stat", "r");
    if (fp) {
        int ppid = 0;
        fscanf(fp, "%*d %*s %*c %d", &ppid);
        fclose(fp);
        
        char parent_exe[256];
        snprintf(parent_exe, sizeof(parent_exe), "/proc/%d/exe", ppid);
        char link[256];
        ssize_t len = readlink(parent_exe, link, sizeof(link)-1);
        if (len > 0) {
            link[len] = '\0';
            const char *debuggers[] = {"gdb", "lldb", "strace", "ltrace", NULL};
            for (int i = 0; debuggers[i]; i++) {
                if (strstr(link, debuggers[i])) return 1;
            }
        }
    }
#endif
    return 0;
}

static int phpshield_check_ld_preload(void) {
#ifndef _WIN32
    const char *ld_preload = getenv("LD_PRELOAD");
    if (ld_preload && strlen(ld_preload) > 0) return 1;
#endif
    return 0;
}

void phpshield_anti_tamper_init(void)
{
#ifndef _WIN32
    // Anti-ptrace on Linux - prevents attaching debugger
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    
    // Block core dumps (prevents memory dumping)
    struct rlimit rlim;
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);
#endif
}

// Continuous monitoring - call this periodically during execution
int phpshield_anti_tamper_continuous_check(void)
{
#ifndef _WIN32
    // Quick TracerPid check
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "TracerPid:", 10) == 0) {
                int pid = 0;
                sscanf(line + 10, "%d", &pid);
                fclose(fp);
                if (pid != 0) {
                    _exit(1); // Immediate exit, no cleanup
                }
                return SUCCESS;
            }
        }
        fclose(fp);
    }
#else
    if (IsDebuggerPresent()) {
        ExitProcess(1);
    }
#endif
    return SUCCESS;
}

int phpshield_anti_tamper_check(int debug)
{
    int flags = 0;
    
    // Check 1: Basic debugger detection
#ifdef _WIN32
    if (IsDebuggerPresent()) flags |= 0x01;
#else
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "TracerPid:", 10) == 0) {
                int pid = 0;
                sscanf(line + 10, "%d", &pid);
                if (pid != 0) flags |= 0x01;
                break;
            }
        }
        fclose(fp);
    }
#endif

    // Check 2: Timing attack detection
    if (phpshield_check_timing_attack()) flags |= 0x02;
    
    // Check 3: Hardware breakpoints
    if (phpshield_check_hardware_breakpoints()) flags |= 0x04;
    
    // Check 4: Remote debugger (Windows)
    if (phpshield_check_remote_debugger()) flags |= 0x08;
    
    // Check 5: LD_PRELOAD hijacking
    if (phpshield_check_ld_preload()) flags |= 0x10;
    
    // Check 6: Process name/parent process detection
    if (phpshield_check_process_name()) flags |= 0x20;
    
    // Check 7: Single-step detection (trap flag)
    if (phpshield_check_single_step()) flags |= 0x40;
    
    // Check 8: Extension integrity (detect patching)
    if (phpshield_check_extension_integrity()) flags |= 0x80;
    
    // Check 9: VM/Container detection (warning only, not fatal)
    int vm_detected = phpshield_check_vm_container();
    if (vm_detected && debug) {
        php_error_docref(NULL, E_NOTICE, "Running in VM/container environment");
    }
    
    if (flags && debug) {
        php_error_docref(NULL, E_WARNING, 
            "PHPShield anti-tamper check failed (flags: 0x%02x)", flags);
    }
    
    // Fail on any debugger flags (not VM)
    return flags ? FAILURE : SUCCESS;
}
