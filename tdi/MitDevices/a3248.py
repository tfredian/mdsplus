import os
import numpy
import array
import ftplib
import tempfile
import socket

import MDSplus


class A3248(MDSplus.Device):
    """
    class to support Aeon model 3248 CAMAC Digitizer.

    Author:  Joshua Stillerman
    Date:    10/2014

    init() - Arm the module
        gain for all channels (1,2,4,8)
        offset for all channels (0..255)->05.1V
        f24 a0  (reset)

    trigger() - Soft trigger the module
    store()  - Store the data from the module

    
    """
    
    parts=[
        {'path':':NAME','type':'text','value':'ksa2:5'},
        {'path':':COMMENT','type':'TEXT'},
        {'path':':MASTER','type':'NUMERIC','value':1,'options':('no_write_shot',)},
        {'path':':PRE_TRIGGER','type':'NUMERIC','value':0,'options':('no_write_shot',)},
        {'path':':GAIN','type':'NUMERIC','value':1,'options':('no_write_shot',)},
        {'path':':OFFSET','type':'NUMERIC','value':0.0,'options':('no_write_shot',)},
        {'path':':CLOCK_CODE','type':'NUMERIC','value':8,'options':('no_write_shot',)},
        {'path':':CLOCK_OUT','type':'AXIS','options':('no_write_model','write_once',)},
        {'path':':ACT_PRE_TRIG','type':'NUMERIC','options':('no_write_model','write_once',)},
        {'path':':ACT_GAIN','type':'NUMERIC','options':('no_write_model','write_once',)},
        {'path':':ACT_OFFSET','type':'NUMERIC','options':('no_write_model','write_once',)},
        ]
    for i in range(4):
        parts.append({'path':':INPUT_%1.1d'%(i+1,),'type':'SIGNAL','options':('no_write_model','write_once',)})
        parts.append({'path':':INPUT_%1.1d:STARTIDX'%(i+1,),'type':'NUMERIC', 'options':('no_write_shot',)})
        parts.append({'path':':INPUT_%1.1d:ENDIDX'%(i+1,),'type':'NUMERIC', 'options':('no_write_shot',)})

    parts.append({'path':':INIT_ACTION','type':'ACTION',
                 'valueExpr':"Action(Dispatch('CAMAC_SERVER','INIT',50,None),Method(None,'INIT',head))",
                 'options':('no_write_shot',)})
    parts.append({'path':':STORE_ACTION','type':'ACTION',
                 'valueExpr':"Action(Dispatch('CAMAC_SERVER','STORE',50,None),Method(None,'STORE',head))",
                 'options':('no_write_shot',)})

    EOR = 1 << 14
    CLOCK_SEL = 15 << 8
