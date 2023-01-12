/*
 * acq420FMC.h
 *
 *  Created on: Mar 11, 2013
 *      Author: pgm
 */

#ifndef ACQ400_STRUCTS_H_
#define ACQ400_STRUCTS_H_

#define MAX_PHYSICAL_SITES	 6
#define MAXDEVICES 		12	/* includes virtual devices 101..106 */

#define MARK(dev) 	dev_dbg((dev), "%s MARK %d", __FUNCTION__, __LINE__)

inline static const char* dio32mode2str(enum DIO432_MODE mode)
{
	switch(mode){
	case DIO432_IMMEDIATE:
		return "IMMEDIATE";
	case DIO432_CLOCKED:
		return "CLOCKED";
	default:
		return "DISABLE";
	}
}

#define MAX_AXIDMA	2

struct OF_PRAMS {
	u32 site;
	u32 dma_channel;
	u32 fifo_depth;
	u32 burst_length;
};
struct STATS {
	/* Driver statistics */
	u32 bytes_written;
	u32 writes;
	u32 reads;
	u32 opens;
	u32 closes;
	u32 errors;

	u32 interrupts;
	u32 fifo_interrupts;
	u32 dma_transactions;
	int shot;
	int completed_shot;
	int run;
	int fifo_errors;


	struct XO_STATS {
		int dma_buffers_out;
		int dma_buffers_in;
	} xo;
};

struct RT_QUEUE_REPORT {
	int report_active;
	int errors;
};

#define MAX_RT_STATUS_MESSAGE 80
struct RUN_TIME {			/** stats, cleared onStart */
	int refill_error;
	int buffers_dropped;		/* a warning, error if quit_on_buffer_exhaustion set*/
	int please_stop;
	unsigned nget;
	unsigned ngetr;
	unsigned nput;
	unsigned hb0_count;

	int status;


	unsigned hb0_ix[2];		/* [0]: previous, [1] : crnt  */
	unsigned long hb0_last;
	struct HBM* hbm_m1;		/* previous hbm for hb0 usage */
	int event_count;

	u32 samples_at_event;
	u32 samples_at_event_latch;
	u32 sample_clocks_at_event;

	u32 axi64_ints;
	u32 axi64_wakeups;		/** work look wake up count */
	u32 axi64_firstups;		/** number of top of list buffers submitted */
	u32 axi64_catchups;		/** number of backlog buffers submitted  */

	struct RT_QUEUE_REPORT getEmptyErrors;
	struct RT_QUEUE_REPORT putFullErrors;
};

#define REG_CACHE_MAP_REGS	4

struct RegCache {
	int id;
	struct acq400_dev *adev;
	unsigned *va;
	unsigned max_reg;
	unsigned map[REG_CACHE_MAP_REGS];
	unsigned *data;
	struct hrtimer timer;
	spinlock_t lock;
};

#define SITE_MAX_AI	32

/* NACC max = 256
 * SR MAX=2M
 * maxpoll = 4K
 * EPICS AI 10Hz
 * => max mean of 400 entries...  too many let's keep to 256 for >>8 adjust
 *
 * init:
 *     sums = 0
 *     nm = 0
 *
 * on poll:
 * if nm < nmax:
 *     sums += raw >> SCALE;
 *     nm += 1
 * if nm == nmax:
 *     mean = sums
 *     init()
 */
struct Subrate {
	struct acq400_dev* adev;
	int raw[SITE_MAX_AI];		/** latest raw value (could/should be DMA @@todo) */
};

struct GatherDesc {
	struct acq400_dev* adev;
	unsigned src_off;   /* source offset, bytes */
	unsigned n32;
	unsigned dst_idx;   /* dest, longs */
};

/** acq400_dev one descriptor per device */
struct acq400_dev {
	dev_t devno;
	struct mutex mutex;
	struct mutex awg_mutex;
	struct cdev cdev;
	struct platform_device *pdev;
	struct dentry* debug_dir;
	char *debug_names;
	struct proc_dir_entry *proc_entry;

