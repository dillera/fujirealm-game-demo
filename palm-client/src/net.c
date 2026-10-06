#include <PalmOS.h>

#include "fnnet.h"
#include "net.h"

#define UNIT 1
#define CONNECT_WAIT_TICKS (SysTicksPerSecond() * 8)
/* A run of polls with no answer at all means the cradle link is gone. */
#define MAX_MISSES 8

static unsigned char chunk[FN_NET_CHUNK];

int net_init(void)
{
    return FnOpen() == errNone ? 0 : 1;
}

void net_done(void)
{
    FnClose();
}

static void reset(struct net_stream *s)
{
    MemSet(s, sizeof(*s), 0);
}

int net_open(struct net_stream *s, const char *host, unsigned port)
{
    char url[96];
    FnNetStatus st;
    UInt32 deadline;
    Err err;

    reset(s);
    StrPrintF(url, "N1:TCP://%s:%u/", host, port);
    err = FnNetOpen(UNIT, url, FN_NET_READ_WRITE, FN_NET_TRANS_NONE);
    if (err != errNone) {
        s->last_error = err;
        return 1;
    }
    s->open = 1;
    deadline = TimGetTicks() + CONNECT_WAIT_TICKS;
    for (;;) {
        err = FnNetGetStatus(UNIT, &st);
        if (err != errNone) {
            s->last_error = err;
            break;
        }
        if (st.connected || st.avail)
            return 0;
        if (st.error != FN_NET_OK && st.error != 0) {
            s->ndev_error = st.error;
            break;
        }
        if ((Int32)(TimGetTicks() - deadline) >= 0) {
            s->ndev_error = st.error;
            break;
        }
        SysTaskDelay(SysTicksPerSecond() / 10);
    }
    net_close(s);
    return 1;
}

void net_close(struct net_stream *s)
{
    if (s->open)
        FnNetClose(UNIT);
    s->open = 0;
}

int net_write(struct net_stream *s, const unsigned char *buf, unsigned len)
{
    while (len > 0) {
        unsigned part = len > FN_NET_CHUNK ? FN_NET_CHUNK : len;
        Err err = FnNetWrite(UNIT, buf, part);

        if (err != errNone) {
            if (err == fnErrNoReply)
                ++s->lost;
            s->last_error = err;
            return 1;
        }
        s->tx_total += part;
        buf += part;
        len -= part;
    }
    return 0;
}

int net_poll(struct net_stream *s)
{
    FnNetStatus st;
    UInt16 room = (UInt16)(NET_RX_CAP - s->rx_count), want, got = 0, i;
    Err err;

    if (!s->open)
        return -1;
    if (room == 0)
        return 0;               /* parsers are behind; let them catch up */
    ++s->polls;
    err = FnNetGetStatus(UNIT, &st);
    if (err != errNone) {
        s->last_error = err;
        return ++s->misses >= MAX_MISSES ? -1 : 0;
    }
    s->misses = 0;
    if (st.avail == 0) {
        if (!st.connected) {
            s->ndev_error = st.error;
            return -1;          /* the server closed the connection */
        }
        return 0;
    }
    want = st.avail;
    if (want > room)
        want = room;
    if (want > FN_NET_CHUNK)
        want = FN_NET_CHUNK;
    err = FnNetRead(UNIT, chunk, want, &got);
    if (err != errNone) {
        /* Sent once (fnnet.h): the bytes of a lost reply are gone, and the
         * COBS framing picks up again at the next zero. */
        if (err == fnErrNoReply)
            ++s->lost;
        s->last_error = err;
        return 0;
    }
    for (i = 0; i < got; ++i) {
        s->rx[s->rx_write] = chunk[i];
        if (++s->rx_write == NET_RX_CAP)
            s->rx_write = 0;
    }
    s->rx_count += got;
    s->rx_total += got;
    return got;
}

int net_get(struct net_stream *s, unsigned char *byte)
{
    if (s->rx_count == 0)
        return 0;
    *byte = s->rx[s->rx_read];
    if (++s->rx_read == NET_RX_CAP)
        s->rx_read = 0;
    --s->rx_count;
    return 1;
}

void net_error_text(const struct net_stream *s, char *out)
{
    if (s->last_error == fnErrNoReply)
        StrCopy(out, "No answer from FujiNet. Is the cradle cabled to it?");
    else if (s->last_error == fnErrRefused)
        StrCopy(out, "FujiNet refused the request.");
    else if (s->ndev_error != 0 && s->ndev_error != FN_NET_OK)
        StrPrintF(out, "FujiNet network error %u.", (unsigned)s->ndev_error);
    else if (s->last_error != 0)
        StrPrintF(out, "Error %x.", (unsigned)s->last_error);
    else
        StrCopy(out, "The server closed the connection.");
}
