#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wchar.h>
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include "module/fixcpuid.h"


#define MODE_CACHE


// #define debug printf
#define debug

#define TAG "[CPUID Fix] "

#define GET_REG(ctx, reg) ((ucontext_t*)ctx)->uc_mcontext.gregs[REG_R##reg]

static int fixcpuid_fd = 0;

static uint64_t cpuid_cache[64][4];
static uint64_t cpuid_ecache[64][4];

static int cache_all_leaves() {
    uint64_t rax, rbx, rcx, rdx;
    
    rax = 0;
    asm volatile("cpuid" : "+a"(rax), "+b"(rbx), "+c"(rcx), "+d"(rdx));
    printf("Highest leaf: 0x%zx\n", rax);
    int max = rax;
    
    rax = 0x80000000;
    asm volatile("cpuid" : "+a"(rax), "+b"(rbx), "+c"(rcx), "+d"(rdx));
    printf("Highest extended leaf: 0x%zx\n", rax);
    int emax = rax;
    
    for(int i = 0; i < max; i++) {
        rax = i;
        asm volatile("cpuid" : "+a"(rax), "+b"(rbx), "+c"(rcx), "+d"(rdx));
        cpuid_cache[i][0] = rax;
        cpuid_cache[i][1] = rbx;
        cpuid_cache[i][2] = rcx;
        cpuid_cache[i][3] = rdx;
    }
    for(int i = 0x80000000; i < emax; i++) {
        rax = i;
        asm volatile("cpuid" : "+a"(rax), "+b"(rbx), "+c"(rcx), "+d"(rdx));
        cpuid_ecache[i - 0x80000000][0] = rax;
        cpuid_ecache[i - 0x80000000][1] = rbx;
        cpuid_ecache[i - 0x80000000][2] = rcx;
        cpuid_ecache[i - 0x80000000][3] = rdx;
    }
}

static int fixcpuid_handler(int signal, siginfo_t *info, void *ctx) {
    if(info->si_code != SI_KERNEL) return 0;

    if(*(uint16_t*)GET_REG(ctx, IP) != 0xa20f) return 0; // ignore if not cpuid opcode

    uint64_t rax, rbx, rcx, rdx;
    rax = GET_REG(ctx, AX);
    rcx = GET_REG(ctx, CX);

    debug(TAG "CPUID call: leaf=%zd, subleaf=%zd\n", rax, rcx);

#if defined(MODE_CACHE)
    size_t index = rax;
    if(index >= 0x80000000) {
        rax = cpuid_ecache[index - 0x80000000][0];
        rbx = cpuid_ecache[index - 0x80000000][1];
        rcx = cpuid_ecache[index - 0x80000000][2];
        rdx = cpuid_ecache[index - 0x80000000][3];
    } else {
        rax = cpuid_cache[index][0];
        rbx = cpuid_cache[index][1];
        rcx = cpuid_cache[index][2];
        rdx = cpuid_cache[index][3];
    }
#else
    ioctl(fixcpuid_fd, FIXCPUID_IOCTL_CMD_TRAP_CPUID, 0);
    asm volatile("rdrand %%rcx" : : : "rcx");
    asm volatile("cpuid" : "+a"(rax), "+b"(rbx), "+c"(rcx), "+d"(rdx));
    ioctl(fixcpuid_fd, FIXCPUID_IOCTL_CMD_TRAP_CPUID, 1);
#endif

    GET_REG(ctx, IP) += 2; // skip cpuid
    GET_REG(ctx, AX) = rax;
    GET_REG(ctx, BX) = rbx;
    GET_REG(ctx, CX) = rcx;
    GET_REG(ctx, DX) = rdx;
    return 1;
}


void __attribute__((constructor)) _cpuidifx() {
  if(!fixcpuid_fd) fixcpuid_fd = open(FIXCPUID_DEVICE_PATH, O_RDONLY);
  if(fixcpuid_fd == -1) {
      debug(TAG "Could not init kernel module, did you load it?\n");
  }
    
    debug(TAG "Init\n");
#if defined(MODE_CACHE)
    cache_all_leaves();
#endif
    
    ioctl(fixcpuid_fd, FIXCPUID_IOCTL_CMD_TRAP_CPUID, 1);
    
    struct sigaction sa = {
        .sa_sigaction = (void*)fixcpuid_handler,
        .sa_flags = SA_SIGINFO,
    };

    sigaction(SIGSEGV, &sa, NULL);

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGSEGV);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    debug(TAG "Init done\n");
}

void __attribute__((destructor)) _cpuidifx_done() {
    debug(TAG "Cleanup\n");
    ioctl(fixcpuid_fd, FIXCPUID_IOCTL_CMD_TRAP_CPUID, 0);
    debug(TAG "Goodbye!\n");
}