	char site_no[4];		/* string, %d 3 chars, \0 */
	char dev_name[16];		/* string, acq400.%d 7+3 chars \0 */
	u32 mod_id;
	wait_queue_head_t waitq;

	struct OF_PRAMS of_prams;

	wait_queue_head_t DMA_READY;
	int dma_callback_done;
	int fifo_isr_done;

	struct dma_chan* dma_chan[2];   /* may get swapped to achieve channel align */
	struct dma_chan* dma_chan0;	/* the "real" chan0 for singles */
	int dma_cookies[2];
	struct task_struct* w_task;
	struct task_struct* h_task;	/* creates fifo histogram */
	wait_queue_head_t w_waitq;
	int task_active;


	wait_queue_head_t event_waitq;



	/* Hardware device constants */
	u32 dev_physaddr;
	void *dev_virtaddr;
	u32 dev_addrsize;

	/* Driver reference counts */
	u32 writers;

	struct STATS stats;

	u8 busy;
	u8 ramp_en;
	u8 data32;
	u8 adc_18b;			/* @@todo set on probe() */
	u8 nchan_enabled;		/* @@todo crude, assumes 1..N */
	u8 word_size;
	u8 RW32_debug;
	u8 sod_mode;			/* Sample On Demand: no trigger */

	unsigned clk_ctr_reg;
	unsigned sample_ctr_reg;

	struct mutex list_mutex;
	struct list_head EMPTIES;	/* empties waiting isr       */
	struct list_head INFLIGHT;	/* buffers in Q 	     */
	struct list_head REFILLS;	/* full buffers waiting app  */
	struct list_head OPENS;		/* buffers in use by app (1) */
	struct list_head STASH;		/* buffers kept out of play */
	struct list_head GRESV;		/* Global Reserve */

	struct HBM** hb;

	int nbuffers;			/* number of buffers available */
	int bufferlen;
	int hitide;
	int lotide;
	int sysclkhz;			/* system clock rate */
	int axi_buffers_after_event;	/* run on this long if set */



	struct CURSOR {
		struct HBM** hb;
		int offset;
	} cursor;
	wait_queue_head_t refill_ready;
	wait_queue_head_t hb0_marker;


	unsigned *fifo_histo;

	struct RUN_TIME rt;

	u32 (*get_fifo_samples)(struct acq400_dev *adev);
	void (*onStart)(struct acq400_dev *adev);
	void (*onStop)(struct acq400_dev *adev);
	void (*onPutEmpty)(struct acq400_dev *adev, struct HBM* hb);
	int (*isFifoError)(struct acq400_dev *adev);

	int event_client_count;

	pid_t continuous_reader;

	unsigned clkdiv_mask;

	struct AXI64_Buffers {
		struct HBM** axi64_hb;		/* reduced set of HB's for AXI64 */
		int ndesc;
	}
		axi64[MAX_AXIDMA];

	void *axi_private;

	struct RegCache clk_reg_cache;
	struct RegCache ctrl_reg_cache;
	struct Subrate* sr;
};

unsigned* dev_rc_alloc_cache(void);
int dev_rc_init(struct acq400_dev *adev, struct RegCache* reg_cache, void* va, int id, unsigned* the_cache);
int dev_rc_register(struct RegCache* reg_cache, int reg_bytes);
int dev_rc_register_init(struct RegCache* reg_cache, int reg_bytes, unsigned initval);
/* returns 0 on success, -1 on fail */

#define RC_HAS_TIMER 1
int dev_rc_finalize(struct RegCache* reg_cache, int id, int has_timer);
void dev_rc_update(struct RegCache* reg_cache, unsigned *va);
int dev_rc_read(struct RegCache* reg_cache, unsigned offset, unsigned* value);
/** return 1 if NOT in cache, OUTPUT *value */
int dev_rc_write(struct RegCache* reg_cache, unsigned offset, unsigned value);
/** return 1 if NOT in cache */

#define acq400_rc_read(adev, reg_bytes) \
	adev->reg_cache.data[(regbytes)/sizeof(unsigned)];


#define AWG_BACKLOG	32			// @@todo

