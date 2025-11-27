/*
 * wrs_trigger.h : White Rabbit Streamers public interface
 *
 *  Created on: 7 Jan 2025
 *      Author: pgm
 */

#ifndef WRS_TRIGGER_H_
#define WRS_TRIGGER_H_
struct wrtd_message;

typedef unsigned u32;

class WRS_Trigger {
protected:
	WRS_Trigger();
public:
	static WRS_Trigger* factory(int site = 0);

	virtual int RX_read(const wrtd_message* message, bool block = true) = 0;
	/* reads current RX packet, block for new message if required */
	virtual int TX_write(wrtd_message* message) = 0;
};
#endif /* WRS_TRIGGER_H_ */ 
