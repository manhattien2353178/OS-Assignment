#include "queue.h"
#include "sched.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;
static struct queue_t running_list;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
static int current_queue_index = 0;
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
    for (int prio = 0; prio < MAX_PRIO; prio++) {
        if (!empty(&mlq_ready_queue[prio])) {
            return 0;
        }
    }
    return 1;
#else
    return empty(&ready_queue);
#endif
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    for (int i = 0; i < MAX_PRIO; i++) {
        mlq_ready_queue[i].size = 0;
        slot[i] = MAX_PRIO - i;
    }
    current_queue_index = 0;
#endif
    ready_queue.size = 0;
    run_queue.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
struct pcb_t *get_mlq_proc(void) {
    struct pcb_t *proc = NULL;

    pthread_mutex_lock(&queue_lock);

    for (int i = 0; i < MAX_PRIO; i++) {
        int queue_index = (current_queue_index + i) % MAX_PRIO;

        if (!empty(&mlq_ready_queue[queue_index]) && slot[queue_index] > 0) {
            proc = dequeue(&mlq_ready_queue[queue_index]);
            if (proc) {
                slot[queue_index]--;
                if (slot[queue_index] == 0) { // Only move if slot is used up
                    current_queue_index = (queue_index + 1) % MAX_PRIO;
                    slot[queue_index] = MAX_PRIO - queue_index; // Reset slot
                }
                break;
            }
        } else if (slot[queue_index] == 0) {
            slot[queue_index] = MAX_PRIO - queue_index;
            current_queue_index = (queue_index + 1) % MAX_PRIO; // Move on
        }
    }

    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_mlq_proc(struct pcb_t *proc) {
    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc) {
    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

struct pcb_t *get_proc(void) {
    return get_mlq_proc();
}

void put_proc(struct pcb_t *proc) {
    proc->ready_queue = &ready_queue;
    proc->mlq_ready_queue = mlq_ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);

    return put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc) {
    proc->ready_queue = &ready_queue;
    proc->mlq_ready_queue = mlq_ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

#else

struct pcb_t *get_proc(void) {
    struct pcb_t *proc = NULL;
    pthread_mutex_lock(&queue_lock);
    if (!empty(&ready_queue)) {
        proc = dequeue(&ready_queue);
    }
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_proc(struct pcb_t *proc) {
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc) {
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&ready_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

#endif