# new-product-readme.md : Adding a new product to ESW

We trace an example: ACQ426ELF

## Packages affected:
1. ACQ400DRV 10-acq420-YYMMDD*tgz
2. acq400ioc 40-acq400ioc-YYMMDD*tgz

## Pre-requisites
A C/C++ cross reference of ACQ400DRV and acq400_kernel saves a huge amount of time.
Peter uses Eclipse CDT. 

# ACQ400DRV

## git-log

Best practise: make a new branch for the new feature, then fold in asap.

```
commit e00c5b85b00d28bed1b694e919ca2937c5ed38fe
Author: Peter Milne <peter.milne@d-tacq.com>
Date:   Sun Oct 20 17:48:20 2024 +0100

    driver plumbing for ACQ426.
    nb: so far, it's really mostly a clone of ACQ465
     On branch acq426
            modified:   Makefile
            modified:   acq400.h
            modified:   acq400_debugfs.c
--
            modified:   acq400_structs.h
            modified:   acq400_sysfs.c
            modified:   init/acq420.init
            deleted:    scripts/acq426.init
            modified:   scripts/mod_id.sh

commit 95e5f7f4240572ae3719c4f3d4afd6e5bee21d20
Author: Peter Milne <peter.milne@d-tacq.com>
Date:   Sun Oct 20 17:48:20 2024 +0100

    driver pluming for ACQ426.
    nb: so far, it's really mostly a clone of ACQ465
     On branch acq426
            modified:   Makefile
            modified:   acq400.h
            modified:   acq400_debugfs.c
--
            modified:   acq400_structs.h
            modified:   acq400_sysfs.c
            modified:   init/acq420.init
            deleted:    scripts/acq426.init
            modified:   scripts/mod_id.sh

commit a45630717dd658185b2305c93f9ea8a43938c1be
Author: Peter Milne <peter.milne@d-tacq.com>
Date:   Sun Oct 20 17:46:27 2024 +0100

    prep userland for ACQ426
    General: move all module.init to CARE to limit /usr/local/bin len
     On branch acq426
            new file:   CARE/acq426.init
            new file:   CARE/acq426_adc.init
            new file:   acq426_drv.c
            new file:   scripts/acq426.init
    
            renamed:    scripts/acq423.init -> CARE/acq423.init
            renamed:    scripts/acq425.init -> CARE/acq425.init
--

```

## grep 
CDT xref is better for this, but harder to report

```
base) pgm@hoy6:~/PROJECTS/ACQ400/ACQ400DRV$ grep ACQ426 *.h
```
### MTYPE
```
acq400_mod_id.h:#define MOD_ID_ACQ426ELF	0x0c
```
### All important ...
```
acq400.h:#define IS_ACQ426(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ426ELF)
```
### regdefs. see doxygen. DRY : Do NOT repeat existing definitions.
```
acq400.h:#define ACQ426_BCSR		(ADC_BASE+0x34)
..
acq400.h:#define ACQ426_FIFO_STA_CAL_FAIL (0x0f000000)
```
### choices to make..
```
acq400_structs.h:	case MOD_ID_ACQ426ELF:
acq400_structs.h:	case MOD_ID_ACQ426ELF:
```
### extensions to main driver acq420fmc.ko
```
base) pgm@hoy6:~/PROJECTS/ACQ400/ACQ400DRV$ grep ACQ426 *.c
```
### DebugFS : YES
```
acq400_debugfs.c:	if (IS_ACQ465(adev)||IS_ACQ426(adev)){
acq400_debugfs.c:	DBG_REG_CREATE(ACQ426_BCSR);
acq400_debugfs.c:	DBG_REG_CREATE(ACQ426_CAL_POINT);
acq400_debugfs.c:	DBG_REG_CREATE(ACQ426_CAL_WIN);
acq400_debugfs.c:		case MOD_ID_ACQ426ELF:
```
### init_defaults: YES
```
acq400_init_defaults.c:		if IS_ACQ426(adev){
```
### sysfs: YES
```
acq400_sysfs.c:		{ MOD_ID_ACQ426ELF,     "acq426elf"	},
acq400_sysfs.c:MAKE_BITS(va_en, ACQ426_BCSR, MAKE_BITS_FROM_MASK, ACQ426_BCSR_VA_EN);
acq400_sysfs.c:MAKE_BITS(vset,  ACQ426_BCSR, MAKE_BITS_FROM_MASK, ACQ426_BCSR_VSET);
acq400_sysfs.c:MAKE_BIT_RON(busy,  ACQ426_BCSR, MAKE_BITS_FROM_MASK, ACQ426_BCSR_BSY);
acq400_sysfs.c:MAKE_BIT_RON(los,   ACQ426_BCSR, MAKE_BITS_FROM_MASK, ACQ426_BCSR_LOS);
acq400_sysfs.c:		acq400wr32(adev, ADC_CTRL, ctrl |= ACQ426_ADC_CTRL_CALIB);
acq400_sysfs.c:		acq400wr32(adev, ADC_CTRL, ctrl &= ~ACQ426_ADC_CTRL_CALIB);
acq400_sysfs.c:	u32 ena = (ctrl&ACQ426_ADC_CTRL_CALIB) != 0;
acq400_sysfs.c:	u32 fail = (sta&ACQ426_FIFO_STA_CAL_FAIL) != 0;
acq400_sysfs.c:	u32 pass = (sta&ACQ426_FIFO_STA_CAL_COMP)==ACQ426_FIFO_STA_CAL_COMP && !fail;
acq400_sysfs.c:	sta &= ACQ426_FIFO_STA_CAL_COMP|ACQ426_FIFO_STA_CAL_FAIL;
acq400_sysfs.c:	return sprintf(buf, "%d %08x %s\n", ctrl&ACQ426_ADC_CTRL_CALIB? 1: 0, sta, pass? "PASS": fail?"FAIL": ena? "BUSY": "IDLE");
acq400_sysfs.c:	return acq400_show_hex32(dev, attr, buf, ACQ426_CAL_POINT);
acq400_sysfs.c:	return acq400_show_hex32(dev, attr, buf, ACQ426_CAL_WIN);
acq400_sysfs.c:		}else if (IS_ACQ426(adev)){
```
### unusual to need a dedicated device driver, but perhaps for SPI
```
acq426_drv.c:	printk("D-TACQ ACQ426 Driver %s\n", REVID);
```

