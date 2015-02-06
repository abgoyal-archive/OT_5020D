#include <linux/slab.h>
#include <linux/interrupt.h>

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"
#include "fm_link.h"

fm_s32 fm_trace_in(struct fm_trace_fifo_t *thiz, struct fm_trace_t *new_tra)
{
    FMR_ASSERT(new_tra);

    if (thiz->len < thiz->size) {
        fm_memcpy(&(thiz->trace[thiz->in]), new_tra, sizeof(struct fm_trace_t));
        thiz->trace[thiz->in].time = jiffies;
        thiz->in = (thiz->in + 1) % thiz->size;
        thiz->len++;
        //WCN_DBG(FM_DBG | RDSC, "add a new tra[len=%d]\n", thiz->len);
    } else {
        WCN_DBG(FM_WAR | RDSC, "tra buf is full\n");
        return -FM_ENOMEM;
    }

    return 0;
}

fm_s32 fm_trace_out(struct fm_trace_fifo_t *thiz, struct fm_trace_t *dst_tra)
{
    if (thiz->len > 0) {
        if (dst_tra) {
            fm_memcpy(dst_tra, &(thiz->trace[thiz->out]), sizeof(struct fm_trace_t));
            fm_memset(&(thiz->trace[thiz->out]), 0, sizeof(struct fm_trace_t));
        }
        thiz->out = (thiz->out + 1) % thiz->size;
        thiz->len--;
        //WCN_DBG(FM_DBG | RDSC, "del a tra[len=%d]\n", thiz->len);
    } else {
        WCN_DBG(FM_WAR | RDSC, "tra buf is empty\n");
    }

    return 0;
}

fm_bool fm_trace_is_full(struct fm_trace_fifo_t *thiz)
{
    return (thiz->len == thiz->size) ? fm_true : fm_false;
}

fm_bool fm_trace_is_empty(struct fm_trace_fifo_t *thiz)
{
    return (thiz->len == 0) ? fm_true : fm_false;
}


struct fm_trace_fifo_t* fm_trace_fifo_create(const fm_s8 *name) 
{
    struct fm_trace_fifo_t *tmp;

    if (!(tmp = fm_zalloc(sizeof(struct fm_trace_fifo_t)))) {
        WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_trace_fifo) -ENOMEM\n");
        return NULL;
    }

    fm_memcpy(tmp->name, name, 20);
    tmp->size = FM_TRACE_FIFO_SIZE;
    tmp->in = 0;
    tmp->out = 0;
    tmp->len = 0;
    tmp->trace_in = fm_trace_in;
    tmp->trace_out = fm_trace_out;
    tmp->is_full = fm_trace_is_full;
    tmp->is_empty = fm_trace_is_empty;
    
    WCN_DBG(FM_NTC | LINK, "%s created\n", tmp->name);

    return tmp;
}


fm_s32 fm_trace_fifo_release(struct fm_trace_fifo_t *fifo) 
{
    if (fifo) {
        WCN_DBG(FM_NTC | LINK, "%s released\n", fifo->name);
        fm_free(fifo);
    }

    return 0;
}


