CC = m68k-palmos-gcc
MULTIGEN=m68k-palmos-multigen
PILRC=pilrc

GENERAL_OBJS=\
	hercule_engine_palm.o \
	progress_dlg.o \
	scoredb.o \
	hercule_event.o \
	hercule_sortdlg.o \
	hercule_tilesets.o \
  hercule-sections.o


hercule-sections.o: hercule-sections.s
	$(CC) -c hercule-sections.s

hercule-sections.s hercule-sections.ld: hercule.def
	$(MULTIGEN) hercule.def