struct BQ {
	unsigned *buf;
	int head;
	int tail;
	int bq_len;
};

static inline int BQ_incr(struct BQ* bq, int cursor){
	return (cursor+1)&(bq->bq_len-1);
}

static inline int BQ_empty(struct BQ* bq){
	return bq->head == bq->tail;
}

static inline int BQ_full(struct BQ* bq){
	return BQ_incr(bq, bq->head) == bq->tail;
}

static inline void BQ_init(struct BQ* bq, int len)
{
        bq->bq_len = len;
        bq->buf = kzalloc(len*sizeof(unsigned), GFP_KERNEL);
}
static inline unsigned BQ_get(struct device* dev, struct BQ* bq)
{
	unsigned item = bq->buf[bq->tail];
	if (BQ_empty(bq)){
		dev_warn(dev, "BQ_get EMPTY");
	}
	smp_store_release(&bq->tail, BQ_incr(bq, bq->tail));
	return item;
}

static inline int BQ_get_st(struct device* dev, struct BQ* bq, unsigned* p_item)
{
	if (BQ_empty(bq)){
		return -1;
	}else{
		*p_item = bq->buf[bq->tail];
		smp_store_release(&bq->tail, BQ_incr(bq, bq->tail));
		return 0;
	}
}

static inline void BQ_put(struct device* dev, struct BQ* bq, unsigned item)
{
	if (BQ_full(bq)){
		dev_warn(dev, "BQ_put FULL");
	}
	bq->buf[bq->head] = item;
	smp_store_release(&bq->head, BQ_incr(bq, bq->head));
}

static inline void BQ_clear(struct BQ* bq)
{
	bq->head = bq->tail = 0;
}

static inline int BQ_space(struct BQ* bq)
{
	return CIRC_SPACE(bq->head, bq->tail, bq->bq_len);
}

static inline int BQ_count(struct BQ* bq)
{
	return CIRC_CNT(bq->head, bq->tail, bq->bq_len);
}

struct GPG_buffer {
	unsigned *gpg_buffer;
	unsigned *gpg_base;
	unsigned gpg_cursor;		/* words .. */
	unsigned gpg_dbgr;
	unsigned gpg_used_bits;
	unsigned gpg_final_state;
	unsigned gpg_timescaler;		/* scale STL TIMES by this factor (default=1) */
};
struct acq400_sc_dev {
	char id[16];
	char status_message[MAX_RT_STATUS_MESSAGE];
	struct acq400_dev adev;
	struct acq400_dev* aggregator_set[MAXDEVICES];
	struct acq400_dev* distributor_set[MAXDEVICES];

	struct BQ_Wrapper {
	/* bq Buffer Queue support */
		struct mutex bq_clients_mutex;		/* BQ clients */
		struct list_head bq_clients;
		int bq_overruns;
		int bq_max;
	} bqw;

	struct StreamDac {
		struct BQ_Wrapper sd_bqw;		/* SD clients (1)! */
		struct BQ refills;			/* fresh buffer Q belongs to driver */
		wait_queue_head_t sd_waitq;
		struct task_struct* sd_task;
	} stream_dac;

	struct SewFifo {
		struct mutex sf_mutex;
		struct circ_buf sf_buf;
		struct task_struct* sf_task;
		wait_queue_head_t sf_waitq;
		struct acq400_dev* adev;
		int regoff;
	} sewFifo[2];
	struct GPG_buffer gpg;
	enum  { SP_OFF, SP_EN, SP_FRAME } SpadEn;
	enum  { SD_SEW, SD_DI4, SD_DI32 } SpadDix;
	struct Spad {
		unsigned spad_en;	/* 0: off 1: spad 2:frame */
		unsigned len;		/* 1..8 */
		unsigned diX;		/* 0 : off 1: di4 2: di32 */
	} spad;				/** scratchpad enable */
	struct WrClient {
		unsigned wc_count;
		unsigned wc_ts;			/* time of event, recorded by ISR */
		unsigned wc_pid;		/* client pid, singleton 		*/
		wait_queue_head_t wc_waitq;	/* client blocks on this		*/
	} pps_client, ts_client,
	  wrtt_client0, wrtt_client1;

};

