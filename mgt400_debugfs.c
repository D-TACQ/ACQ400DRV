/* ------------------------------------------------------------------------- */
/* acq400_drv.c  D-TACQ ACQ400 FMC  DRIVER   
 *
 * mgt400_debugfs.c
 *
 *  Created on: 13 Jan 2015
 *      Author: pgm
 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2015 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
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

#include "acq400.h"
#include "mgt400.h"

/* @@REMOVEME */
#include "acq400_debugfs_internal.h"

struct dentry* mgt400_debug_root;

#define adev	mdev

void hudp_createDebugfs(struct mgt400_dev* mdev,char* pcursor)
{
	DBG_REG_CREATE_RW(HUDP_CON);
	DBG_REG_CREATE_RW(HUDP_IP_ADDR);
	DBG_REG_CREATE_RW(HUDP_GW_ADDR);
	DBG_REG_CREATE_RW(HUDP_NETMASK);
	DBG_REG_CREATE_RW(HUDP_MAC);
	DBG_REG_CREATE_RW(HUDP_SRC_PORT);
	DBG_REG_CREATE_RW(HUDP_TX_PKT_SZ);
	DBG_REG_CREATE_RW(HUDP_RX_PORT);
	DBG_REG_CREATE_RW(HUDP_RX_SRC_ADDR);
	DBG_REG_CREATE_RW(HUDP_TX_PKT_COUNT);
	DBG_REG_CREATE_RW(HUDP_RX_PKT_COUNT);
	DBG_REG_CREATE_RW(HUDP_DISCO_COUNT);
	DBG_REG_CREATE_RW(HUDP_RX_PKT_LEN);
	DBG_REG_CREATE_RW(HUDP_STATUS);
	DBG_REG_CREATE_RW(HUDP_CALC_PKT_SZ);
	DBG_REG_CREATE(ARP_RESP_MAC_UPPER);
	DBG_REG_CREATE(ARP_RESP_MAC_LOWER);
	DBG_REG_CREATE(UDP_SLICE);
	DBG_REG_CREATE_RW(HUDP_DEST_ADDR);
	DBG_REG_CREATE_RW(HUDP_DEST_PORT);
}

void mgt400_createDebugfs(struct mgt400_dev* mdev)
{

	char* pcursor;
	if (!mgt400_debug_root){
		mgt400_debug_root = debugfs_create_dir("mgt400", 0);
		if (!mgt400_debug_root){
			dev_warn(DEVP(adev), "failed create dir mgt400");
			return;
		}
	}
#if 0
	dev_rc_init(DEVP(adev), &adev->reg_cache,
			adev->dev_virtaddr, adev->of_prams.site, MOD_REG_MAX);
#endif
	pcursor = adev->debug_names = kmalloc(4096, GFP_KERNEL);
	adev->debug_dir = debugfs_create_dir(
			adev->devname, mgt400_debug_root);

	if (!mdev->debug_dir){
		dev_warn(&adev->pdev->dev, "failed create dir %s", adev->devname);
		return;
	}
	DBG_REG_CREATE(MOD_ID);

	if (IS_MGT_HUDP(mdev)){
		return hudp_createDebugfs(mdev, pcursor);
	}
	DBG_REG_CREATE(ZDMA_CR);
	DBG_REG_CREATE_NAME("HEART", HEART);
	DBG_REG_CREATE(AURORA_CR);
	DBG_REG_CREATE(AURORA_SR);
	DBG_REG_CREATE(ZIDENT);
	DBG_REG_CREATE(COMMS_TXB_FSR);
	DBG_REG_CREATE(COMMS_TXB_FCR);
	DBG_REG_CREATE(COMMS_RXB_FSR);
	DBG_REG_CREATE(COMMS_RXB_FCR);
	DBG_REG_CREATE(ASTATS1);
	DBG_REG_CREATE(ASTATS2);
/* read clear, best not debug
	DBG_REG_CREATE(ALAT_AVG);
	DBG_REG_CREATE(ALAT_MIN_MAX);
*/
	if (IS_MGT_DRAM(mdev)){
		DBG_REG_CREATE(MGT_DRAM_STA);
		DBG_REG_CREATE(MGT_DRAM_RX_CNT);
		DBG_REG_CREATE(MGT_DRAM_TX_CNT);
	}

	DBG_REG_CREATE(PCIE_CTRL);
	DBG_REG_CREATE(PCIE_INTR);
	DBG_REG_CREATE(PCI_CSR);
	DBG_REG_CREATE(PCIE_DEV_CSR);
	DBG_REG_CREATE(PCIE_LINK_CSR);
	DBG_REG_CREATE(PCIE_CONF);
	DBG_REG_CREATE(PCIE_BUF_CTRL);
	DBG_REG_CREATE(DMA_TEST);
	DBG_REG_CREATE(DMA_CTRL);
	DBG_REG_CREATE(DMA_FIFO_SR);
	DBG_REG_CREATE(DESC_FIFO_SR);
	DBG_REG_CREATE(DMA_PUSH_DESC_SR);
	DBG_REG_CREATE(DMA_PULL_DESC_SR);
	DBG_REG_CREATE(DMA_PUSH_COUNT_LW);
	DBG_REG_CREATE(DMA_PULL_COUNT_LW);
	DBG_REG_CREATE(DMA_PUSH_DESC_LEN);
	DBG_REG_CREATE(DMA_PULL_DESC_LEN);
#if 0
	dev_rc_finalize(DEVP(adev), &adev->reg_cache, adev->of_prams.site);
#endif
#undef adev
}
void mgt400_removeDebugfs(struct mgt400_dev* adev)
{
	debugfs_remove_recursive(adev->debug_dir);
	kfree(adev->debug_names);
}
