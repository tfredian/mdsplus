AM_CFLAGS = $(TARGET_ARCH)
AM_LDFLAGS = $(TARGET_ARCH)

lib_LIBRARIES = libMdsSql.a
libMdsSql_a_SOURCES =
libMdsSql_a_SOURCES += dbstubs.c
libMdsSql_a_SOURCES += dynamic_syb.c
libMdsSql_a_CPPFLAGS = $(SYBASE_INC)
libMdsSqlLDFLAGS = $(AM_LDFLAGS) $(SYBASE_LIB) $(DNET_STUB) $(LIBSOCKET)

dlldir = $(libdir)
LINKDLL = $(LINK) $^ @LINKSHARED@ $(libMdsSqlLDFLAGS)
if MINGW
dll_SCRIPTS = MdsSql.dll
dll_DATA = MdsSql.dll.a
%.dll %.dll.a: %.def $(libMdsSql_a_OBJECTS)
	$(LINKDLL) -Wl,--out-implib=MdsSql.dll.a
else
dll_SCRIPTS = libMdsSql$(SHARETYPE)
libMdsSql$(SHARETYPE): $(libMdsSql_a_OBJECTS)
	$(LINKDLL)
endif

CLEANFILES = $(dll_SCRIPTS) $(dll_DATA)
