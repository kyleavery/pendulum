#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <ucontext.h>
#include <semaphore.h>
#include <openssl/rc4.h>

#define DELAY   5
#define KEY     (unsigned char*)"abcd1234abcd1234"

extern char __executable_start;
extern char __etext;

typedef struct itimerspec timerspec_t;
typedef struct sigevent sigevent_t;

typedef struct {
    int delay;
    timer_t timer;
    timerspec_t spec;
    sigevent_t sigev;
} timer_state;

typedef struct {
    sem_t block_main;
    sem_t block_timer;
    struct {
        ucontext_t prot_rw;
        ucontext_t encrypt;
        ucontext_t delay;
        ucontext_t decrypt;
        ucontext_t prot_rx;
        ucontext_t signal;
        ucontext_t block;
    } ctx;
    struct {
        void* data;
        size_t size;
    } target;
    struct {
        unsigned char* key;
        RC4_KEY enc_key;
        RC4_KEY dec_key;
    } rc4;
} pen_state;
