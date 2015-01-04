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
        {'path':':TRIG_SRC','type':'numeric','options':('no_write_shot',)},
        {'path':':MASTER','type':'NUMERIC','value':1,'options':('no_write_shot',)},
        {'path':':PRE_TRIGGER','type':'NUMERIC','value':0,'options':('no_write_shot',)},
        {'path':':GAIN','type':'NUMERIC','value':1,'options':('no_write_shot',)},
        {'path':':OFFSET','type':'NUMERIC','value':0.0,'options':('no_write_shot',)},
        {'path':':CLOCK_CODE','type':'NUMERIC','value':8,'options':('no_write_shot',)},
        {'path':':CLOCK_SRC','type':'numeric','options':('no_write_shot',)},
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

#
# bit masks in registers
#
    EOR = 1 << 15
    CLOCK_SEL = 15 << 8
    PRE_TRIG = 1 << 12
    MAX_SAMPLES = 32767
    GAIN = 3 << 7
#
# clock codes
# 
    DELTA_TS = (1E-7, 1.5E-7, 2E-7, 2.5E-7, 3E-7, 3.5E-7, 4E-7, 0., 1E-6,  1.5E-6, 2E-6, 2.5E-6, 3E-6, 3.5E-6, 4E-6, 0.) 
#
#  ACQ specific error codes
#  defined in includes/mitdevices_msg.h
#  - FIX THESE !!
    DEV_BAD_GAIN = 0x277C8022
    DEV_BAD_OFFSET = 277C8052
    DEV_NOT_TRIGGERED = 0x277c8062
#
    debug=None
    cam=None

    """
	Method trigger

	Soft trigger the board by sending an F25 A0 to the module
        Specified by :name
    """
    def trigger(self, arg):
        import camshr
	if not self.cam:
	    self.cam= camshr.CamShr(str(self.name.record))
	self.cam.CamPiow(0,25)
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
        from math import log
        if not self.cam:
            self.cam= camshr.CamShr(str(self.name.record))
        # reset the module
        self.cam.CamPiow(0,24)

        # set the gain and offset
        offset = max(min(float(self.gain.record), 5.1),0)
        register=int(offset/.02)
        gain = int(self.gain.record)
	if gain not in (1,2,4,8):
	    return self.DEV_BAD_GAIN
        register |= int(log(gain, 2)) << 8
        self.cam.CamPiow(0,17, data=register)

        # set the status register
	register=0
        if bool(self.master.record):
            register |= 1 << 15
        pre_trig = int(self.pre_trigger.record)
        if pre_trig != 0:
            register |= 1 << self.PRE_TRIG
            register |=  (pre_trig/128) 
        register |= int(self.clock_code.record) << 8
        print 'clock code', int(self.clock_code.record), 'shifted ',  int(self.clock_code.record) << 8, "register ", register
        self.cam.CamPiow(0,16, data=register)

        # arm the module
        self.cam.CamPiow(1,26)
        return 1

    INIT=init

    def store(self, arg):
        import camshr
        import MDSplus
        if not self.cam:
            self.cam= camshr.CamShr(str(self.name.record))
        self.cam.CamPiow(0,27)
	if not self.cam.CamQ():
            return self.DEV_NOT_TRIGGERED
        status,status_reg = self.cam.CamPiow(0,0)
        print "status = %d, reg=%d, EOF=%d"%(status, status_reg, self.EOR)
        if (status_reg & self.EOR) == 0:
            return self.DEV_NOT_TRIGGERED
        delta_t = self.DELTA_TS[(status_reg & self.CLOCK_SEL) >> 8]
        print "status_reg", status_reg, "clock bits", status_reg & self.CLOCK_SEL, 'clock bits shifted', (status_reg & self.CLOCK_SEL >> 8), 'delta t', self.DELTA_TS[(status_reg & self.CLOCK_SEL) >> 8]
        if not delta_t == 0:
            self.clock_out.record = MDSplus.Range(None, None, delta_t)
        else :
            self.clock_out.record = self.clock_src
	if not status_reg & self.PRE_TRIG == 0:
            preTrig = status_reg & 0xFF*128
            status, starting_addr = self.cam.CamPiow(1,1)
        else:
            preTrig = 0
            starting_addr = 0
        postTrig = self.MAX_SAMPLES - preTrig
        status, gain_reg = self.cam.CamPiow(0,1)
        gain = 2**((gain_reg & self.GAIN) >> 7)
        print "gain is ", gain
        self.act_gain.record=gain
        offset = gain_reg & 0xFF
        print "offset is ", offset
        self.act_offset.record = offset
        
        starting_addr, status = self.cam.CamPiow(1,1)
        act_gain = self.__getattr__('act_gain')
        act_offset = self.__getattr__('act_offset')
        for chan in range(4):
	    chan_node = self.__getattr__('input_%1.1d' % (chan+1,))
            if chan_node.on :
                try:
                    start = max(int(self.__getattr__('input_%1.1d_startidx'%(chan+1,))),-preTrig)
                except:
                    start = -preTrig
                try:
                    end = min(int(self.__getattr__('input_%1.1d_endidx'%(chan+1,))),postTrig-1)
                except:
                    end = postTrig-1
                self.cam.CamPiow(1, 17, data=starting_addr)
                status,buf = self.cam.CamQstopw(chan, 2, 32768)
                dim = MDSplus.Dimension(MDSplus.Window(start, end, self.trig_src), self.clock_out)
                dat = MDSplus.Data.compile(
                        'build_signal(build_with_units(($value-$)*.02/$, "Volts"), build_with_units($, "Counts"), build_with_units($,"seconds"))', act_offset, act_gain, buf, dim)
                exec('c=self.input_'+'%1d'%(chan+1,)+'.record=dat')
        return 1

    STORE=store
