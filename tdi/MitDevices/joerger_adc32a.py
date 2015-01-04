import os
import numpy
import array
import ftplib
import tempfile
import socket

import MDSplus


class JOERGER_ADC32A(MDSplus.Device):
    """
    class to support Joerger model ADC32A CAMAC Digitizer.

    Author:  Joshua Stillerman
    Date:    10/2014

    init() - Arm the module
        gain for each channel (1,2,4,8)
        offset for all channels (0..255)->05.1V
        f24 a0  (reset)

    trigger() - Soft trigger the module
    store()  - Store the data from the module

    
    """
    
    parts=[
        {'path':':NAME','type':'text','value':'ksa2:5'},
        {'path':':COMMENT','type':'TEXT'},
        {'path':':ACTIVE_CHAN','type':'numeric','value':32, 'options':('no_write_shot',)},
        {'path':':MAX_SAMPLES','type':'numeric','value':32768, 'options':('no_write_shot',)},
        {'path':':TRIG_SRC','type':'numeric','options':('no_write_shot',)},
        {'path':':CLOCK_SRC','type':'numeric','options':('no_write_shot',)},
        {'path':':INT_CLOCK','type':'AXIS','options':('no_write_model','write_once',)},
        ]
    for i in range(32):
        parts.append({'path':':INPUT_%1.1d'%(i+1,),'type':'SIGNAL','options':('no_write_model','write_once',)})
        parts.append({'path':':INPUT_%2.2d:STARTIDX'%(i+1,),'type':'NUMERIC', 'options':('no_write_shot',)})
        parts.append({'path':':INPUT_%2.2d:ENDIDX'%(i+1,),'type':'NUMERIC', 'options':('no_write_shot',)})
        parts.append({'path':':INPUT_%2.2d:GAIN'%(i+1,),'type':'NUMERIC', 'value':1})

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
    """
    def init(self, arg):
        import camshr
        if not self.cam:
            self.cam= camshr.camshr(str(self.name.record))
        # trigger the module just in case
        self.cam.CamPiow(1,25)

        # read the module id
        status, module_id = cam.CamPiow(0, 3)
        memsize = self.MEM_SIZES[module_id&0x7]

        # reset the module
        self.cam.CamPiow(1,25)

        active_c = int(self.active_chans)
        status_reg = log2(self.active_c)

        active_mem = int(self.max_samples)
        status_reg = status_reg & (log2(active_mem) << 3)
        status_reg = status_reg & self.BURST_MODE
        if self.clock_src.rlength > 0:
            status_reg = status_reg & self.EXT_CLOCK
        status_reg = status_reg & self.SEPARATE_GAIN
        self.cam.CamPiow(0, 16, data=status_reg)

        # set the gain for each channel
        g10 = module_id & self.gain10
        for chan in range(16):
            cgain = int(self.__getattr__('input_%2.2d_gain'%(chan+1,)))
            self.cam.CamPiow(chan, 17, data=log10(cgain) if g10 else log2(cgain))
            cgain = int(self.__getattr__('input_%2.2d_gain'%(chan+17,)))
            self.cam.CamPiow(chan, 20, data=log10(cgain) if g10 else log2(cgain))

        # enable lams
        self.cam.CamPiow(0, 26, data=0)
        return 1

    INIT=init

    def store(self, arg):
        import camshr
        import MDSplus
        if not self.cam:
            self.cam= camshr.camshr(str(self.name.record))
        self.cam.CamPiow(0,8)
	if not self.cam.Q():
            return NotTriggered

        # read the module ID
        status, module_id = cam.CamPiow(0, 3)
        memsize = self.MEM_SIZES[module_id&0x7]

        # read the status register
        status, status_reg = cam.CamPiow(0, 4)
        active_c = 1<<(status_reg & self.ACTIVE_CHANS) 
        active_m = 1<<((satus_reg & self.ACTIVE_MEM) >> 3)
        chan_size = active_m / active_c

        # read all of the data in the module
        samps_read =0
        first = True
        while samps_read < active_m:
            chunk_size =  min(active_m-samps_read, 32768) 
            status, buf = self.cam.CamStopw(0, 2, chunk_size)
            samps_read += chunk_size
            chunk_arr = np.asarray(buf)
            data = chunk_arr if first else np.concatenate(data, chunk_arr)
            first = False

        for chan in range(active_c):
	    chan_node = self.__getattr__('input_%2.2d' % (chan+1,))
            if chan_node.on :
                try:
                    start = max(int(self.__getattr__('input_%1.1d_startidx'%(chan+1,))),-preTrig)
                except:
                    start = -preTrig
                try:
                    end = min(int(self.__getattr__('input_%1.1d_endidx'%(chan+1,))),postTrig-1)
                except:
                    end = postTrig-1
                c_gain = data[chan*chan_size] & self.GAIN_BITS >> 14
                c_data = data[chan::active_chans]


                self.cam.CamPiow(1, 17, data=starting_addr)
                buf,status = self.cam.CamQstopw(chan, 2, 32768)
                dim = MDSplus.Dimension(MDSplus.Window(start, end, self.trig_src), self.clock_out)
                dat = MDSplus.Data.compile(
                        'build_signal(build_with_units(($value-self.act_offset)*.02/self.act_gain, "Volts"), build_with_units($, "Counts"), build_with_units($,"seconds"))', buf, dim)
                exec('c=self.input_'+'%1d'%(chan+1,)+'.record=dat')
        return 1

    STORE=store

    def log2(x):
        import math
        return int(math.log(x,2))
