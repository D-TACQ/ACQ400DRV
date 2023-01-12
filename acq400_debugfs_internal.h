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


/**
 * debugfs_create_u16 - create a debugfs file that is used to read and write an unsigned 16-bit value
 * @name: a pointer to a string containing the name of the file to create.
 * @mode: the permission that the file should have
 * @parent: a pointer to the parent dentry for this file.  This should be a
 *          directory dentry if set.  If this parameter is %NULL, then the
 *          file will be created in the root of the debugfs filesystem.
 * @value: a pointer to the variable that the file should read to and write
 *         from.
 *
 * This function creates a file in debugfs with the given name that
 * contains the value of the variable @value.  If the @mode variable is so
 * set, it can be read from, and written to.
 *
 * This function will return a pointer to a dentry if it succeeds.  This
 * pointer must be passed to the debugfs_remove() function when the file is
 * to be removed (no automatic cleanup happens if your module is unloaded,
 * you are responsible here.)  If an error occurs, %NULL will be returned.
 *
 * If debugfs is not enabled in the kernel, the value -%ENODEV will be
 * returned.  It is not wise to check for this value, but rather, check for
 * %NULL or !%NULL instead as to eliminate the need for #ifdef in the calling
 * code.
 */
struct dentry *debugfs_create_axi_s16(const char *name, umode_t mode,
				  struct dentry *parent, u16 *value)
{
	/* if there are no write bits set, make read only */
	if (!(mode & S_IWUGO))
		return debugfs_create_file(name, mode, parent, value, &fops_axi_s16_ro);
	/* if there are no read bits set, make write only */
	if (!(mode & S_IRUGO))
		return debugfs_create_file(name, mode, parent, value, &fops_axi_s16_wo);

	return debugfs_create_file(name, mode, parent, value, &fops_axi_s16);
}
//EXPORT_SYMBOL_GPL(debugfs_create_axi_s16);


struct dentry* acq400_debug_root;


/*
 * Use DBG_REG_CREATE_NAME[_NC] for "clock" registers ie part of the clk cache, with precise sample time.
 */
#define DBG_REG_CREATE_NAME(name, reg) do {				\
	int rc = dev_rc_register(&adev->clk_reg_cache, reg);\
	void* va = rc==0? adev->clk_reg_cache.data: adev->dev_virtaddr; \
	sprintf(pcursor, rc==0? "%s.CLK.0x%02x": "%s.0x%02x", name, reg);\
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

#define DBG_REG_CREATE_NAME_NC_NUM(root, num, name, reg) do {		\
	void* va = adev->dev_virtaddr; 					\
	sprintf(pcursor, "%s.%d.%s.0x%02x", root, num, name, reg);	\
	debugfs_create_x32(pcursor, S_IRUGO, adev->debug_dir, va+(reg));\
	pcursor += strlen(pcursor) + 1; 				\
	} while(0)

#if 0
#define DBG_REG_CREATE(reg) do {					\
	int rc = dev_rc_register(&adev->reg_cache, reg);	\
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
#if 1
#define DBG_REG_CREATE_CTRL(reg) do {					\
	void* va = adev->dev_virtaddr; 					\
	dev_rc_register_init(&adev->ctrl_reg_cache, reg, acq400rd32(adev, reg));	\
	sprintf(pcursor, "%s.CTL.0x%02x", #reg, reg);			\
	debugfs_create_x32(pcursor, S_IRUGO, adev->debug_dir, va+(reg));\
	pcursor += strlen(pcursor) + 1;					\
	} while(0)
#else
#define DBG_REG_CREATE_CTRL(reg) do {					\
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
