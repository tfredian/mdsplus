CLASS_S=101
CLASS_D=102
CLASS_A=104
CLASS_XD=106
CLASS_XS=107
CLASS_R=108
CLASS_CA=109
CLASS_APD=110

class mdsclasses:
    mdsclass=-1

    def __init__(self,mdsclass):
        self.mdsclass=mdsclass

    def __str__(self):
        names={CLASS_S:'CLASS_S',CLASS_D:'CLASS_D',CLASS_A:'CLASS_A',CLASS_XS:'CLASS_XD',CLASS_XS:'CLASS_XS',
               CLASS_R:'CLASS_R',CLASS_CA:'CLASS_CA',CLASS_APD:'CLASS_APD'}
        if (self.mdsclass in names):
            return names[self.mdsclass]
        else:
            return "CLASS_%d" % (self.mdsclass,)
