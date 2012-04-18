#ifndef __HERCULE_EVENT_H__
#define __HERCULE_EVENT_H__

typedef enum {
  HerculeEvent_Timer = firstUserEvent,
  HerculeEvent_SortClues
} HerculeEventTypes;


void HerculeEventInit( void );
void HerculeEventCleanup( void );

void HerculeRegisterForm( UInt16 formId, Boolean (*handler)( EventPtr ) );
void HerculeEventLoop( void );

Boolean HerculeHandleEvent( EventPtr event );

void HerculeStartTimer( Int32 ticks );
void HerculeStopTimer( void );

#endif /* __HERCULE_EVENT_H__ */