enum {	WR_TIGA_S1, WR_TIGA_S2, WR_TIGA_S3, WR_TIGA_S4, WR_TIGA_S5, WR_TIGA_S6 };

struct acq400_tiga_dev {
	struct acq400_sc_dev sc_dev;
	struct WrClient ts_clients[6];		/* hook tiga wr clients here */
	struct WrClient tt_clients[6];
};

struct acq400_bolo_dev {
	char id[16];
	struct acq400_dev adev;
	struct Bolo8 {
		char* awg_buffer;
		int awg_buffer_max;
		int awg_buffer_cursor;
		short offset_dacs[8];
	} bolo8;
};

struct ATD {
	u32 event_source;
	struct hrtimer timer;
};

struct XTD_dev {
	char id[16];
	struct acq400_dev adev;
	struct ATD atd, atd_display;
};

struct ADC_dev {
	char id[16];
	struct acq400_dev adev;

	struct Subrate subrate;

	int acq480_train;
};

struct XO_dev {
	/* woo-ah - are we using site0 playloop_length or site1 wait for testing..*/
	char id[16];
	struct acq400_dev adev;

	struct XO {
		unsigned max_fifo_samples;
		unsigned hshift;		/* scale to histo 256 elems */
		int (*getFifoSamples)(struct acq400_dev* adev);
		/* physchan is offset 0..N-1, lchan is faceplate 1..N */
		int (*physchan)(int lchan);
		unsigned fsr;
	} xo;
	struct AO_Immediate {
		union {
			short ch[AO424_MAXCHAN];
			unsigned lw[AO424_MAXCHAN/2];
		} _u;
	} AO_immediate;

	struct AOPlayloop {
		unsigned length;
		unsigned cursor;
		unsigned oneshot;
		unsigned repeats;	/* run one-shot more than once */
		unsigned maxshot;	/* max times to run a one-shot */
		unsigned maxlen;	/* max buffer (for concurrent update */
		unsigned push_buf;	/* index of current push buffer */
		unsigned pull_buf;	/* index of current pull buffer */
		unsigned first_buf;
		unsigned last_buf;
		unsigned cycles;	/* output: number of cycles so far */
	} AO_playloop;

	struct DIO432 {
		enum DIO432_MODE mode;
		unsigned byte_is_output;	/* 1:byte[0], 2:byte[1] 4:byte[2], 8:byte[3] */
		unsigned DI32;
		unsigned DO32;
	} dio432;

	struct AO424 {
		union {
			volatile u32 lw[AO424_MAXCHAN];
			struct {
				u16 ao424_spans[AO424_MAXCHAN];
				u16 ao424_initvals[AO424_MAXCHAN];
			} ch;
		} u;
		int encoded_twocmp;
	} ao424_device_settings;
};

struct PG_dev {
	char id[16];
	struct acq400_dev adev;
	struct GPG_buffer gpg;
	struct GPG_buffer gpg32;
};

#define MAXLBUF	  1024
#define BQ_MAXLEN 512
/** acq400_path_descriptor - one per open path */

struct EventInfo {
	int pollin;
	struct HBM *hbm0;
	struct HBM *hbm1;
};
struct acq400_path_descriptor {
	struct acq400_dev* dev;
	int minor;
	struct list_head RESERVED;
	struct list_head bq_list;
	wait_queue_head_t waitq;
	struct BQ bq;
	unsigned char lbuf[MAXLBUF];
	u32 samples_at_event;
	struct EventInfo eventInfo;
	unsigned client_private;
};
#define HB0_COUNT(pd)	((pd)->bq.head)

#define PD(filp)		((struct acq400_path_descriptor*)filp->private_data)
#define SETPD(filp, value)	(filp->private_data = (value))
#define PDSZ			(sizeof (struct acq400_path_descriptor))
#define ACQ400_DEV(filp)	(PD(filp)->dev)
#define DEVP(adev)		(&(adev)->pdev->dev)
#define PDEV(filp)		(DEVP(ACQ400_DEV(filp)))
#define SITE(adev)		((adev).of_prams.site)








