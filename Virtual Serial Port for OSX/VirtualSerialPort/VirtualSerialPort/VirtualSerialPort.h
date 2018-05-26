//
//  Copyright © 2016 FracturedSoftware. All rights reserved.
//  Based on code by Hiroki Mori. https://sourceforge.net/p/ubsa-osx/svn/13/tree/FTDIDriver/  (BSD License)
//

#ifndef VIRTUAL_SERIAL_PORT_H
#define VIRTUAL_SERIAL_PORT_H

#include <IOKit/IOService.h>
#include <IOKit/serial/IOSerialDriverSync.h> // superclass
#include "SccQueue.h"
#include "Shared.h"
#include "VSPUserClient.h"


#define SPECIAL_SHIFT       (5)
#define SPECIAL_MASK		((1<<SPECIAL_SHIFT) - 1)
//#define	CONTINUE_SEND       1
#define DEFAULT_NOTIFY		(0x00)
#define DEFAULT_AUTO		(PD_RS232_A_RFR | PD_RS232_A_CTS | PD_RS232_A_DSR)
#define DEFAULT_STATE		(PD_S_TX_ENABLE | PD_S_RX_ENABLE | PD_RS232_A_TXO | PD_RS232_A_RXO)
#define STATE_ALL           (PD_RS232_S_MASK | PD_S_MASK)
#define EXTERNAL_MASK   	(PD_S_MASK | (PD_RS232_S_MASK & ~PD_RS232_S_LOOP))
#define MIN_BAUD (50 << 1)
#define kDefaultBaudRate	9600
#define kMaxBaudRate		230400
#define kMaxCirBufferSize	kMessageBufferSize


#define IDLE_XO	   			0
#define NEEDS_XOFF 			1
#define SENT_XOFF 			-1
#define NEEDS_XON  			2
#define SENT_XON  			-2


#define DriverClassName         VirtualSerialPort
#define kSimpleDriverClassName  "VirtualSerialPort"
#define kPortName               "VirtualSerialPort"


static inline mach_timespec long2tval(unsigned long val){
    mach_timespec tval;
    
    tval.tv_sec = val / NSEC_PER_SEC;
    tval.tv_nsec = val % NSEC_PER_SEC;
    return tval;
}

static inline unsigned long tval2long(mach_timespec val){
    return (val.tv_sec * NSEC_PER_SEC) + val.tv_nsec;
}



typedef struct BufferMarks{
    unsigned long	BufferSize;
    unsigned long	HighWater;
    unsigned long	LowWater;
    bool		OverRun;
} BufferMarks;


typedef struct{
    // State and serialization variables
    
    UInt32		State;
    UInt32		WatchStateMask;
    IOLock      *serialRequestLock;
    
    // queue control structures:
    
    CirQueue    RX;
    CirQueue    TX;
    
    BufferMarks RXStats;
    BufferMarks TXStats;
    
    // UART configuration info:
    
    UInt32		CharLength;
    UInt32		StopBits;
    UInt32		TX_Parity;
    UInt32		RX_Parity;
    UInt32		BaudRate;
    bool        MinLatency;
    
    // flow control state & configuration:
    
    UInt8		XONchar;
    UInt8		XOFFchar;
    UInt32		SWspecial[ 0x100 >> SPECIAL_SHIFT ];
    UInt32		FlowControl;			// notify-on-delta & auto_control
    UInt32      FlowControlState;       // tx flow control state, one of PAUSE_SEND if paused or CONTINUE_SEND if not blocked
    
    SInt16		RXOstate;    			// Indicates our receive state.
    SInt16		TXOstate;               // Indicates our transmit state, if we have received any Flow Control.
    
    mach_timespec	DataLatInterval;
    mach_timespec	CharLatInterval;
    
} PortInfo;

class VSPUserClient;


class DriverClassName : public IOSerialDriverSync{
    
    OSDeclareDefaultStructors(VirtualSerialPort)

private:
    
    bool        fTerminate;				// Are we being terminated (ie the device was unplugged)
    bool        fStopping;				// Are we being "stopped"
    IOLock      *RXBufferLock;

public:
    
    VSPUserClient *client;
    IOService   *fProvider;
    PortInfo    fPort;

    virtual bool    start(IOService* provider)override;
    virtual void    stop(IOService* provider) override;
    
    virtual IOReturn acquirePort(bool sleep, void *refCon) override;
    virtual IOReturn releasePort(void *refCon) override;
    virtual IOReturn setState(UInt32 state, UInt32 mask, void *refCon) override;
    virtual UInt32 getState(void *refCon) override;
    virtual IOReturn watchState(UInt32 *state, UInt32 mask, void *refCon) override;
    virtual UInt32 nextEvent(void *refCon) override;
    virtual IOReturn executeEvent(UInt32 event, UInt32 data, void *refCon) override;
    virtual IOReturn requestEvent(UInt32 event, UInt32 *data, void *refCon) override;
    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon) override;
    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon) override;
    virtual IOReturn enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon) override;
    virtual IOReturn dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon) override;
 
    void    initStructure(void);
    bool    allocateResources(void);
    void    releaseResources(void);
    bool    createSerialStream(void);
    void    setStructureDefaults(void);
    void    writePortState(UInt32 state, UInt32 mask);
    UInt32  readPortState(void);
    IOReturn    privateWatchState(UInt32 *state, UInt32 mask);
    void    checkQueues(void);
    bool    allocateRingBuffer(CirQueue *Queue);
    void    freeRingBuffer(CirQueue *Queue);
    
    // Called from VSPTester via VSPUserClient
    virtual IOReturn sendData(TRBufferStruct* inStruct, UInt32* sendCount);
    virtual IOReturn getInfo(void);
    
    // Debug
    
    void debugEvent(char const *str, UInt32 event, UInt32 data);
};

#endif
