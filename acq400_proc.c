/* ------------------------------------------------------------------------- */
/* acq400_proc.c  		                     	 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 pgm, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Mar 24, 2013
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * acq400_proc.c
 *
 *  Created on: Mar 24, 2013
 *      Author: pgm
 */

#include "acq400.h"
#include "hbm.h"

static struct proc_dir_entry *acq400_proc_root;

/* proc_seq_single: single call to show only ..  bad use of proc_seq..*/
static void *acq400_proc_seq_single_start(struct seq_file *s, loff_t *pos)
{
        if (*pos == 0) {
                return s->private;
        }

        return NULL;
}

static void *acq400_proc_seq_single_next(struct seq_file *s, void *v, loff_t *pos)
{
        (*pos)++;
        return NULL;
}

static void acq400_proc_seq_stop(struct seq_file *s, void *v)
{
}

extern int ao420_mapping[];

static int acq400_proc_seq_show_channel_mapping(struct seq_file *s, void *v)
{
        struct acq400_dev *adev = v;
        int ii;
        int *mapping = IS_AO420(adev)? ao420_mapping: 0;

        for (ii = 0; ii < adev->nchan_enabled; ++ii){
        	char sep = ii+1 >= adev->nchan_enabled? '\n': ',';
        	seq_printf(s, "%d%c", mapping==0? ii+1: mapping[ii], sep);
        }
        return 0;
}

static int acq400_proc_seq_show_qstats(struct seq_file *s, void *v)
{
        struct acq400_dev *adev = v;
        int stats[_BS_MAX] = {};
        int ibuf;
        int istat;

        mutex_lock(&adev->list_mutex);
        for (ibuf = 0; ibuf < adev->nbuffers; ++ibuf){
        	stats[adev->hb[ibuf]->bstate]++;
        }
        mutex_unlock(&adev->list_mutex);

        for(istat = 0; istat < _BS_MAX; ++istat){
        	seq_printf(s, "%d,", stats[istat]);
        }
        seq_printf(s, "%d\n", adev->rt.buffers_dropped);
        return 0;
}
static int acq400_proc_seq_show_stats(struct seq_file *s, void *v)
{
        struct acq400_dev *adev = v;

        if (mutex_lock_interruptible(&adev->mutex)) {
                return -EINTR;
        }

/* acq400_knobs only shows first line .. */
        seq_printf(s, "ngets=%u "
        	      "nputs=%u ", adev->rt.nget, adev->rt.nput);
        seq_printf(s, "axi64=%u,%u,%u ",
                		adev->rt.axi64_wakeups,
                		adev->rt.axi64_firstups, adev->rt.axi64_catchups);
        seq_printf(s,  "ngetr=%d\n", adev->rt.ngetr);
        seq_printf(s, "getEmptyErrors=%u,%u "
        	      "putFullErrors=%u,%u ",
		      adev->rt.getEmptyErrors.errors, adev->rt.getEmptyErrors.report_active,
		      adev->rt.putFullErrors.errors, adev->rt.putFullErrors.report_active);

        seq_printf(s, "axi64_wakeups=%u "
                      "axi64_firstups=%u "
                      "axi64_catchups=%u "
		      "acq64_ints=%u ",
                      adev->rt.axi64_wakeups,
                      adev->rt.axi64_firstups,
		      adev->rt.axi64_catchups,
		      adev->rt.axi64_ints);
        seq_printf(s, "refill_error=%d "
        	       "please_stop=%d ",
        		adev->rt.refill_error, adev->rt.please_stop);
        seq_printf(s, "xo_in=%d xo_out=%d", adev->stats.xo.dma_buffers_in,
        		adev->stats.xo.dma_buffers_out);
        seq_printf(s, "\n");
        mutex_unlock(&adev->mutex);
        return 0;
}


static int intDevFromProcFile(struct file* file, struct seq_operations *seq_ops)
{
	seq_open(file, seq_ops);
	{
		// @@todo hack .. assumes parent is the id .. could do better?
		const char* dname = file->f_path.dentry->d_parent->d_iname;
		((struct seq_file*)file->private_data)->private =
					acq400_lookupSite(dname[0] -'0');

	}
	return 0;
}
static int acq400_proc_open_channel_mappingc(struct inode *inode, struct file *file)
{
	static struct seq_operations acq400_proc_seq_ops_channel_mapping = {
	        .start = acq400_proc_seq_single_start,
	        .next = acq400_proc_seq_single_next,
	        .stop = acq400_proc_seq_stop,
	        .show = acq400_proc_seq_show_channel_mapping
	};

	return intDevFromProcFile(file, &acq400_proc_seq_ops_channel_mapping);
}

