#ifndef FUJIREALM_PALM_NET_H
#define FUJIREALM_PALM_NET_H

/* A TCP byte stream through FujiNet's N1: device, over the HotSync cradle
 * (fujinet-palm common/fnnet), standing in for the Atari/Lynx Netstream.
 * Every call is a FujiBus exchange on the serial port, so callers poll at a
 * bounded rate. */

#define NET_RX_CAP 1024

struct net_stream {
    unsigned char open;
    unsigned char rx[NET_RX_CAP];
    unsigned rx_read;
    unsigned rx_write;
    unsigned rx_count;
    /* Link statistics, for the diagnostics line. */
    unsigned long rx_total;
    unsigned long tx_total;
    unsigned long polls;
    unsigned lost;              /* READ/WRITE exchanges with no reply */
    unsigned char misses;       /* consecutive failed polls */
    int last_error;             /* Palm Err, or 0 */
    unsigned char ndev_error;   /* FujiNet's NDEV status, if it gave one */
};

/* Open the cradle port. 0 on success. */
int net_init(void);
void net_done(void);

/* Open tcp://host:port and wait (bounded) for it to connect. 0 on success. */
int net_open(struct net_stream *s, const char *host, unsigned port);
void net_close(struct net_stream *s);

/* Send all of buf. 0 on success. */
int net_write(struct net_stream *s, const unsigned char *buf, unsigned len);

/* STATUS, then one READ of what is waiting, into the ring. Returns the bytes
 * received, 0 if none, -1 if the connection is gone. */
int net_poll(struct net_stream *s);

/* Take one byte from the ring; 0 if empty. */
int net_get(struct net_stream *s, unsigned char *byte);

/* Why the last call failed, for the screen. */
void net_error_text(const struct net_stream *s, char *out);

#endif
