#include "pendulum.h"

int prep_context(pen_state* s, int sleep_time) {
    RC4_set_key(&s->rc4.enc_key, 16, s->rc4.key);
    RC4_set_key(&s->rc4.dec_key, 16, s->rc4.key);

    if (getcontext(&s->ctx.prot_rw) == -1 ||
        getcontext(&s->ctx.encrypt) == -1 ||
        getcontext(&s->ctx.delay) == -1 ||
        getcontext(&s->ctx.decrypt) == -1 ||
        getcontext(&s->ctx.prot_rx) == -1 ||
        getcontext(&s->ctx.signal) == -1 ||
        getcontext(&s->ctx.block) == -1) {
        perror("getcontext");
        return 1;
    }

    s->ctx.prot_rw.uc_link = &s->ctx.encrypt;
    makecontext(&s->ctx.prot_rw, (void (*)(void))mprotect, 3, s->target.data, s->target.size, PROT_READ | PROT_WRITE);

    s->ctx.encrypt.uc_link = &s->ctx.delay;
    makecontext(&s->ctx.encrypt, (void (*)(void))RC4, 4, &s->rc4.enc_key, s->target.size, s->target.data, s->target.data);

    s->ctx.delay.uc_link = &s->ctx.decrypt;
    makecontext(&s->ctx.delay, (void (*)(void))sleep, 1, sleep_time);

    s->ctx.decrypt.uc_link = &s->ctx.prot_rx;
    makecontext(&s->ctx.decrypt, (void (*)(void))RC4, 4, &s->rc4.dec_key, s->target.size, s->target.data, s->target.data);

    s->ctx.prot_rx.uc_link = &s->ctx.signal;
    makecontext(&s->ctx.prot_rx, (void (*)(void))mprotect, 3, s->target.data, s->target.size, PROT_READ | PROT_EXEC);

    s->ctx.signal.uc_link = &s->ctx.block;
    makecontext(&s->ctx.signal, (void (*)(void))sem_post, 1, &s->block_main);

    s->ctx.block.uc_link = &s->ctx.prot_rw;
    makecontext(&s->ctx.block, (void (*)(void))sem_wait, 1, &s->block_timer);

    return 0;
}

int pendulum(pen_state* s, int delay) {
    if (prep_context(s, delay) != 0) {
        return 1;
    }

    sem_post(&s->block_timer);
    sem_wait(&s->block_main);

    return 0;
}

static inline ucontext_t init_ctx() {
    return (ucontext_t){
        .uc_stack.ss_sp = calloc(SIGSTKSZ, 1),
        .uc_stack.ss_size = SIGSTKSZ,
        .uc_stack.ss_flags = 0
    };
}

static inline int init_state(pen_state** s) {
    *s = calloc(1, sizeof(pen_state));
    if (*s == NULL) {
        perror("calloc");
        return 1;
    }

    (*s)->ctx.prot_rw = init_ctx();
    (*s)->ctx.encrypt = init_ctx();
    (*s)->ctx.delay = init_ctx();
    (*s)->ctx.decrypt = init_ctx();
    (*s)->ctx.prot_rx = init_ctx();
    (*s)->ctx.signal = init_ctx();
    (*s)->ctx.block = init_ctx();
    (*s)->target.data = &__executable_start;
    (*s)->target.size = (size_t) &__etext - (size_t) &__executable_start;
    (*s)->rc4.key = KEY;

    if (sem_init(&(*s)->block_main, 0, 0) == -1 ||
        sem_init(&(*s)->block_timer, 0, 0) == -1) {
        perror("sem_init");
        return 1;
    }

    return 0;
}

static inline int init_timer(pen_state* s, timer_state** t) {
    if (s == NULL || t == NULL) {
        return 1;
    }

    *t = calloc(1, sizeof(timer_state));
    if (*t == NULL) {
        perror("calloc");
        return 1;
    }

    (*t)->delay = DELAY;

    (*t)->sigev.sigev_notify = SIGEV_THREAD;
    (*t)->sigev.sigev_notify_function = (void*)setcontext;
    (*t)->sigev.sigev_value.sival_ptr = (void*)&s->ctx.prot_rw;

    if (timer_create(CLOCK_REALTIME, &(*t)->sigev, &(*t)->timer) == -1) {
        perror("timer_create");
        return 1;
    }

    (*t)->spec.it_interval.tv_sec = 0;
    (*t)->spec.it_interval.tv_nsec = 0;
    (*t)->spec.it_value.tv_sec = 0;
    (*t)->spec.it_value.tv_nsec = 1;

    return 0;
}

static int start_timer(pen_state* s, timer_state* t) {
    if (s == NULL || t == NULL) {
        return 1;
    }

    if (prep_context(s, t->delay) != 0) {
        return 1;
    }

    if (timer_settime(t->timer, 0, &t->spec, NULL) == -1) {
        perror("timer_settime");
        return 1;
    }
    sem_wait(&s->block_main);

    return 0;
}

int main() {
    pen_state* s = NULL;
    timer_state* t = NULL;
    int counter = 0;

    if (init_state(&s) != 0) {
        return 1;
    }

    if (init_timer(s, &t) != 0) {
        return 1;
    }

    if (start_timer(s, t) != 0) {
        return 1;
    }

    while(1) {
        printf("Good morning %d\n", ++counter);
        if (pendulum(s, t->delay) != 0) {
            break;
        }
    }

    return 0;
}
