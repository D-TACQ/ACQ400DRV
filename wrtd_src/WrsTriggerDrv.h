/*
 * wrs_trigger.h : White Rabbit Streamers public interface
 *
 *  Created on: 7 Jan 2025
 *      Author: pgm
 */

#ifndef WRS_TRIGGER_DRV_H_
#define WRS_TRIGGER_DRV_H_

struct wrtd_message;

typedef unsigned u32;

#define PKT_LW	10
#define WRS_PKT_LW		10
#define WRS_PKT_FULL_READ	((1+WRS_PKT_LW)*sizeof(u32)) /* full count in bytes */

class WrsTriggerDrv {
public:
    WrsTriggerDrv(const std::string&, int, int, bool);
    ~WrsTriggerDrv();

    int rx_target_count;
    int rx_count;
    int tx_count;
    int usleep_time;
    int rx_block;
    FILE *fp;
    int fd;
    int rt_prio = 0;
    int verbose = 0;
    unsigned ts;

    u32 tx_pkt[PKT_LW];
    u32* rx_pkt;

    void dump_ts(const char*);
    void set_wr_ts_drives_soft_trigger();
    void pulse_soft_trigger();

    int transmit(u32*);
    int receive(u32*);

    void copy_integers(uint32_t*, const uint32_t*, size_t);
    void write_to_file(const uint32_t*, size_t, const char*);

    const char* ui(int argc, const char** argv);
    void dump_pkt(u32* pkt, const char* id);

private:
    int sysdev_fd;
    u32 *mapped_base;
    u32 *tx_mem;
    u32 *rx_mem;
    FILE *rx_feed;
};
#endif /* WRS_TRIGGER_DRV_H_ */