void event_isr(unsigned long data);

extern int event_isr_msec;

extern struct acq400_dev* acq400_devices[];
extern struct acq400_dev* acq400_sites[];
extern const char* acq400_names[];

void acq400_createSysfs(struct device *dev);
void acq400_delSysfs(struct device *dev);

void acq400_module_init_proc(void);
void acq400_module_remove_proc(void);
void acq400_init_proc(struct acq400_dev* adev);
void acq400_del_proc(struct acq400_dev* adev);

void acq400wr32(struct acq400_dev *adev, int offset, u32 value);
void acq400setbits(struct acq400_dev *adev, int offset, u32 bits);
void acq400clrbits(struct acq400_dev *adev, int offset, u32 bits);

u32 acq400rd32(struct acq400_dev *adev, int offset);
u32 acq400rd32_nocache(struct acq400_dev *adev, int offset);
u32 acq400rd32_upcount(struct acq400_dev *adev, int offset);

u32 acq420_set_fmt(struct acq400_dev *adev, u32 adc_ctrl);

int getHeadroom(struct acq400_dev *adev);

#define MAXDMA	0x4000

#define GET_FULL_OK		0
#define GET_FULL_DONE 		1
#define GET_FULL_REFILL_ERR	2

#define FIFO_PA(adev)  ((adev)->dev_physaddr + AXI_FIFO)

#define NCHAN	4
#define BYTES_PER_CHANNEL(adev) ((adev)->data32? 4: 2)

static inline int _is_acq42x(struct acq400_dev *adev) {
	switch(GET_MOD_ID(adev)){
	case MOD_ID_ACQ420FMC:
	case MOD_ID_ACQ420FMC_2000:
	case MOD_ID_ACQ425ELF:
	case MOD_ID_ACQ425ELF_2000:
	case MOD_ID_ACQ424ELF:
	case MOD_ID_ACQ427ELF:
	case MOD_ID_ACQ427ELF_2000:
	case MOD_ID_ACQ423ELF:
		return true;
	default:
		return false;
	}
}

static inline int _has_variable_data32(struct acq400_dev *adev) {
	switch(GET_MOD_ID(adev)){
	case MOD_ID_ACQ420FMC:
	case MOD_ID_ACQ425ELF:
	case MOD_ID_ACQ427ELF:
	case MOD_ID_ACQ465ELF:
		return true;
	default:
		return false;
	}
}





/* HDMI_SYNC_CON */
#define SYNC_CON
int ao420_physChan(int lchan /* 1..4 */ );

int set_gpg_top(struct acq400_dev* adev, u32 gpg_count);

#define AOSS(adev)	((adev)->nchan_enabled*(adev)->word_size)
#define AOSAMPLES2BYTES(adev, xx) ((xx)*AOSS(adev))
#define AOBYTES2SAMPLES(adev, xx) ((xx)/AOSS(adev))



#define MAX_LOTIDE(adev) \
	(adev->xo.max_fifo_samples - xo400_getFillThreshold(adev)*2)



int ao420_getFifoHeadroom(struct acq400_dev* adev);


void set_spadN(struct acq400_dev* adev, int n, u32 value);
u32 get_spadN(struct acq400_dev* adev, int n);
void set_XOspadN(struct acq400_dev* adev, int n, u32 value);
u32 get_XOspadN(struct acq400_dev* adev, int n);

struct acq400_dev* acq400_lookupSite(int site);

extern int acq400_init_descriptor(struct acq400_path_descriptor** pd);

void write32(volatile u32* to, volatile u32* from, int nwords);

void dio432_set_mode(struct acq400_dev* adev, enum DIO432_MODE mode, int force);
/* immediate, clocked */
void dio432_init_clocked(struct acq400_dev* adev);
void dio432_disable(struct acq400_dev* adev);

extern int a400fs_init(void);
extern void a400fs_exit(void);

