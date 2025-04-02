#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
    if (q == NULL)
        return 1;
    return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
    /* TODO: put a new process to queue [q] */
    if (q->size == MAX_QUEUE_SIZE || q == NULL)
    {
        return;
    }
    q->proc[proc->priority] = proc;
    q->size++;
}
struct pcb_t *dequeue(struct queue_t *q)
{
    /* TODO: return a pcb whose prioprity is the highest
     * in the queue [q] and remember to remove it from q
     * */
    if (q->size == 0 || q == NULL)
    {
        return NULL;
    }
    // Track the highest priority in the process queue
    int highest_prio = 0;
    for (int i = 1; i < q->size; i++)
    {
        if (q->proc[i]->priority > q->proc[highest_prio]->priority)
        {
            highest_prio = i;
        }
    }
    // Save the highest prioritised process 
    struct pcb_t * prio_proc = q->proc[highest_prio];
    // Remove the highest prioritised process out of the queue
    for (int i = highest_prio; i < q->size - 1; i++)
    {
        q->proc[i] = q->proc[i + 1];
    }
    q->size--;
    // Return the highest prioritised process
    return prio_proc;
}
