#include "channel.h"
#include "hashtable.h"

typedef struct {
    hash_t *gc;
} chan_gc_t;
thrd_static(int, chan_id_gen, 0)
thrd_static(chan_gc_t, chan, nullptr)
static char error_message[SCRAPE_SIZE] = {0};

typedef struct channel_co_s channel_co_t;
typedef struct msg_queue_s msg_queue_t;
struct msg_queue_s {
    channel_co_t **a;
    u32 n;
    u32 m;
};

struct channel_s {
    raii_type type;
    bool select_ready;
    u32 bufsize;
    u32 elem_size;
    u32 nbuf;
    u32 off;
    u32 id;
    char *name;
    unsigned char *buf;
    msg_queue_t a_send;
    msg_queue_t a_recv;
    raii_values_t *data;
};

struct channel_co_s {
    channel_t c;
    void_t v;
    u32 op;
    routine_t *co;
    channel_co_t *x_msg;
};

enum {
    CHANNEL_END,
    CHANNEL_SEND,
    CHANNEL_RECV,
    CHANNEL_OP,
    CHANNEL_BLK,
};

static void chan_gc(channel_t ch) {
    if (is_chan_empty()) {
        chan_gc_t *gc = try_calloc(1, sizeof(chan_gc_t));
        gc->gc = hash_create();
        chan_update(gc);
    }

    if (is_type(ch, RAII_CHANNEL))
        hash_put(chan()->gc, simd_itoa(ch->id, error_message), ch);
}

static channel_t channel_create(int elem_size, int bufsize) {
    channel_t c = try_calloc(1, sizeof(struct channel_s) + bufsize * elem_size);
    raii_values_t *s = try_calloc(1, sizeof(raii_values_t));

    c->id = ++*chan_id_gen();
    c->elem_size = elem_size;
    c->bufsize = bufsize;
    c->nbuf = 0;
    c->data = s;
    c->select_ready = false;
    c->buf = (unsigned char *)(c + 1);
    c->type = RAII_CHANNEL;

    chan_gc(c);
    return c;
}

RAII_INLINE void channel_destroy(void) {
    if (!is_chan_empty()) {
        chan_gc_t *gc = chan();
        hash_free(gc->gc);
        RAII_FREE(gc);
        chan_update(nullptr);
    }
}

RAII_INLINE channel_t channel(void) {
    return channel_create(sizeof(raii_values_t), 0);
}

RAII_INLINE channel_t channel_buf(int elem_count) {
    return channel_create(sizeof(raii_values_t), elem_count);
}

void channel_free(channel_t c) {
    if (!is_empty(c) && is_type(c, RAII_CHANNEL)) {
        c->type = -1;
        int id = c->id;
        if (!is_empty(c->name))
            RAII_FREE(c->name);

        RAII_FREE(c->data);
        RAII_FREE(c->a_recv.a);
        RAII_FREE(c->a_send.a);
        memset(c, 0, sizeof(raii_type));
        RAII_FREE(c);

        if (hash_count(chan()->gc) > 0)
            hash_delete(chan()->gc, simd_itoa(id, error_message));
    }
}

static RAII_INLINE void add_msg(msg_queue_t *a, channel_co_t *alt) {
    if (a->n == a->m) {
        a->m += 16;
        a->a = RAII_REALLOC(a->a, a->m * sizeof(a->a[ 0 ]));
    }
    a->a[ a->n++ ] = alt;
}

static RAII_INLINE void del_msg(msg_queue_t *a, u32 i) {
    --a->n;
    a->a[ i ] = a->a[ a->n ];
}

/*
 * doesn't really work for things other than CHANNEL_SEND and CHANNEL_RECV
 * but is only used as arg to channel_msg, which can handle it
 */
#define other_op(op) (CHANNEL_SEND + CHANNEL_RECV - (op))

