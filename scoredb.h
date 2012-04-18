#ifndef __SCOREDB_H__
#define __SCOREDB_H__

typedef struct {
  UInt32  date;
  UInt32  seed;
  UInt16  seconds;
  UInt8   handicap;
  Boolean auto_deduce;
  Boolean warn_on_bad;
  UInt16  warnings;
  UInt16  undos;
  UInt16  hints;
  Char    player[ 4 ];
} SCORE_RECORD;


DmOpenRef OpenScoreDB( void );
void      CloseScoreDB( DmOpenRef scoreDB );

Boolean   ScoreInTop( DmOpenRef scoreDB, SCORE_RECORD* score, Int16 topScoreCount );
Int16     AddScore( DmOpenRef scoreDB, SCORE_RECORD* score, Int16 topScoreCount, UInt16* rank );
void      ClearScores( DmOpenRef scoreDB );

#endif /* __SCOREDB_H__ */