### scripts

```
(base) pgm@hoy6:~/PROJECTS/ACQ400/ACQ400DRV$ grep ACQ426 scripts/*
scripts/mod_id.sh:MOD_ID_ACQ426ELF=C

```

### CARE scripts (maybe) :

```
(base) pgm@hoy6:~/PROJECTS/ACQ400/ACQ400DRV$ grep ACQ426 CARE/*
CARE/acq426_calibrate_links:ACQ426_BOOT_CLK_TIMEOUT=${ACQ426_BOOT_CLK_TIMEOUT:-10}
CARE/acq426_calibrate_links:	if [ $CTRIES -gt $ACQ426_BOOT_CLK_TIMEOUT ]; then
CARE/acq426.init:ACQ426_DEFSPAN=${ACQ426_DEFSPAN:-M10-10}
CARE/acq426.init:/sbin/insmod $MODFILE ${ACQ426_PARAMS}
CARE/acq426.init:                        echo $ACQ426_DEFSPAN > $span
CARE/acq426.init:        echo ${ACQ426_DATA32:-0} > $dk/data32
grep: CARE/ATD-DSP: Is a directory
CARE/choose_vap:		ACQ426*)
CARE/choose_vap:			echo "# $PN: ACQ426 DETECTED set VRANGE 15V"
```
### init 

Probably:
```
(base) pgm@hoy6:~/PROJECTS/ACQ400/ACQ400DRV$ grep -i ACQ426 init/*
init/acq420.init:ACQ426_SITES=""
init/acq420.init:    ${MOD_ID_ACQ426ELF})
init/acq420.init:        ACQ426_SITES="${ACQ426_SITES} $site";;
...
init/acq420.init:        $acq426_init ${ACQ426_SITES}
```

# acq400ioc

```
acq400ioc$ git log | grep -A3 -B3 -i ACQ426
...
# HOURS OF FUN, start with:
```
git log | grep -i -B3 ACQ426
```
## Important, Always
Author: Peter Milne <peter.milne@d-tacq.com>
Date:   Mon Oct 21 08:29:05 2024 +0100

    acq426 intro. nothing to see yet.
     On branch acq426
            modified:   scripts/load.records
            modified:   scripts/mtype.sh
```
## other interesting files, ymmv
```
(base) pgm@hoy6:~/PROJECTS/ACQ400/acq400ioc$ git ls-files | grep -i acq426
db/acq426.db
protocols/acq426.proto
protocols/acq426ER.proto
```