static msg_queue_t *channel_msg(channel_t c, unsigned int op) {
    switch (op) {
        case CHANNEL_SEND:
            return &c->a_send;
        case CHANNEL_RECV:
            return &c->a_recv;
        default:
            return NULL;
    }
}

static int channel_coro_can_exec(channel_co_t *a) {
    msg_queue_t *ar;
    channel_t c;

    if (a->op == CHANNEL_OP)
        return 0;
    c = a->c;
    if (c->bufsize == 0) {
        ar = channel_msg(c, other_op(a->op));
        return ar && ar->n;
    } else {
        switch (a->op) {
            case CHANNEL_SEND:
                return c->nbuf < c->bufsize;
            case CHANNEL_RECV:
                return c->nbuf > 0;
            default:
                return 0;
        }
    }
}

static RAII_INLINE void channel_coro_enqueue(channel_co_t *a) {
    msg_queue_t *ar;

    ar = channel_msg(a->c, a->op);
    add_msg(ar, a);
}

static void channel_coro_dequeue(channel_co_t *a) {
    u32 i;
    msg_queue_t *ar;

    ar = channel_msg(a->c, a->op);
    if (is_empty(ar)) {
        snprintf(error_message, SCRAPE_SIZE, "bad use of channel_coro_dequeue op=%d\n", a->op);
        raii_panic(error_message);
    }

    for (i = 0; i < ar->n; i++)
        if (ar->a[ i ] == a) {
            del_msg(ar, i);
            return;
        }
    raii_panic("cannot find self in channel_coro_dequeue");
}

static RAII_INLINE void channel_coro_all_dequeue(channel_co_t *a) {
    int i;

    for (i = 0; a[ i ].op != CHANNEL_END && a[ i ].op != CHANNEL_BLK; i++)
        if (a[ i ].op != CHANNEL_OP)
            channel_coro_dequeue(&a[ i ]);
}

static RAII_INLINE void amove(void_t dst, void_t src, unsigned int n) {
    if (dst) {
        if (is_empty(src))
            memset(dst, 0, n);
        else
            memmove(dst, src, n);
    }
}

/*
 * Actually move the data around.  There are up to three
 * players: the sender, the receiver, and the channel itself.
 * If the channel is unbuffered or the buffer is empty,
 * data goes from sender to receiver.  If the channel is full,
 * the receiver removes some from the channel and the sender
 * gets to put some in.
 */
static void channel_coro_copy(channel_co_t *s, channel_co_t *r) {
    channel_co_t *t;
    channel_t c;
    unsigned char *cp;

    /*
     * Work out who is sender and who is receiver
     */
    if (is_empty(s) && is_empty(r))
        return;
    RAII_ASSERT(!is_empty(s));
    c = s->c;
    if (s->op == CHANNEL_RECV) {
        t = s;
        s = r;
        r = t;
    }
    RAII_ASSERT(is_empty(s) || s->op == CHANNEL_SEND);
    RAII_ASSERT(is_empty(r) || r->op == CHANNEL_RECV);

    /*
     * channel_t is empty (or unbuffered) - copy directly.
     */
    if (s && r && c->nbuf == 0) {
        amove(r->v, s->v, c->elem_size);
        return;
    }

    /*
     * Otherwise it's always okay to receive and then send.
     */
    if (r) {
        cp = c->buf + c->off * c->elem_size;
        amove(r->v, cp, c->elem_size);
        --c->nbuf;
        if (++c->off == c->bufsize)
            c->off = 0;
    }
    if (s) {
        cp = c->buf + (c->off + c->nbuf) % c->bufsize * c->elem_size;
        amove(cp, s->v, c->elem_size);
        ++c->nbuf;
    }
}

static void channel_coro_exec(channel_co_t *a) {
    int i;
    msg_queue_t *ar;
    channel_co_t *other;
    channel_t c;

    c = a->c;
    ar = channel_msg(c, other_op(a->op));
    if (ar && ar->n) {
        i = rand() % ar->n;
        other = ar->a[ i ];
        channel_coro_copy(a, other);
        channel_coro_all_dequeue(other->x_msg);
        other->x_msg[ 0 ].x_msg = other;
        coro_enqueue(other->co);
    } else {
        channel_coro_copy(a, NULL);
    }
}