static int acq400_proc_open_qstats(struct inode *inode, struct file *file)
{
	static struct seq_operations acq400_proc_seq_ops_qstats = {
	        .start = acq400_proc_seq_single_start,
	        .next = acq400_proc_seq_single_next,
	        .stop = acq400_proc_seq_stop,
	        .show = acq400_proc_seq_show_qstats
	};

	return intDevFromProcFile(file, &acq400_proc_seq_ops_qstats);
}
static int acq400_proc_open_stats(struct inode *inode, struct file *file)
{
	static struct seq_operations acq400_proc_seq_ops_channel_mapping = {
	        .start = acq400_proc_seq_single_start,
	        .next = acq400_proc_seq_single_next,
	        .stop = acq400_proc_seq_stop,
	        .show = acq400_proc_seq_show_stats
	};

	return intDevFromProcFile(file, &acq400_proc_seq_ops_channel_mapping);
}

static void *acq400_proc_seq_start_buffers(struct seq_file *s, loff_t *pos)
{
	struct acq400_dev *adev = s->private;

        if (*pos == 0) {
        	seq_printf(s, "Buffers\n");
        	seq_printf(s, "%3s,%8s,%10s,%8s,%s\n", "ix", "va", "pa", "len", "state");
        }
        if (*pos < adev->nbuffers){
        	return adev->hb[*pos];
        }

        return NULL;
}


static void *acq400_proc_seq_next_buffers(struct seq_file *s, void *v, loff_t *pos)
{
	struct acq400_dev *adev = s->private;

	if (++(*pos) < adev->nbuffers){
		return adev->hb[*pos];
	}else{
		return NULL;
	}
}


static int acq400_proc_seq_show_buffers(struct seq_file *s, void *v)
{
        struct acq400_dev *dev = s->private;
        struct HBM * hbm = v;

        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }

        seq_printf(s, "%03d,%p,0x%08x,0x%06x,%d\n", hbm->ix, hbm->va, hbm->pa, hbm->len, hbm->bstate);
        mutex_unlock(&dev->mutex);
        return 0;
}


static int acq400_proc_open_buffers(struct inode *inode, struct file *file)
{
	/* SEQ operations for /proc */
	static struct seq_operations acq400_proc_seq_ops_buffers = {
	        .start = acq400_proc_seq_start_buffers,
	        .next = acq400_proc_seq_next_buffers,
	        .stop = acq400_proc_seq_stop,
	        .show = acq400_proc_seq_show_buffers
	};

	return intDevFromProcFile(file, &acq400_proc_seq_ops_buffers);
}

void* lookupQueue(struct list_head* list, loff_t cursor)
/* this is _really_ inefficient. we don't care */
{
	loff_t tpos = 0;
	struct HBM *hbm;
	list_for_each_entry(hbm, list, list){
		if (tpos++ == cursor){
			return hbm;
		}
	}
	return 0;
}

#define DEF_PROC_OPSQ(QNAME) \
static void *acq400_proc_seq_start_##QNAME(struct seq_file *s, loff_t *pos)	\
{										\
	struct acq400_dev *adev = s->private;					\
        if (*pos == 0) {							\
        	if (!list_empty(&adev->QNAME)){					\
        		return list_first_entry(&adev->QNAME, struct HBM, list);\
        	}								\
        }else{									\
		return lookupQueue(&adev->QNAME, *pos);				\
	}									\
        									\
        return NULL;								\
}										\
static void *acq400_proc_seq_next_##QNAME(struct seq_file *s, void *v, loff_t *pos) \
{										\
	struct acq400_dev *adev = s->private;					\
										\
	return lookupQueue(&adev->QNAME, ++(*pos));				\
}										\
static int acq400_proc_open_##QNAME(struct inode *inode, struct file *file)	\
{										\
	/* SEQ operations for /proc */						\
	static struct seq_operations acq400_proc_seq_ops_buffers = {		\
	        .start = acq400_proc_seq_start_##QNAME,				\
	        .next = acq400_proc_seq_next_##QNAME,				\
	        .stop = acq400_proc_seq_stop,					\
	        .show = acq400_proc_seq_show_buffers				\
	};									\
										\
	return intDevFromProcFile(file, &acq400_proc_seq_ops_buffers);		\
}										\
										\
