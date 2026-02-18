#include <stdio.h>
#include <stdlib.h>

#include "ttk.h"
#include "ttk_priv.h"

ttk_timer ttk_timers = 0;

ttk_timer ttk_create_timer(int ms, void (*fn)()) {
#define TIMER_ALLOCD (head->delay)
    static ttk_timer head;
    ttk_timer cur;
    int num;

    if (!ms && !fn) {
        free(head);
        head = 0;
        return 0;
    }
    for (cur = head; cur && cur->next; cur = cur->next);
    num = (cur) ? cur - head : 0;

    if (head && ttk_timers && ttk_timers != head->next) {
        head->next = ttk_timers;
    }
    if (!head || ++num > TIMER_ALLOCD) {
        int a = head ? TIMER_ALLOCD + 64 : 63;
        cur = realloc(head, (a + 1) * sizeof(struct _ttk_timer));
        if (head && cur != head) {
            fprintf(stderr, "realloc relocated timers. Broken.\n");
        }
        if (!head) cur->next = 0;
        head = cur;
        TIMER_ALLOCD = a;
    }

    cur = head;
    while (cur->next == cur + 1) cur = cur->next;
    (cur + 1)->next = cur->next;
    cur->next = cur + 1;
    cur = cur->next;
    ttk_timers = head->next;

    cur->started = ttk_getticks();
    cur->delay = ms;
    cur->fn = fn;
    return cur;
#undef TIMER_ALLOCD
}

void ttk_destroy_timer(ttk_timer tim) {
    ttk_timer cur = ttk_timers, last = 0;
    while (cur && (cur != tim)) {
        last = cur;
        cur = cur->next;
    }
    if (cur != tim) {
        fprintf(stderr, "Warning: didn't delete nonexistent timer %p\n", tim);
        return;
    }
    if (last)
        last->next = cur->next;
    else
        ttk_timers = cur->next;
}
