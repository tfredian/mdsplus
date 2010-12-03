from _mdsshr import MdsBigDescriptors
if MdsBigDescriptors:
    from _descriptor64 import *
else:
    from _descriptor32 import *
