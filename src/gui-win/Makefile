
include ../../Makefile.cnf

CFLAGS += -I../includes $(SDL_CFLAGS)

GUIWINOBJS = opencon.o

all: gui-win.a previous-winicon.o

gui-win.a: $(GUIWINOBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

hatari-winicon.o:
	$(WINDRES) -o previous-winicon.o previous-winicon.rc

clean distclean:
	$(RM) *.o
