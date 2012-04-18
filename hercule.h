#ifndef __SENTRY_hercule__H
#define __SENTRY_hercule__H

#define GETOBJECT( frm, objId ) FrmGetObjectPtr( frm, FrmGetObjectIndex( frm, objId ) )

#define DBTYPE             'DATA'
#define DBID               'JBlp'

#define PUZZLE_ROWS        4
#define PUZZLE_COLS        5

#define formPuzzleRedraw   ( 14 )

extern MemHandle g_puzzle;

void msg_box( Char* fmt, ... );

#endif /* __SENTRY_hercule__H */
