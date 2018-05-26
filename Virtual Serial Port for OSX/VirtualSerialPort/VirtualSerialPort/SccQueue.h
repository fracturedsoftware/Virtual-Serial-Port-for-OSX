/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef __SCCQUEUE__
#define __SCCQUEUE__

#include "sys/types.h"

typedef struct CirQueue{
    UInt8	*Start;
    UInt8	*End;
    UInt8	*NextChar;
    UInt8	*LastChar;
    UInt32	Size;
    UInt32	InQueue;
}CirQueue;

typedef enum QueueStatus{
    queueNoError = 0,
    queueFull,
    queueEmpty,
    queueMaxStatus
}QueueStatus;

QueueStatus	InitQueue(CirQueue *Queue, UInt8 *Buffer, UInt32 Size);
QueueStatus	CloseQueue(CirQueue *Queue);
void		ResetQueue(CirQueue *Queue);
UInt32		AddtoQueue(CirQueue *Queue, UInt8 *Buffer, UInt32 Size);
UInt32		RemovefromQueue(CirQueue *Queue, UInt8	*Buffer, UInt32 MaxSize);
UInt32		FreeSpaceinQueue(CirQueue *Queue);
UInt32		UsedSpaceinQueue(CirQueue *Queue);
UInt32		GetQueueSize( CirQueue *Queue);
QueueStatus	AddBytetoQueue(CirQueue *Queue, char Value);
QueueStatus	GetBytetoQueue(CirQueue *Queue, UInt8 *Value);
QueueStatus GetQueueStatus(CirQueue *Queue);
UInt8*		BeginDirectReadFromQueue(CirQueue *Queue, UInt32 *size, bool *queueWrapped);
void		EndDirectReadFromQueue(CirQueue *Queue, UInt32 size);

#endif