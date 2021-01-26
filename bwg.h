/*
 * bwg.h
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 */

#ifndef BWG_H_
#define BWG_H_

#undef NCHAN
#define NCHAN	8

struct bwg_dev {
	struct of_prams {
		int site;
		int irq;
		int sn;
		int phys;	/* 0: SFP, 1: PCIe */
	} of_prams;
	char devname[16];
	u32 mod_id;

	struct BWG_CHAN {
		unsigned cursor;
	} bwg_chan[NCHAN];
	struct platform_device *pdev;
	struct resource *mem;
	void *va;
	struct cdev cdev;
	char* debug_names;
	struct dentry* debug_dir;
	int RW32_debug;
};

#define dev_virtaddr	va
#define CH_LEN		0x4000
#define CH_MAX_SAM	(CH_LEN/sizeof(u32))

struct bwg_path_descriptor {
	struct bwg_dev* dev;
	int minor;
	unsigned cursor;
	u32 ch_buf[CH_LEN/sizeof(u32)];
};

#undef PD
#undef PDSZ
#define PD(filp)			((struct bwg_path_descriptor*)filp->private_data)
#define BWG_DEV(filp)			(PD(filp)->dev)
#define SETPD(filp, value)		(filp->private_data = (value))
#define PDSZ				(sizeof (struct bwg_path_descriptor))
#define DEVP(adev)			(&(adev)->pdev->dev)




extern struct bwg_dev* bwg_devices[];

void bwg_wr32(struct bwg_dev *adev, int offset, u32 value);
u32 bwg_rd32(struct bwg_dev *adev, int offset);
void bwg_createDebugfs(struct bwg_dev* adev);
void bwg_createSysfs(struct device *dev);
void bwg_createDebugfs(struct bwg_dev* adev);
void bwg_removeDebugfs(struct bwg_dev* adev);
void bwg_clear_counters(struct bwg_dev* mdev);

int bwg_clear_histo(struct bwg_dev *mdev, int minor);

void bwg_wr32(struct bwg_dev *bdev, int offset, u32 value);
u32 bwg_rd32(struct bwg_dev *bdev, int offset);

/* REGS */
#undef MOD_ID
#define MOD_ID		(0x0000)
#define __WA		(0x0020)
#define __WB		(0x0040)

#define WA_CR		(__WA+0x0)
#define WA_LEN		(__WA+0x4)
#define WA_NCO		(__WA+0x8)
#define WA_STA		(__WA+0xc)

#define WB_CR		(__WB+0x0)
#define WB_LEN		(__WB+0x4)
#define WB_NCO		(__WB+0x8)
#define WB_STA		(__WB+0xc)

/* MEMORY REGIONS 	*/

#define MEM_REGS	0x00000
#define MEM_RAM_A	0x20000
#define MRM_RAM_B	0x30000




#define MINOR_REGS	0
#define MINOR_CH(chn)		(chn)    // 1..8

#define CHIX(minor)	((minor)-1)
#define CH_OFF(chix, iw)	(MEM_RAM_A + CH_LEN*(chix) + (iw)*sizeof(u32))


#define BWG_MINOR_COUNT	9


#define MOD_ID_MGT_DRAM		0x95

#define IS_MGT_DRAM(mdev)	(GET_MOD_ID(mdev) == MOD_ID_MGT_DRAM)

#endif /* BWG_H_ */