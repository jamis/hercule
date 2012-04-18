#ifndef __PROGRESS_DLG_H__
#define __PROGRESS_DLG_H__

#include <PalmOS.h>

FormPtr OpenProgressDlg( const Char* label );
void    CloseProgressDlg( FormPtr progress, UInt16 delay );

void    UpdateProgressDlg( FormPtr progress, UInt8 percent );

#endif /* __PROGRESS_DLG_H__ */
