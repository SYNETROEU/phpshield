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
    uint64_t start = phpshield_rdtsc();
    volatile int dummy = 0;
    for (int i = 0; i < 100; i++) dummy++;
    uint64_t elapsed = phpshield_rdtsc() - start;
    return elapsed > 50000; // Suspiciously slow = debugger
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
        if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) return 1;
    }
#elif defined(__x86_64__) || defined(__i386__)
    unsigned long dr0 = 0, dr1 = 0, dr2 = 0, dr3 = 0;
    __asm__ volatile("mov %%dr0, %0" : "=r"(dr0));
    __asm__ volatile("mov %%dr1, %0" : "=r"(dr1));
    __asm__ volatile("mov %%dr2, %0" : "=r"(dr2));
    __asm__ volatile("mov %%dr3, %0" : "=r"(dr3));
    if (dr0 || dr1 || dr2 || dr3) return 1;
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
    // Anti-ptrace on Linux
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
#endif
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
    
    // Check 6: VM/Container detection (informational) - DISABLED
    // if (phpshield_check_vm_container()) flags |= 0x20;
    
    if (flags && debug) {
        php_error_docref(NULL, E_WARNING, 
            "PHPShield anti-tamper check failed (flags: 0x%02x)", flags);
    }
    
    // Fail on debugger flags, allow VM/container
    return (flags & 0x1F) ? FAILURE : SUCCESS;
}