void ao424_setspan_defaults(struct acq400_dev* adev);
int ao424_set_spans(struct acq400_dev* adev);

void acq400_sew_fifo_init(struct acq400_dev* adev, int ix);
int acq400_sew_fifo_destroy(struct acq400_dev* adev, int ix);
int acq400_sew_fifo_write_bytes(
		struct acq400_dev* adev, int ix, const char __user *buf, size_t count);
void measure_ao_fifo(struct acq400_dev *adev);

static inline void x400_enable_interrupt(struct acq400_dev *adev)
{
	u32 int_ctrl = acq400rd32(adev, ADC_INT_CSR);
	acq400wr32(adev, ADC_INT_CSR,	int_ctrl|0x1);
}

static inline void x400_disable_interrupt(struct acq400_dev *adev)
{
	u32 int_ctrl = acq400rd32(adev, ADC_INT_CSR);
	acq400wr32(adev, ADC_INT_CSR, int_ctrl & ~0x1);
}


static inline u32 x400_get_interrupt(struct acq400_dev *adev)
{
	return acq400rd32(adev, ADC_INT_CSR);
}

static inline void x400_clr_interrupt(struct acq400_dev *adev, u32 int_csr)
{
	acq400wr32(adev, ADC_INT_CSR, int_csr);
}

static inline void x400_set_interrupt(struct acq400_dev *adev, u32 int_csr)
{
	acq400wr32(adev, ADC_INT_CSR, int_csr);
}



short ao424_fixEncoding(struct acq400_dev *adev, int pchan, short value);
/* LTC2752 data is always unsigned. But bipolar ranges are represented ext as
 * signed this converts, dependent on range
 */

extern int acq400_set_bufferlen(struct acq400_dev *adev, int _bufferlen);

extern int acq400_set_dist_bufferlen(struct acq400_dev *adev, int _bufferlen);
extern int acq400_get_dist_bufferlen(struct acq400_dev *adev);

enum AO_playloop_oneshot { AO_continuous, AO_oneshot, AO_oneshot_rearm };

void acq2006_estop(struct acq400_dev *adev);

int acq400_enable_trg(struct acq400_dev *adev, int enable);
/* returns TRUE if previously enabled */
void acq400_enable_trg_if_master(struct acq400_dev *adev);

extern void acq400_enable_event0(struct acq400_dev *adev, int enable);

extern int acq400_event_count_limit;

static inline int getSHL(unsigned mask)
/* converts mask to shift */
{
	int shl;
	for (shl = 0; (mask&1) == 0; ++shl, mask >>= 1){
		;
	}
	return shl;
}
static inline unsigned getField(unsigned xx, unsigned mask)
/* field normalize */
{
	return (xx&mask) >> getSHL(mask);
}

int axi64_init_dmac(struct acq400_dev *adev);
#define CMASK0	0x01
#define CMASK1  0x02
int axi64_load_dmac(struct acq400_dev *adev, unsigned cmask);
int axi64_free_dmac(struct acq400_dev *adev);
int axi64_claim_dmac_channels(struct acq400_dev *adev);
int axi64_tie_off_dmac(struct acq400_dev *adev, int ichan, int nbuffers);
/* close off descriptor +nbuffers to prevent overrun */

int acq400_setDelTrg(struct acq400_dev *adev, int ch, int threshold);
int acq400_getDelTrg(struct acq400_dev *adev, int ch, int *threshold);
int acq400_clearDelTrg(struct acq400_dev *adev);
int acq400_clearDelTrgEvent(struct acq400_dev *adev);

int acq480_train_fail(struct acq400_dev *adev);


#define SPI_STROBE_NONE 0
#define SPI_STROBE_SELF 1
#define SPI_STROBE_GROUP 2
void acq400_spi_strobe(void *clidata, int cs, int mode);


enum ACQ1014_ROUTE {
	ACQ1014_FP,
	ACQ1014_LOC,
	ACQ1014_RP
};

