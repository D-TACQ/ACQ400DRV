/*
 * spadcop.c : copy src to spadN on timeout
 * sscanf(buf, "%d,%d,%x,%d", &enable, &site, &reg, &usecs);
 *
 *
 *  Created on: 23 Jul 2019
 *      Author: pgm
 */

#include "acq400.h"

int spadcop_tightloop = 0;
module_param(spadcop_tightloop, int, 0644);
MODULE_PARM_DESC(spadcop_tightloop, "set to 1 to ignore timer interrupt and go flat out");

struct SpadCop {
	volatile u32* src;
	volatile u32* dst;
	ktime_t kt_period;
	struct hrtimer timer;
	int enabled;
	int src_site;
	u32 reg;
	unsigned updates;
};


enum hrtimer_restart _spadCopTime(struct hrtimer* hrt, ktime_t _kt)
{
	uint64_t kt = (uint64_t)_kt;	// do not use before 1970!
	struct SpadCop *sc = container_of(hrt, struct SpadCop, timer);
	unsigned ns = do_div(kt, 1000000000);
	unsigned secs = kt&0xfffff;	/* truncated to 20b */
	unsigned short ms = ns/1000000;

	secs = (secs << 12) | ms;

	*sc->dst = secs;				// 20 bits seconds, 12 bits msec
	hrtimer_forward_now(hrt, sc->kt_period);
	return HRTIMER_RESTART;
}
enum hrtimer_restart spadCopTimeRealAction(struct hrtimer* hrt)
{
	return _spadCopTime(hrt, ktime_get_real_ns());
}

enum hrtimer_restart spadCopTimeTAIAction(struct hrtimer* hrt)
{
	return _spadCopTime(hrt, ktime_get_clocktai());
}

enum hrtimer_restart spadCopMsecAction(struct hrtimer* hrt)
{
	struct SpadCop *sc = container_of(hrt, struct SpadCop, timer);

	if (acq400_trigger_ns){
		u64 ns =  ktime_get_real_ns() - acq400_trigger_ns;
		*sc->dst = (ns >> 20);		// 20 bits seconds, 12 bits msec ?
	}else{
		*sc->dst = 0;
	}
	hrtimer_forward_now(hrt, sc->kt_period);
	return HRTIMER_RESTART;
}
enum hrtimer_restart spadCopRampAction(struct hrtimer* hrt)
{
	struct SpadCop *sc = container_of(hrt, struct SpadCop, timer);
	*sc->dst = ++sc->updates;
	hrtimer_forward_now(hrt, sc->kt_period);
	return HRTIMER_RESTART;
}
enum hrtimer_restart spadCopRead(struct hrtimer* hrt)
{
	struct SpadCop *sc = container_of(hrt, struct SpadCop, timer);
	++sc->updates;
	*sc->dst = *sc->src;

	hrtimer_forward_now(hrt, sc->kt_period);
	return HRTIMER_RESTART;
}

enum hrtimer_restart spadCopReadClear(struct hrtimer* hrt)
{
	struct SpadCop *sc = container_of(hrt, struct SpadCop, timer);
	++sc->updates;
	*sc->src = *sc->dst = *sc->src;

	hrtimer_forward_now(hrt, sc->kt_period);
	return HRTIMER_RESTART;
}
void spadCopStart(struct SpadCop *sc)
{
	hrtimer_init(&sc->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	sc->timer.function = sc->reg==0xcafe? spadCopMsecAction:
			     sc->reg==0xfeed? spadCopTimeRealAction:
			     sc->reg==0xd0d0? spadCopTimeTAIAction:
			     sc->reg==0xcccc? spadCopRampAction:
			     sc->enabled&0x2? spadCopReadClear: spadCopRead;
	if (spadcop_tightloop == 0){
		hrtimer_start(&sc->timer, sc->kt_period, HRTIMER_MODE_REL);
	}else{
		while(sc->enabled){
			int iy;
			sc->timer.function(&sc->timer);
			for (iy = 0; iy < spadcop_tightloop; ++iy){
				yield();
			}
		}
	}
}

void spadCopStop(struct SpadCop *sc)
{
	if (spadcop_tightloop == 0){
		hrtimer_cancel(&sc->timer);
	}
}

#define MAXSPAD	8
struct SpadCop scx[MAXSPAD];

int _spad_cop_enable(int ispad, u32 site, u32 reg, u32 usecs)
{
	struct acq400_dev* adev = acq400_lookupSite(site);
	struct SpadCop* sc = &scx[ispad];
	if (adev){
		struct acq400_dev* site0 = acq400_lookupSite(0);
		sc->dst = (u32*)(site0->dev_virtaddr+SPADN(ispad));
		sc->src = (u32*)(adev->dev_virtaddr+reg);
		sc->reg = reg;
		sc->src_site = site;
		sc->kt_period = usecs * 1000;
		sc->updates = 0;
		spadCopStart(sc);
		return 0;
	}else{
		return -ENODEV;
	}
}
int _spad_cop_set(int ispad, const char* buf)
{
	struct SpadCop* sc = &scx[ispad];
	u32 enable, site, reg, usecs;
	int nf = sscanf(buf, "%d,%d,%x,%d", &enable, &site, &reg, &usecs);
	if (nf >= 1){
		if (enable){
			if (sc->enabled){
				return -EBUSY;
			}else if (nf == 4){
				sc->enabled = enable;
				return _spad_cop_enable(ispad, site, reg, usecs);
			}else{
				return -1;
			}
		}else{
			spadCopStop(sc);
			sc->enabled = 0;
			return 0;
		}
	}else{
		return -1;
	}
}
int spad_cop_set(int ispad, const char* buf)
{
	if (ispad < 0 || ispad >= MAXSPAD) return -1;
	return _spad_cop_set(ispad, buf);
}

int _spad_cop_show(int ispad, char* buf)
{
	struct SpadCop* sc = &scx[ispad];
	u32 usecs = sc->kt_period >> 10;
	return sprintf(buf, "%d,%d,%x,%d %u\n",
			sc->enabled, sc->src_site, sc->reg, usecs, sc->updates);
}
int spad_cop_show(int ispad, char* buf)
{
	if (ispad < 0 || ispad >= MAXSPAD) return -1;
	return _spad_cop_show(ispad, buf);
}

#define MAKE_SPADCOP(NAME)						\
static ssize_t show_spadcop##NAME(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return spad_cop_show(NAME, buf);				\
}									\
									\
static ssize_t store_spadcop##NAME(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	int rc = spad_cop_set(NAME, buf);				\
	return rc == 0? count: rc;					\
}									\
static DEVICE_ATTR(spadcop##NAME, S_IRUGO|S_IWUSR, 		        \
		show_spadcop##NAME, store_spadcop##NAME)


MAKE_SPADCOP(0);
MAKE_SPADCOP(1);
MAKE_SPADCOP(2);
MAKE_SPADCOP(3);
MAKE_SPADCOP(4);
MAKE_SPADCOP(5);
MAKE_SPADCOP(6);
MAKE_SPADCOP(7);


const struct attribute *spadcop_attrs[] = {
	&dev_attr_spadcop0.attr,
	&dev_attr_spadcop1.attr,
	&dev_attr_spadcop2.attr,
	&dev_attr_spadcop3.attr,
	&dev_attr_spadcop4.attr,
	&dev_attr_spadcop5.attr,
	&dev_attr_spadcop6.attr,
	&dev_attr_spadcop7.attr,
	NULL
};
