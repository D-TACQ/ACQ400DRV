#!/usr/bin/python
import epics
import time
import numpy as np
import os


hn = os.getenv("IOC_HOST")

fmt = "{}:{}:AI:CAL:ESLO"



def record_cal(cal_pvs):
#    print("record_cal len {}".format(len(cal_pvs)))
    #for pv in cal_pvs:
    #   print(pv)
    caldat = np.array([pv.get()[1:] for pv in cal_pvs])
    flat = caldat.reshape((caldat.size))
    olddat = np.fromfile("/dev/shm/calblob", dtype=np.float32)
    if not np.array_equal(olddat, caldat):
        print("write calblob")
        flat.tofile("/dev/shm/calblob")
        
def onChangeFactory(_cal_pvs, _old_state):
    cal_pvs = _cal_pvs
    old_state = _old_state
            
    def onChange(pvname=None, value=None, char_value=None, **kw):
        nonlocal old_state, cal_pvs
#        print("{}  {} = {}".format(pvname, old_state, value))
        if old_state == 0 and value == 1:
            record_cal(cal_pvs)
        old_state = value
        
    return onChange
        
old_state = "000"
cal_pvs = [] 
    
sitelist_pv = epics.PV("{}:SITELIST".format(hn))
sitelist = [ x.split('=')[0] for x in sitelist_pv.get().split(',')[1:]]

cal_pvs.extend( [ epics.PV("{}:{}:AI:CAL:ESLO".format(hn, site)) for site in sitelist])
cal_pvs.extend( [ epics.PV("{}:{}:AI:CAL:EOFF".format(hn, site)) for site in sitelist])
record_cal(cal_pvs)

state = epics.PV("{}:MODE:TRANS_ACT:STATE".format(hn))
state.add_callback(onChangeFactory(cal_pvs, old_state))

while True:
    time.sleep(100)