int acq400_get_site(struct acq400_dev *adev, char* s);
int acq400_add_set(struct acq400_dev* set[], struct acq400_dev *adev, int site);
void acq400_clear_set(struct acq400_dev* set[]);
int acq400_read_set(struct acq400_dev* set[],
		struct acq400_dev *adev, char *buf, int maxbuf);
int acq400_add_aggregator_set(struct acq400_dev *adev, int site);
int acq400_read_aggregator_set(struct acq400_dev *adev, char *buf, int maxbuf);
unsigned acq400_convert_aggregator_set_to_register_mask(struct acq400_dev *adev);
void acq400_clear_aggregator_set(struct acq400_dev *adev);
int acq400_add_distributor_set(struct acq400_dev *adev, int site);
void acq400_clear_distributor_set(struct acq400_dev *adev);
void acq400_visit_set(struct acq400_dev *set[], void (*action)(struct acq400_dev *adev));
void acq400_visit_set_arg(struct acq400_dev *set[], void (*action)(struct acq400_dev *adev, void* arg), void*arg);
void init_axi_dma(struct acq400_dev* adev);


extern void acq400_mod_init_defaults(struct acq400_dev* adev);


extern int acq420_isFifoError(struct acq400_dev *adev);	/* REFACTORME */
extern void release_dma_channels(struct acq400_dev *adev);
extern void ao420_clear_fifo_flags(struct acq400_dev *adev);
extern void ao420_reset_fifo(struct acq400_dev *adev);
extern void acq400_clear_histo(struct acq400_dev *adev);

static inline u32 acq420_get_fifo_samples(struct acq400_dev *adev)
{
	return acq400rd32(adev, ADC_FIFO_SAMPLES);
}

extern int xo400_write_fifo(struct acq400_dev* adev, int frombyte, int bytes);



extern int acq400_reserve_dist_buffers(struct acq400_path_descriptor* pd);
extern int acq400_free_buffers(struct acq400_dev *adev, int free_from);
extern int acq420_convActive(struct acq400_dev *adev);
extern void acq400_getID(struct acq400_dev *adev);

extern int get_dma_channels(struct acq400_dev *adev);
extern void release_dma_channels(struct acq400_dev *adev);
extern void add_fifo_histo(struct acq400_dev *adev, u32 status);
extern void add_fifo_histo_ao42x(struct acq400_dev *adev, unsigned samples);
extern void go_rt(int prio);

int check_fifo_statuses(struct acq400_dev *adev);

/* MGT-DRAM-8 */
int axi64_data_once(struct acq400_dev *adev, unsigned char blocks[], int nb);
void xilinx_dma_halt(struct dma_chan *chan);
void axi64_terminate(struct dma_chan* dma_chan);

extern u32 aggregator_get_fifo_samples(struct acq400_dev *adev);

extern void acq2006_aggregator_disable(struct acq400_dev *adev);
extern void acq2006_aggregator_enable(struct acq400_dev *adev);
extern void sc_data_engine_disable(unsigned dex);
extern void sc_data_engine_reset_enable(unsigned dex);
extern void acq2106_aggregator_reset(struct acq400_dev *adev);
extern void acq2106_distributor_reset_enable(struct acq400_dev *adev);
extern void acq400_enable_trg_if_master(struct acq400_dev *adev);
extern int acq400_enable_trg(struct acq400_dev *adev, int enable);

extern int fifo_monitor(void* data);

extern void poison_one_buffer_fastidious(struct acq400_dev *adev, struct HBM* hbm);
extern void poison_all_buffers(struct acq400_dev *adev);
extern int check_all_buffers_are_poisoned(struct acq400_dev *adev);
extern int dma_done(struct acq400_dev *adev, struct HBM* hbm);
extern int poison_overwritten(struct acq400_dev *adev, struct HBM* hbm);
extern void clear_poison_all_buffers(struct acq400_dev *adev);


extern unsigned poison_offset(struct acq400_dev *adev);
#define POISON0 0xc0de0000
#define POISON1 0xc1de0000

#define USZ	sizeof(u32)

#define FIRST_POISON_WORD(pob) ((pob)/USZ-2)
#define POISON_SZ		(2*USZ)

