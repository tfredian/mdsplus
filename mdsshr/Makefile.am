
AM_CFLAGS = $(THREAD) @GLOBUS_CFLAGS@ -I/usr/include/libxml2
AM_LDFLAGS = $(THREAD)
LIBS=@LIBS@ @LIBDL@ $(XML_LIBS) @SOCKETLIB@ @NSLLIB@ @CLOCK_GETTIME_LIB@

lib_LIBRARIES = libMdsShr.a

libMdsShr_a_SOURCES = \
        librtl.c \
        mdsmsg.c \
        MDSprintf.c \
        MdsCmprs.c \
        MdsCompareXd.c \
        MdsCompress.c \
        MdsEvents.c \
        MdsGet1DxA.c \
        MdsGet1DxS.c \
        MdsGetSetShotId.c \
        MdsGetStdMsg.c \
        MdsPk.c \
        MdsSerialize.c \
        MdsShrThreadSafe.c \
        MdsXdRoutines.c \
	mds_dsc_string.c \
	MdsFloatToTime.c \
	UdpEvents.c \
	UdpEventSettings.c

if MINGW
DEF = $(srcdir)/MdsShr.def 
XML_LIBS=-Wl,-Bstatic -lxml2 -lz -Wl,-Bdynamic -liconv -ldl

dlldir = $(libdir)
dll_DATA = MdsShr.dll.a
$(dll_DATA): MdsShr.dll
	dlltool --def $(DEF) --dllname $< --output-lib $@

MdsShr.dll:
	$(LINK.c) -o $@ @LINKSHARED@ $(OBJECTS) $(DEF) $(LIBS)

else
XML_LIBS=@XML_LIBS@
dlldir = $(libdir)
dll_SCRIPTS = libMdsShr@SHARETYPE@

libMdsShr$(SHARETYPE): $(libMdsShr_a_OBJECTS)
	$(LINK.c) $(OUTPUT_OPTION) @LINKSHARED@ $^ $(LIBS)

endif

