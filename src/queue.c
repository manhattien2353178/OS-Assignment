#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q) {
    if (q == NULL) return 1;
    return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc) {
    if (q == NULL || proc == NULL) {
        return;
    }

    if (q->size < MAX_QUEUE_SIZE) {
        q->proc[q->size] = proc;
        q->size++;
    }
}

struct pcb_t *dequeue(struct queue_t *q) {
    if (q == NULL || q->size == 0) {
        return NULL;
    }

    int highest_prio_index = 0;
    for (int i = 1; i < q->size; i++) {
        if (q->proc[i]->prio < q->proc[highest_prio_index]->prio) {
            highest_prio_index = i;
        }
    }

    struct pcb_t *result = q->proc[highest_prio_index];

    // Shift elements down to remove the selected process
    for (int i = highest_prio_index; i < q->size - 1; i++) {
        q->proc[i] = q->proc[i + 1];
    }
    q->size--;
    return result;
}