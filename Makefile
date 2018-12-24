include make.inc

#CFLAGS = -O2 -Wall -Wno-unknown-pragmas `palm-config --cflags`
#CFLAGS = -O2 -Wall -Wno-unknown-pragmas -palmos5.0
CFLAGS = -O2 -Wall -Wno-unknown-pragmas
#CFLAGS = -g -Wall -Wno-unknown-pragmas -palmos4.0

all: registered unregistered registered_color unregistered_color

registered: hercule.prc
unregistered: hercule_unreg.prc
registered_color: hercule_c.prc
unregistered_color: hercule_c_unreg.prc

hercule.prc: reg/.intermediate resources_reg
	build-prc -o hercule.prc hercule.def reg/.intermediate *.bin
	rm *.bin

hercule_c.prc: *.c *.h
	make -f Makefile.color reg

hercule_unreg.prc: unreg/.intermediate resources_unreg
	build-prc -o hercule_unreg.prc hercule.def unreg/.intermediate *.bin
	rm *.bin

hercule_c_unreg.prc: *.c *.h
	make -f Makefile.color unreg

reg/.intermediate: reg/hercule.o $(GENERAL_OBJS) hercule-sections.ld
	$(CC) $(CFLAGS) -o reg/.intermediate reg/hercule.o $(GENERAL_OBJS) hercule-sections.ld

unreg/.intermediate: unreg/hercule.o $(GENERAL_OBJS) hercule-sections.ld
	$(CC) $(CFLAGS) -o unreg/.intermediate unreg/hercule.o $(GENERAL_OBJS) hercule-sections.ld

reg/hercule.o: hercule.c herculeRsc.h hercule_engine_palm.h progress_dlg.h scoredb.h
	$(CC) $(CFLAGS) -c -o reg/hercule.o -DREGISTERED hercule.c

unreg/hercule.o: hercule.c herculeRsc.h hercule_engine_palm.h progress_dlg.h scoredb.h
	$(CC) $(CFLAGS) -c -o unreg/hercule.o -DUNREGISTERED hercule.c


hercule_engine_palm.o: hercule_engine_palm.c hercule_engine_palm.h progress_dlg.h
progress_dlg.o: progress_dlg.h progress_dlg.c
scoredb.o: scoredb.h hercule.h scoredb.c
hercule_event.o: hercule_event.h hercule.h hercule_event.c
hercule_sortdlg.o: hercule_sortdlg.h hercule.h hercule_sortdlg.c
hercule_tilesets.o: hercule_tilesets.h hercule.h hercule_tilesets.c hercule_event.h

resources_reg: hercule.rcp hercule_monochrome.rcp hercule_reg.rcp herculeRsc.h
	cat hercule.rcp hercule_monochrome.rcp hercule_reg.rcp > temp.rcp
	$(PILRC) -q -I . temp.rcp
	rm temp.rcp

resources_unreg: hercule.rcp hercule_monochrome.rcp hercule_unreg.rcp herculeRsc.h
	cat hercule.rcp hercule_monochrome.rcp hercule_unreg.rcp > temp.rcp
	$(PILRC) -q -I . temp.rcp
	rm temp.rcp

clean:
	-rm -f reg/.intermediate* unreg/.intermediate* *.o *.bin reg/*.o unreg/*.o *.bin *.s *.ld

realclean: clean
	-rm -f *.prc