extern void poison_one_buffer(struct acq400_dev *adev, struct HBM* hbm);

extern struct acq400_dev* acq400_lookupSite(int site);
extern void acq400_enable_event0(struct acq400_dev *adev, int enable);

extern int acq400_set_AXI_DMA_len(struct acq400_dev *adev, int len);
extern int acq400_get_AXI_DMA_len(struct acq400_dev *adev);

#define NSEC_PER_MSEC	1000000L

#define XO400_PLAYCONTINUOUS  0xFFFFFFFF
int xo400_reset_playloop(struct acq400_dev* adev, unsigned playloop_length);

void acq400_enable_adc(struct acq400_dev* adev);

extern void acq400_init_event_info(struct EventInfo *eventInfo);

#define CMASK (~0x7f)

static inline u32 _acq400_adc_sample_count(struct acq400_dev* adev)
{
	u32 c0, c1;
	unsigned retry = 0;
	c0 = acq400rd32(adev, ADC_SAMPLE_CTR);
	while (((c1 = acq400rd32(adev, ADC_SAMPLE_CTR))&CMASK) != (c0&CMASK)){
		c0 = c1;
		if (++retry > 5){
			return 0xffffffff;
		}
	}
	return c1;
}
static inline u32 acq400_adc_sample_count(void)
{
        struct acq400_dev* adev = acq400_sites[1];

        if (!adev){
                return 0xffffffff;
        }else{
                return _acq400_adc_sample_count(adev);
        }
}


static inline u32 acq400_adc_latch_count(void)
{
        struct acq400_dev* adev = acq400_sites[1];

        if (!adev){
                return 0xffffffff;
        }else{
                return acq400rd32(adev, EVT_SC_LATCH);
        }
}

/* ONLY valid if SPAD enabled */
static inline u32 acq400_agg_sample_count(void)
{
	return acq400rd32(acq400_sites[0], SPADN(0));
}

void acq400_soft_trigger(unsigned enable);

static inline void acq400_timer_init(
	struct hrtimer* timer,
	enum hrtimer_restart (*function)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer->function = function;
}

extern u64 acq400_trigger_ns;

#define IRQ_REQUEST_OFFSET	0		/* arg to platform get irq is OFFSET from region. Linux knows best! */

extern int acq400_wr_init_irq(struct acq400_dev* adev);
extern int acq400_wr_open(struct inode *inode, struct file *file);
extern int acq400_tiga_open(struct inode *inode, struct file *file);
extern void ao420_reset_fifo(struct acq400_dev *adev);

extern int ao424_16;
extern void ao424_set_odd_channels(struct acq400_dev *adev, int odd_chan_en);

extern struct GPG_buffer* get_gpg(struct acq400_dev* adev, int gpg32);
void init_gpg_buffer(struct acq400_dev* adev, struct GPG_buffer *gpg, unsigned mem_base, unsigned dbgr);

extern int DMA_TIMEOUT;
#define DEFAULT_DMA_TIMEOUT 		msecs_to_jiffies(10000)
/* first time : infinite timeout .. we probably won't live this many jiffies */
#define START_TIMEOUT		0x7fffffff

extern dma_cookie_t
dma_async_memcpy_callback(
	struct dma_chan *chan,
	dma_addr_t dma_dst, dma_addr_t dma_src,
	size_t len, unsigned long flags,
	dma_async_tx_callback callback,
	void *callback_param);

extern dma_cookie_t
dma_async_memcpy(
	struct dma_chan *chan, dma_addr_t src, 	dma_addr_t dest, size_t len);
extern
int dma_memcpy(
	struct acq400_dev* adev, dma_addr_t dest, dma_addr_t src, size_t len);

extern void acq400_dma_callback(void *param);
extern void xo400_getDMA(struct acq400_dev* adev);
extern int xo_data_loop(void *data);
extern int streamdac_data_loop(void *data);
#define WORKER_DONE(pdesc)	(pdesc->client_private)

extern int firstDistributorBuffer(void);
extern int lastDistributorBuffer(void);

#endif /* ACQ400_STRUCTS_H_ */
