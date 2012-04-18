#ifndef __HERCULE_SORT_DLG_H__
#define __HERCULE_SORT_DLG_H__

typedef struct {
  Boolean auto_sort;
  UInt8   first;
  UInt8   second;
  UInt8   third;
  UInt8   fourth;
} HerculeClueSortInfo;

extern HerculeClueSortInfo g_sort_info;


void HerculeRegisterSortDlg( void );

UInt8* HerculeSortDlgStore( UInt16* size );
void   HerculeSortDlgRead( UInt8** buffer );

#endif /* __HERCULE_SORT_DLG_H__ */