static int channel_proc(channel_co_t *a) {
    int i, j, n_can, n, can_block;
    channel_t c;
    routine_t *t;

    coro_stealer();
    coro_stack_check(512);
    for (i = 0; a[ i ].op != CHANNEL_END && a[ i ].op != CHANNEL_BLK; i++);
    n = i;
    can_block = a[ i ].op == CHANNEL_END;

    t = coro_ref_current();
    for (i = 0; i < n; i++) {
        a[ i ].co = t;
        a[ i ].x_msg = a;
    }

    RAII_LOG("processed ");

    n_can = 0;
    for (i = 0; i < n; i++) {
        c = a[ i ].c;

        RAII_INFO(" %c:", "esrnb"[ a[ i ].op ]);
#ifdef USE_DEBUG
        if (c->name)
            printf("%s", c->name);
        else
            printf("%p", c);
#endif
        if (channel_coro_can_exec(&a[ i ])) {
            RAII_LOG("*");
            n_can++;
        }
    }

    if (n_can) {
        j = rand() % n_can;
        for (i = 0; i < n; i++) {
            if (channel_coro_can_exec(&a[ i ])) {
                if (j-- == 0) {
                    c = a[ i ].c;
                    RAII_INFO(" => %c:", "esrnb"[ a[ i ].op ]);
#ifdef USE_DEBUG
                    if (c->name)
                        printf("%s", c->name);
                    else
                        printf("%p", c);
#endif
                    RAII_LOG(" ");

                    channel_coro_exec(&a[ i ]);
                    return i;
                }
            }
        }
    }

    RAII_LOG(" ");
    if (!can_block)
        return -1;

    for (i = 0; i < n; i++) {
        if (a[ i ].op != CHANNEL_OP)
            channel_coro_enqueue(&a[ i ]);
    }

    a[0].c->select_ready = true;
    coro_suspend();
    coro_unref(t);

    /*
     * the guy who ran the op took care of dequeueing us
     * and then set a[0].x_msg to the one that was executed.
     */
    return (int)(a[ 0 ].x_msg - a);
}

void channel_print(channel_t c) {
    u32 i;
    printf("--- start print channel ---\n");
    printf("buf content: [nbuf: %d, off: %d] \n", c->nbuf, c->off);
    printf("buf: ");
    for (i = 0; i < c->bufsize; i++) {
        unsigned long *p = (unsigned long *)c->buf + i;
        printf("%ld ", *p);
    }
    printf("\n");
    printf("msg queue: ");
    channel_co_t *a = *(c->a_send.a);
    for (i = 0; i < c->a_send.n; i++) {
        unsigned long *p = (unsigned long *)a->v;
        printf("%ld ", *p);
        a = a->x_msg[ 0 ].x_msg;
    }
    printf("\n");
    printf("--- end print channel ---\n");
}

static RAII_INLINE int _channel_op(channel_t c, unsigned int op, void_t p, unsigned int can_block) {
    channel_co_t a[ 2 ];

    a[ 0 ].c = c;
    a[ 0 ].op = op;
    a[ 0 ].v = p;
    a[ 1 ].op = can_block ? CHANNEL_END : CHANNEL_BLK;
    if (channel_proc(a) < 0)
        return -1;
    return 1;
}

RAII_INLINE bool chan_ready(channel_t c) {
    return c->select_ready;
}

RAII_INLINE void chan_ready_reset(channel_t c) {
    c->select_ready = false;
}

RAII_INLINE int chan_send(channel_t c, void_t v) {
    return _channel_op(c, CHANNEL_SEND, &v, 1);
}

RAII_INLINE values_type chan_recv(channel_t c) {
    _channel_op(c, CHANNEL_RECV, c->data, 1);
    return c->data->value;
}
