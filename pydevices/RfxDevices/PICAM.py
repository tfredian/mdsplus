# 
# Copyright (c) 2017, Massachusetts Institute of Technology All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice, this
# list of conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

from MDSplus import mdsExceptions, Device, Data, Range, Dimension, Window, Int32, Float32, Float64, Float32Array
from threading import Thread
from ctypes import CDLL, byref, c_int, c_void_p, c_byte, c_float, c_char_p
import os
import time
import sys, traceback

class PICAM(Device):
    """Princeton Instrument CCD camera"""
    parts=[{'path':':MODEL_ID', 'type':'numeric', 'value':0},
        {'path':':SERIAL_NUM', 'type':'numeric', 'value':0},
        {'path':':COMMENT', 'type':'text'},
        {'path':':CLOCK_MODE', 'type':'text', 'value':'INTERNAL'},
        {'path':':CLOCK_FREQ', 'type':'numeric', 'value':100},
        {'path':':CLOCK_START', 'type':'numeric'},
        {'path':':CLOCK_DURAT', 'type':'numeric'},
        {'path':':CLOCK_SOURCE', 'type':'numeric'},
        {'path':':EXP_TIME', 'type':'numeric', 'value':100},
        {'path':':TEMPERTURE', 'type':'numeric', 'value':-70},
        {'path':':SHUTTER_MODE', 'type':'text', 'value':'NORMAL'},
        {'path':':READOUT_TIME', 'type':'numeric'},
        {'path':':CLEANS', 'type':'numeric'},
        {'path':':SKIP', 'type':'numeric'},
        {'path':':CHIP_OR', 'type':'text','value':'NORMAL'},
        {'path':':ADC', 'type':'text','value':'SLOW'},
        {'path':'.SPECTROMETER', 'type':'structure'},
        {'path':'.SPECTROMETER:GRATING', 'type':'numeric', 'value':1},
        {'path':'.SPECTROMETER:LAMBDA', 'type':'numeric'},
        {'path':'.SPECTROMETER:SLIT', 'type':'numeric'}]

    for i in range(0,24):
        parts.append({'path':'.SPECTRUM_%02d'%(i+1), 'type':'structure'})
        parts.append({'path':'.SPECTRUM_%02d:NAME'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d:ROI'%(i+1), 'type':'structure'})
        parts.append({'path':'.SPECTRUM_%02d.ROI:START_X'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d.ROI:END_X'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d.ROI:BIN_X'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d.ROI:START_Y'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d.ROI:END_Y'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d.ROI:BIN_Y'%(i+1), 'type':'text'})
        parts.append({'path':'.SPECTRUM_%02d:DATA'%(i+1), 'type':'signal', 'options':('no_write_model', 'no_compress_on_put')  })
    del(i)

    parts.append({'path':':INIT_ACTION','type':'action',
        'valueExpr':"Action(Dispatch('CPCI_SERVER','PULSE_PREPARATION',50,None),Method(None,'init',head))",
        'options':('no_write_shot',)})
    parts.append({'path':':START_ACTION','type':'action',
        'valueExpr':"Action(Dispatch('PXI_SERVER','INIT',50,None),Method(None,'start_store',head))",
        'options':('no_write_shot',)})
    parts.append({'path':':STOP_ACTION','type':'action',
        'valueExpr':"Action(Dispatch('PXI_SERVER','FINISH_SHOT',50,None),Method(None,'stop_store',head))",
        'options':('no_write_shot',)})


