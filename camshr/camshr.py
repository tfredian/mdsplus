class CamShr:
    """
       method: __init__

       arguments:
          name : the name of the CAMAC module to talk to.
                 format "ks{a|b}c:s"
                        a - highway 0
                        b - highway 1
                        c - crate number
                        s - slot number
    """
    def __init__(self,name):
        import ctypes as ct
        self.camshr=ct.CDLL("libKSCamShr.so")
        self.name = name
        self.iosb = 0
        self.CAMPiow = None
        self.CAMBytcnt = None
        self.CAMStopw = None
        self.CAMFStopw = None
        self.CAMGetStat = None
        self.CAMPioQrepw = None
        self.CAMPiow = None
        self.CAMQ = None
        self.CAMQstopw = None
        self.CAMQrepw = None
        self.CAMXandQ = None

    def getData(self):
        return self.data

    def getIosb(self):
        return self.iosb

    """
      Method CamPiow

      Arguments
         a - integer - camac address
         f - integer - camac function code
         data - integer = camac data to be written if write operation
         width - 16/24

      note - use the getData method to retrieve the data read if read operation. 
    """
    def CamPiow(self, a, f, data=None, width=16):
        import ctypes as ct
        if not self.CAMPiow:
            self.CAMPiow=self.camshr.CamPiow
            self.CAMPiow.argtypes=[ct.c_char_p, ct.c_int, ct.c_int, ct.c_void_p, ct.c_int, ct.c_ulonglong]
        if not data:
            data = 0
        if type(data) == int :
            l_data = ct.c_int(data)
        elif type(data) == ct.c_int :
            l_data = data
        elif type(data) == list:
            l_data = ct.c_int(data[0])
        else:
            raise Exception("CamPiow unsupported type for data %s"%type(data))
        status = self.CAMPiow(self.name, a, f, ct.byref(l_data), width, self.iosb)
        if type(data) == list:
            data[0] = l_data.value
        else:
            data = l_data.value
        return status, data

    def CamBytcnt(self, iosb=None):
        import ctypes as ct
        if not self.CAMBytcnt:
            self.CAMBytcnt=self.camshr.CamBytcnt
            self.CAMBytcnt.argtypes=[ct.c_ulonglong]
        if iosb:
            status = self.CAMBytcnt(iosb)
        else:
            status = self.CAMBytcnt(self.iosb)
        return status
 
    def CamStopw(self, a, f, num, data, width=16):
        import ctypes as ct
        if not self.CAMStopw:
            self.CAMStopw=self.camshr.CamStopw
            self.CAMStopw.argtypes=[ct.c_char_p, ct.c_int, ct.c_int, ct.c_int, ct.POINTER(ct.c_int), ct.c_int, ct.c_ulonglong]
        return self.CAMStopw(self.name, a, f, num, data, width, self.iosb)

    def CamFStopw(self, name, a, f, num, data, width=16, iosb=None):
        import ctypes as ct
        if not self.CAMFStopw:
            self.CAMFStopw=self.camshr.CamFStopw
            self.CAMFStopw.argtypes=[ct.c_char_p, ct.c_int, t.c_int, ct.c_int, ct.POINTER(ct.c_int), ct.c_int, ct.c_ulonglong]
        status = self.CAMFStopw(name, a, f, num, data, width, self.iosb)
        if iosb:
            iosb = self.iosb
        return status


    def CamGetStat(self, iosb=None):
        import ctypes as ct
        if not self.CAMGetStat:
            self.CAMGetStat=self.camshr.CamGetStat
            self.CAMGetStat.argtypes=[ct.c_ulonglong]
        if iosb:
            status = self.CAMGetStat(iosb)
        else:
            status = self.CAMGetStat(self.iosb)
        return status

    def CamPioQrepw(self, name, a, f, data=0, width=16, iosb=None):
        import ctypes as ct
        if not self.CAMPioQrepw:
            self.CAMPioQrepw=self.camshr.CamPioQrepw
            self.CAMPioQrepw.argtypes=[ct.c_char_p, ct.c_int, ct.c_int, ct.c_void_p, ct.c_int, ct.c_ulonglong]
        status = self.CAMPioQrepw(name, a, f, data, width, self.iosb)
        if iosb:
            iosb = self.iosb
        return status

    def CamQ(self, iosb=None):
        import ctypes as ct
        if not self.CAMQ:
            self.CAMQ=self.camshr.CamQ
            self.CAMQ.argtypes=[ct.c_ulonglong]
        if iosb:
            status = self.CAMQ(iosb)
        else:
            status = self.CAMQ(self.iosb)
        return status

    def CamXandQ(self, iosb=None):
        import ctypes as ct
        if not self.CAMXandQ:
            self.CAMXandQ=self.camshr.CamXandQ
            self.CAMXandQ.argtypes=[ct.c_ulonglong]
        if iosb:
            status = self.CAMXandQ(iosb)
        else:
            status = self.CAMXandQ(self.iosb)
        return status

    def CamQstopw(self, a, f, num, width=16, iosb=None):
        import ctypes as ct
        if not self.CAMQstopw:
            self.CAMQstopw=self.camshr.CamQstopw
            self.CAMQstopw.argtypes=[ct.c_char_p, ct.c_int, ct.c_int, ct.c_int, ct.POINTER(ct.c_ushort), ct.c_int, ct.c_ulonglong]
        data = (ct.c_ushort*num)()
        d = ct.cast(data, ct.POINTER(ct.c_ushort))
        status = self.CAMQstopw(self.name, a, f, num, d, width, self.iosb)
        if iosb:
            iosb = self.iosb
        return status, data

    def CamQrepw(self, name, a, f, num, width=16, iosb=None):
        import ctypes as ct
        if not self.CAMQrepw:
            self.CAMQrepw=self.camshr.CamQrepw
            self.CAMQrepw.argtypes=[ct.c_char_p, ct.c_int, t.c_int, ct.c_int, ct.c_void_p, ct.c_int, ct.c_ulonglong]
        data = c_int*num
        status = self.CAMQrepw(name, a, f, num, data, width, self.iosb)
        if iosb:
            iosb = self.iosb
        return status, data

#
# * Dummy entry points for unimplemented routines
#
# define dummyRtn(name) extern int name() { printf("kscamshr %s not implimented\n","name");return(1);}

    def CamAssign(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)

    def CamFQrepw(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)

    def CamGetMAXBUF(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)

    def CamQScanw(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)

    def CamSetMAXBUF(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)

    def CamStat(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)

    def CamVerbose(self, *args, **kwargs):
        print "%s not implimented"%(__name__,)
