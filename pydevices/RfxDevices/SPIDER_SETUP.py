from MDSplus import mdsExceptions, Device

class SPIDER_SETUP(Device):
     """SPIDER Experiment Setup"""
     parts=[{'path':':COMMENT', 'type':'text'},

      {'path':'.ISEPS', 'type':'structure'},
      {'path':'.ISEPS:CAEN_FREQ', 'type':'numeric', 'value':2},
      {'path':'.ISEPS:CAEN_START', 'type':'numeric', 'value':0},
      {'path':'.ISEPS:CAEN_DURAT', 'type':'numeric', 'value':30},
      {'path':'.ISEPS:NI6259_FREQ', 'type':'numeric', 'value':10000},
      {'path':'.ISEPS:NI6259_START', 'type':'numeric', 'value':0},
      {'path':'.ISEPS:NI6259_DURAT', 'type':'numeric', 'value':60},
      {'path':'.ISEPS:NI6368_FREQ', 'type':'numeric', 'value':10000},
      {'path':'.ISEPS:NI6368_START', 'type':'numeric', 'value':0},
      {'path':'.ISEPS:NI6368_DURAT', 'type':'numeric', 'value':60},
      {'path':'.ISEPS:BREAK_DEAD', 'type':'numeric', 'value':10},
      {'path':'.ISEPS:BREAK_REC', 'type':'numeric', 'value':0}]

     for i in range(0,8):
           parts.append({'path':'.ISEPS.WAVE_%d'%(i+1), 'type':'structure'})
           parts.append({'path':'.ISEPS.WAVE_%d:WAVE'%(i+1), 'type':'signal'})
           parts.append({'path':'.ISEPS.WAVE_%d:MIN_X'%(i+1), 'type':'numeric'})
           parts.append({'path':'.ISEPS.WAVE_%d:MAX_X'%(i+1), 'type':'numeric'})
           parts.append({'path':'.ISEPS.WAVE_%d:MIN_Y'%(i+1), 'type':'numeric'})
           parts.append({'path':'.ISEPS.WAVE_%d:MAX_Y'%(i+1), 'type':'numeric'})

     parts.append({'path':'.ISEPS.WAVE_REC', 'type':'structure'})
     parts.append({'path':'.ISEPS.WAVE_REC:WAVE', 'type':'signal'})
     parts.append({'path':'.ISEPS.WAVE_REC:MIN_X', 'type':'numeric'})
     parts.append({'path':'.ISEPS.WAVE_REC:MAX_X', 'type':'numeric'})
     parts.append({'path':'.ISEPS.WAVE_REC:MIN_Y', 'type':'numeric'})
     parts.append({'path':'.ISEPS.WAVE_REC:MAX_Y', 'type':'numeric'})


     parts.append({'path':'.GVS', 'type':'structure'})
     parts.append({'path':'.GVS:TRIG_SOURCE', 'type':'numeric'})

     for i in range(0,8):
           parts.append({'path':'.GVS.WAVE_%d'%(i+1), 'type':'structure'})
           parts.append({'path':'.GVS.WAVE_%d:WAVE'%(i+1), 'type':'signal'})
           parts.append({'path':'.GVS.WAVE_%d:MIN_X'%(i+1), 'type':'numeric'})
           parts.append({'path':'.GVS.WAVE_%d:MAX_X'%(i+1), 'type':'numeric'})
           parts.append({'path':'.GVS.WAVE_%d:MIN_Y'%(i+1), 'type':'numeric'})
           parts.append({'path':'.GVS.WAVE_%d:MAX_Y'%(i+1), 'type':'numeric'})

     parts.append({'path':'.SPIDER', 'type':'structure'})
     parts.append({'path':'.SPIDER:T_START_SP', 'type':'numeric', 'value':0})
     parts.append({'path':'.SPIDER:BON_START', 'type':'numeric', 'value':0})
     parts.append({'path':'.SPIDER:BON_STOP', 'type':'numeric', 'value':1})
     parts.append({'path':'.SPIDER:WREF_START', 'type':'numeric', 'value':0})
     parts.append({'path':'.SPIDER:WREF_STOP', 'type':'numeric', 'value':0})


