/*
  * UAE - The Un*x Amiga Emulator
  *
  * Definitions for accessing cycle counters on a given machine, if possible.
  *
  * Copyright 1997, 1998 Bernd Schmidt
  * Copyright 1999 Brian King - Win32 specific
  */
#ifndef _RPT_H_
#define _RPT_H_

typedef unsigned long frame_time_t;
extern frame_time_t read_processor_time (void);

#endif