#
#  ACQ specific error codes
#  defined in includes/mitdevices_msg.h
#  - FIX THESE !!
    DEV_BAD_GAIN = 0x277CA5B2
    DEV_BAD_OFFSET = 0x277CA5B2
    debug=None

    """
	Method trigger

	Soft trigger the board by sending an F25 A0 to the module
        Specified by :name
        Using the settings from:
             :MASTER  1 - Master  0 - Slave
             :PRE_TRIGGER 0 -> 32767  (rounded to the previous multiple of 128)
             :GAIN  - 1,2,4,8
             :OFFSET  - 0->5.1 V  (rounded to nearest .02)
             :CLOCK_CODE - 0 - 15
                0 - 10   MHz
                1 - 6.67 MHz
                2 - 5    MHz
                3 - 4    MHz
                4 - 3.33 MHz
                5 - 2.86 MHz
                6 - 2.5  MHz
                7 - External Clock
                8 - 1    MHz
                9 - 667  KHz
               10 - 500  KHz
               11 - 400  KHz
               12 - 333  KHz
               13 - 286  KHz
               14 - 250  KHz
               15 - External Clock
    """
    def trigger(self, arg):
        import camshr
	if not self.cam:
	    self.cam= camshr.camshr(str(self.name.record))
	cam.CamPiow(0,25)
        return 1
    TRIGGER=trigger

    """
        Method init

        Arm the digitizer Specified by :name
        Using the settings from:
             :MASTER  1 - Master  0 - Slave
             :PRE_TRIGGER 0 -> 32767  (rounded to the previous multiple of 128)
             :GAIN  - 1,2,4,8
             :OFFSET - 0->5.1 V  (rounded to nearest .02)
             :CLOCK_CODE - 0 - 15
                0 - 10   MHz
                1 - 6.67 MHz
                2 - 5    MHz
                3 - 4    MHz
                4 - 3.33 MHz
                5 - 2.86 MHz
                6 - 2.5  MHz
                7 - External Clock
                8 - 1    MHz
                9 - 667  KHz
               10 - 500  KHz
               11 - 400  KHz
               12 - 333  KHz
               13 - 286  KHz
               14 - 250  KHz
               15 - External Clock
    """
    def init(self, arg):
        import camshr
        if not self.cam:
            self.cam= camshr.camshr(str(self.name.record))
        # reset the module
        self.cam.CamPiow(0,24)

        # set the gain and offset
        offset = max(min(float(self.gain.record), 5.1),0)
        register=int(offset/.02)
	if gain not in (1,2,4,8):
	    return self.DEV_BAD_GAIN
        register |= log2(gain) << 8
        self.cam.CamPiow(0,17, data=register)

        # set the status register
	register=0
        if bool(self.master.record):
            register |= 1 << 15
        pre_trig = int(self.pre_trig.record)
        if pre_trig != 0:
            register |= 1 << 12
            register |=  (pre_trig/128) 
        register |= int(self.clock_code) << 8
        self.cam.CamPiow(0,16, data=register)

        # arm the module
        self.cam.CamPiow(1,26)
        return 1

    INIT=init

    def store(self, arg):
        import camshr
        if not self.cam:
            self.cam= camshr.camshr(str(self.name.record))
        self.cam.CamPiow(0,27)
	if not self.cam.Q():
            return NotTriggered
        status_reg,status = self.cam.CamPiow(0,0)
        if not ((status_reg & self.EOR) == 0):
            return NotTriggered
        clock = status_reg & self.CLOCK_SEL >> 8
        gain_reg, status = self.cam.CamPiow(1,0)
        offset = gain_reg & 0xFF
        gain = (gain_reg & self.GAIN) >> 8
        starting_addr, status = self.cam.CamPiow(1,1)
        for chan in range(4):
            buf,status = self.cam.CamQstopw(chan, 2, 32768)
            self.store_channel(chan, buf, gain, offset, pre, clock)
        return 1

    STORE=store

    def log2(x):
        import math
        return int(math.log(x,2))

    def store_channel(self, chan, buf, gain, offset, pre, clock):
        print "store channel\n\tchan=%d\n\tgain=%d\n\t\offset=%d\n\tpre=%d\n\tclock=%d\n"%(chan, gain, offset, pre, clock)

    def debugging(self):
	if self.debug == None :
            self.debug=os.getenv("DEBUG_DEVICES")
	return(self.debug)

#    def storeChannel(self, chan, chanMask, preTrig, postTrig, clock, vins):
#        if self.debugging():
#            print "working on channel %d" % chan
#        chan_node = self.__getattr__('input_%2.2d' % (chan+1,))
#        if chan_node.on :
#            if self.debugging():
#                print "it is on so ..."
#            if chanMask[chan:chan+1] == '1' :
#                try:
#                    start = max(int(self.__getattr__('input_%2.2d_startidx'%(chan+1,))),-preTrig)
#                except:
#                    start = -preTrig
#                try:
#                    end = min(int(self.__getattr__('input_%2.2d_endidx'%(chan+1,))),postTrig-1)
#                except:
#                    end = postTrig-1
#                try:
#                    inc = max(int(self.__getattr__('input_%2.2d_inc'%(chan+1,))),1)
#                except:
#                    inc = 1
#
# could do the coeffs
#
#
#                if self.debugging():
#                    print "about to readRawData(%d, preTrig=%d, start=%d, end=%d, inc=%d)" % (chan+1, preTrig, start, end, inc)
#                try:
#                    buf = self.readRawData(chan+1, preTrig, start, end, inc, False)
#                    if self.debugging():
#                        print "readRawData returned %s\n" % (type(buf),)
#                    if inc == 1:
#                        dim = MDSplus.Dimension(MDSplus.Window(start, end, self.trig_src ), clock)
#                    else:
#                        dim = MDSplus.Dimension(MDSplus.Window(start/inc, end/inc, self.trig_src ), MDSplus.Range(None, None, clock.evaluate().getDelta()*inc))
#                    dat = MDSplus.Data.compile(
#                        'build_signal(build_with_units((($1+ ($2-$1)*($value - -32768)/(32767 - -32768 ))), "V") ,build_with_units($3,"Counts"),$4)',
#                        vins[chan*2], vins[chan*2+1], buf,dim) 
#                    exec('c=self.input_'+'%02d'%(chan+1,)+'.record=dat')
#                except Exception, e:
#                    print "error processing channel %d\n%s\n" %(chan+1, e,)
#

    def trigger(self, arg):
        if self.debugging():
            print "starting trigger"
        if not self.cam:
            self.cam= camshr.camshr(str(self.name.record))
        # trigger the module
        self.cam.CamPiow(0,25)
        return 1

    TRIGGER=trigger
