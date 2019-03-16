/*
 * acq400_debugfs_internal.h
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 */

#ifndef ACQ400_DEBUGFS_INTERNAL_H_
#define ACQ400_DEBUGFS_INTERNAL_H_

static int axi_s16_set(void *data, u64 val)
/* we'd need a shadow for this .. ignore for now */
{
	return -1;
}
static int axi_s16_get(void *data, u64 *val)
{
	u32 addr = (u32)data;
	u32 v32 = *(u32*)(addr& ~3);
	if (addr&3){
		*val = v32 & 0x0000ffff;
	}else{
		*val = v32 >> 16;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_axi_s16, axi_s16_get, axi_s16_set, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_axi_s16_ro, axi_s16_get, NULL, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_axi_s16_wo, NULL, axi_s16_set, "%lld\n");


extern struct dentry *debugfs_create_axi_s16(const char *name, umode_t mode,
				  struct dentry *parent, u16 *value);



extern struct dentry* acq400_debug_root;


#define DBG_REG_CREATE_NAME(name, reg) do {				\
	int rc = dev_rc_register(DEVP(adev), &adev->reg_cache, reg);	\
	void* va = rc==0? adev->reg_cache.data: adev->dev_virtaddr; 	\
	sprintf(pcursor, "%s.0x%02x", name, reg);			\
	debugfs_create_x32(pcursor, S_IRUGO, adev->debug_dir, va+(reg));\
	pcursor += strlen(pcursor) + 1; 				\
	} while(0)

#define DBG_REG_CREATE_NAME_N(name) DBG_REG_CREATE_NAME(#name, name)

#define DBG_REG_CREATE_NAME_NC(name, reg) do {				\
	void* va = adev->dev_virtaddr; 					\
	sprintf(pcursor, "%s.0x%02x", name, reg);			\
	debugfs_create_x32(pcursor, S_IRUGO, adev->debug_dir, va+(reg));\
	pcursor += strlen(pcursor) + 1; 				\
	} while(0)

#if 0
#define DBG_REG_CREATE(reg) do {					\
	int rc = dev_rc_register(DEVP(adev), &adev->reg_cache, reg);	\
	void* va = rc==0? adev->reg_cache.data: adev->dev_virtaddr; 	\
	sprintf(pcursor, "%s.0x%02x", #reg, reg);			\
	debugfs_create_x32(pcursor, S_IRUGO, adev->debug_dir, va+(reg));\
	pcursor += strlen(pcursor) + 1;					\
	} while(0)
#else
#define DBG_REG_CREATE(reg) do {					\
	void* va = adev->dev_virtaddr; 					\
	sprintf(pcursor, "%s.0x%02x", #reg, reg);			\
	debugfs_create_x32(pcursor, S_IRUGO, adev->debug_dir, va+(reg));\
	pcursor += strlen(pcursor) + 1;					\
	} while(0)
#endif

#define DBG_REG_CREATE_RW(reg) 					\
	sprintf(pcursor, "%s.0x%02x", #reg, reg);		\
	debugfs_create_x32(pcursor, S_IRUGO|S_IWUGO,		\
		adev->debug_dir, adev->dev_virtaddr+(reg));     \
	pcursor += strlen(pcursor) + 1
#define CH_REG_CREATE(name, reg)				\
	sprintf(pcursor, "%s", #name);				\
	debugfs_create_axi_s16(pcursor, S_IRUGO,		\
		chdir, adev->dev_virtaddr+(reg));     		\
	pcursor += strlen(pcursor) + 1


#endif /* ACQ400_DEBUGFS_INTERNAL_H_ */
