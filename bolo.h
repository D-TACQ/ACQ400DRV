/* ------------------------------------------------------------------------- *
 * bolo.h  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 8 Apr 2014  
 *    Author: pgm                                                         
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

#ifndef BOLO_H_
#define BOLO_H_

#define B8_AWG_MEM 0x2000

#define B8_MOD_ID 0x0000
#define B8_SYS_CON 0x0004
#define B8_TIM_CON 0x0008

#define B8_INT_CSR 0x0018
#define B8_CLK_CTR 0x001C
#define B8_CLKDIV 0x0040

#define B8_ADC_CON 0x0104
#define B8_ADC_HITIDE 0x010C
#define B8_ADC_FIFO_CNT 0x0110
#define B8_ADC_FIFO_STA 0x0114
#define B8_ADC_SAMPLE_CNT 0x0120
#define B8_ADC_CONV_TIME 0x014c

#define B8_DAC_CON 0x0204
#define B8_DAC_WAVE_TOP 0x020C
#define B8_DAC_FIFO_STA 0x0214
#define B8_DAC_SAMPLE_CNT 0x0220
#define B8_DAC_SPI 0x0248
#define B8_DAC_SPI_RBK 0x024c

#define B8_DAC_CON_LL 0x00000100 /* 1 */
#define B8_DAC_CON_DS32 0x00000080 /* data size 0:packed 1:unpacked */
#define B8_DAC_CON_ENA 0x00000010 /* 1 */
#define B8_DAC_CON_RST 0x00000008 /* 0 */

#define B8_DAC_CON_INIT (B8_DAC_CON_DS32)

/* Current ADc */
#define B8_CAD_CON 0x0304
#define B8_CAD_DELAY 0x0340
#define B8_CAD_1 0x0380
#define B8_CAD_2 0x0382 /* do not read : hi BYTE */
#define B8_CAD_3 0x0384
#define B8_CAD_4 0x0386 /* do not read : hi BYTE */
#define B8_CAD_5 0x0388
#define B8_CAD_6 0x038A /* do not read : hi BYTE */
#define B8_CAD_7 0x038C
#define B8_CAD_8 0x038E /* do not read : hi BYTE */
#define B8_CAD_A1 0x0390
#define B8_CAD_A2 0x0392 /* do not read : hi BYTE */
#define B8_CAD_A3 0x0394
#define B8_CAD_A4 0x0396 /* do not read : hi BYTE */
#define B8_CAD_A5 0x0398
#define B8_CAD_A6 0x039A /* do not read : hi BYTE */
#define B8_CAD_A7 0x039C
#define B8_CAD_A8 0x039E /* do not read : hi BYTE */

#define B8_CAD_CON_ENABLE (1 << 4)

/* Offset DAc */
#define B8_ODA_CON 0x0404
#define B8_ODA_DATA 0x0448

#define B8_ODA_CON_ENABLE (1 << 4)

#define B8_ODA_DATA_RUN (1 << 31)
#define B8_ODA_DATA_COMPLETE (1 << 30)
#define B8_ODA_DATA_RDnW (1 << 23)
#define B8_ODA_DATA_ADDR_SHL (16)

#define B8_ODA_DATA_ADDR(reg) ((0x8 | (reg)) << B8_ODA_DATA_ADDR_SHL)

#define B8_ADC_CONV_TIME_DEFAULT 0x36

short bolo8_get_offset_dacN(struct acq400_dev *adev, int ix);
void bolo8_set_offset_dacN(struct acq400_dev *adev, int ix, short offset);

extern int bolo8_isFifoError(struct acq400_dev *adev);
extern int bolo_open_awg(struct inode *inode, struct file *file);
extern void bolo8_init_defaults(struct acq400_dev *adev);
#endif /* BOLO_H_ */