static struct file_operations acq400_proc_ops_##QNAME = {			\
        .owner = THIS_MODULE,							\
        .open = acq400_proc_open_##QNAME,					\
        .read = seq_read,							\
        .llseek = seq_lseek,							\
        .release = seq_release							\
}

DEF_PROC_OPSQ(EMPTIES);
DEF_PROC_OPSQ(INFLIGHT);
DEF_PROC_OPSQ(REFILLS);
DEF_PROC_OPSQ(OPENS);
DEF_PROC_OPSQ(STASH);
DEF_PROC_OPSQ(GRESV);


static struct file_operations acq400_proc_ops_channel_mappingc = {
        .owner = THIS_MODULE,
        .open = acq400_proc_open_channel_mappingc,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};

static struct file_operations acq400_proc_ops_qstats = {
        .owner = THIS_MODULE,
        .open = acq400_proc_open_qstats,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};


static struct file_operations acq400_proc_ops_buffers = {
        .owner = THIS_MODULE,
        .open = acq400_proc_open_buffers,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};


static struct file_operations acq400_proc_ops_stats = {
        .owner = THIS_MODULE,
        .open = acq400_proc_open_stats,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};



void acq400_init_proc(struct acq400_dev* acq400_dev)
/* create unique stats entry under /proc/acq420/ */
{
	acq400_dev->proc_entry = proc_mkdir(acq400_dev->site_no, acq400_proc_root);

	proc_create("channel_mapping", 0, acq400_dev->proc_entry, &acq400_proc_ops_channel_mappingc);
	proc_create("buffers", 0, acq400_dev->proc_entry, &acq400_proc_ops_buffers);
	proc_create("EMPTIES", 0, acq400_dev->proc_entry, &acq400_proc_ops_EMPTIES);
	proc_create("INFLIGHT", 0, acq400_dev->proc_entry, &acq400_proc_ops_INFLIGHT);
	proc_create("REFILLS", 0, acq400_dev->proc_entry, &acq400_proc_ops_REFILLS);
	proc_create("OPENS", 0, acq400_dev->proc_entry, &acq400_proc_ops_OPENS);
	proc_create("STASH", 0, acq400_dev->proc_entry, &acq400_proc_ops_STASH);
	proc_create("GRESV", 0, acq400_dev->proc_entry, &acq400_proc_ops_GRESV);
	proc_create("stats", 0, acq400_dev->proc_entry, &acq400_proc_ops_stats);
	proc_create("Qstats", 0, acq400_dev->proc_entry, &acq400_proc_ops_qstats);
}

void acq400_del_proc(struct acq400_dev* adev)
{
	remove_proc_entry(adev->site_no, acq400_proc_root);
}

static int ktime_check_show(struct seq_file *m, void *v)
{
	unsigned jiffies0 = jiffies;
	unsigned jiffies1;

	msleep(1000);
	jiffies1 = jiffies;
	seq_printf(m, "HZ:%u j0 %u j1 %u dj:%u\n", HZ, jiffies0, jiffies1,
			jiffies1>jiffies0? jiffies1-jiffies0: jiffies0-jiffies1);

	return 0;
}

static int proc_ktime_check_open(struct inode *inode, struct file *file)
{
	return single_open(file, ktime_check_show, NULL);
}
static ssize_t add_timing_check(void)
{
	static const struct file_operations ktime_check_ops = {
		.owner 		= THIS_MODULE,
		.open           = proc_ktime_check_open,
		.read		= seq_read,
		.llseek		= seq_lseek,
		.release	= single_release,
	};
	struct proc_dir_entry *entry;

	/* create the current config file */
	entry = proc_create("ktime_check", S_IRUGO, acq400_proc_root, &ktime_check_ops);
	if (!entry)
		return -ENOMEM;

	return 0;
}
void acq400_module_init_proc(void)
{
	acq400_proc_root = proc_mkdir("driver/acq400", 0);
	add_timing_check();
}
void acq400_module_remove_proc()
{
	remove_proc_entry("driver/acq400", NULL);
}
