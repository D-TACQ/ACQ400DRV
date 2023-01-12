
#include "acq400.h"


int cache_sites;
module_param(cache_sites, int, 0444);
MODULE_PARM_DESC(cache_sites, "number of sites using reg cache");

int cache_sites_mask = 0xffff;
module_param(cache_sites_mask, int, 0644);
MODULE_PARM_DESC(cache_sites_mask, "inhibit update on these sites");


#define REG_CACHE_BITS		32
#define REG_CACHE_BIT_MASK	0x1F
#define REG_CACHE_MAX		(REG_CACHE_MAP_REGS*REG_CACHE_BITS)

#define reg2map(reg, ix, bit) do { 	\
	ix = (reg)/REG_CACHE_BITS; 	\
	bit = (reg)&REG_CACHE_BIT_MASK; \
	} while(0)

#define map2reg(ix, bit) ((ix)*REG_CACHE_BITS + (bit))




int dev_rc_register(struct RegCache* reg_cache, int reg_bytes)
{
	unsigned ix, bit;
	unsigned reg = reg_bytes/sizeof(unsigned);
	if (reg < reg_cache->max_reg){
		reg2map(reg, ix, bit);
		reg_cache->map[ix] |= 1 << bit;
		return 0;
	}else{
		return -1;
	}
}

int dev_rc_register_init(struct RegCache* reg_cache, int reg_bytes, unsigned initval)
{
	unsigned ix, bit;
	unsigned reg = reg_bytes/sizeof(unsigned);
	if (reg < reg_cache->max_reg){
		reg2map(reg, ix, bit);
		reg_cache->map[ix] |= 1 << bit;
		reg_cache->data[reg] = initval;
		return 0;
	}else{
		return -1;
	}
}

unsigned* dev_rc_alloc_cache()
{
	return kzalloc(REG_CACHE_MAX*sizeof(unsigned), GFP_KERNEL);
}
int dev_rc_init(struct acq400_dev *adev, struct RegCache* reg_cache, void* va, int id, unsigned* the_cache)
{
	int max_reg = REG_CACHE_MAX;

	cache_sites |= 1<<id;
	reg_cache->id = id;
	reg_cache->adev = adev;
	reg_cache->va = va;
	reg_cache->data = the_cache;
	reg_cache->max_reg = max_reg;
	spin_lock_init(&reg_cache->lock);
	return max_reg * sizeof(unsigned);
}

void dev_rc_update(struct RegCache* reg_cache, unsigned* va)
{
	int ix, bit;

	for (ix = 0; ix < REG_CACHE_MAP_REGS; ++ix){
		u32 map = reg_cache->map[ix];
		for (bit = 0; map; ++bit, map >>= 1){
			unsigned reg = map2reg(ix, bit);

			if (map&1){
				reg_cache->data[reg] = ioread32(va + reg);
			}else if (reg > reg_cache->max_reg){
				break;
			}
		}
	}
}

int dev_rc_read(struct RegCache* reg_cache, unsigned offset, unsigned* value)
{
	int ix, bit;
	int reg = offset/sizeof(unsigned);

	if (reg > reg_cache->max_reg){
		return 1;
	}

	reg2map(reg, ix, bit);

	if ((reg_cache->map[ix] & 1<<bit) != 0){
		*value = reg_cache->data[reg];
		return 0;
	}else{
		return 1;
	}
}

int dev_rc_write(struct RegCache* reg_cache, unsigned offset, unsigned value)
{
	int ix, bit;
	int reg = offset/sizeof(unsigned);

	if (reg > reg_cache->max_reg){
		return 1;
	}

	reg2map(reg, ix, bit);

	if ((reg_cache->map[ix] & 1<<bit) != 0){
		unsigned long flags;
		spin_lock_irqsave(&reg_cache->lock, flags);
		reg_cache->data[reg] = value;
		iowrite32(value, reg_cache->adev->dev_virtaddr + offset);
		spin_unlock_irqrestore(&reg_cache->lock, flags);
		return 0;
	}else{
		return 1;
	}
}

enum hrtimer_restart dev_rc_timer_update(struct hrtimer* hrt)
{
	struct RegCache* reg_cache =
			container_of(hrt, struct RegCache, timer);
	if ((cache_sites_mask&(1<<reg_cache->id)) == 0){
		dev_rc_update(reg_cache, reg_cache->va);
	}
	hrtimer_forward_now(hrt, ktime_set(1, 0));
	return HRTIMER_RESTART;
}

int dev_rc_finalize(struct RegCache* reg_cache, int id, int has_timer)
{
	int ix, bit;
	int last = 0;

	for (ix = 0; ix < REG_CACHE_MAP_REGS; ++ix){
		for (bit = 0; bit < REG_CACHE_BITS; ++bit){
			if (reg_cache->map[ix]&(1<<bit)){
				last = map2reg(ix, bit);
			}
		}
	}
#if 0
	if (last <= reg_cache->max_reg/2){
		unsigned* old = reg_cache->data;

		if (last != 0){
			unsigned* small_data = kzalloc(++last*sizeof(unsigned), GFP_KERNEL);
			memcpy(small_data, old, last*sizeof(unsigned));
			reg_cache->data = small_data;
			reg_cache->max_reg = last;
			dev_info(DEVP(reg_cache->adev), "dev_rc_finalize reduce cache from %d to %d bytes",
							REG_CACHE_MAX*sizeof(unsigned), last*sizeof(unsigned));
		}else{
			reg_cache->data = has_timer? (unsigned*)empty_zero_page: 0;  // readonly .. just give it zeros.
			reg_cache->max_reg = last;
		}
		kfree(old);
	}
#endif
	dev_info(DEVP(reg_cache->adev), "%s site:%d max:%d last:%d map:%08x %08x %08x %08x %s",
			__FUNCTION__, id, reg_cache->max_reg, last,
			reg_cache->map[0], reg_cache->map[1],
			reg_cache->map[2], reg_cache->map[3],
			has_timer? "T": "");

	if (has_timer){
		hrtimer_init(&reg_cache->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
		reg_cache->timer.function = dev_rc_timer_update;
		hrtimer_start(&reg_cache->timer, ktime_set(0, id*10*NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}

	return last;
}




