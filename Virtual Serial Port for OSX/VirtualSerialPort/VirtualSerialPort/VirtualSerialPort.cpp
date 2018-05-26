//
//  Copyright © 2016 FracturedSoftware. All rights reserved.
//  Based on code by Hiroki Mori. https://sourceforge.net/p/ubsa-osx/svn/13/tree/FTDIDriver/  (BSD License)
//

#include <IOKit/IOLib.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>
#include "VirtualSerialPort.h"

// Define the superclass.
#define super IOService

OSDefineMetaClassAndStructors(VirtualSerialPort, IOSerialDriverSync)

bool DriverClassName::start(IOService *provider){
    DEBUG_IOLog("VirtualSerialPort::start\n");
    
    fTerminate = false;
    fStopping = false;
    client = NULL;
    
    initStructure();
    
    if (!super::start(provider)){
        return false;
    }
    
    fProvider = OSDynamicCast(IOService, provider);
    if(!fProvider){
        return false;
    }
    
    if (!allocateResources()){
        return false;
    }
    
    // Publish SerialStream services
    if (!createSerialStream()){
        return false;
    }
    
    // Looks like we're ok
    fProvider->retain();
    registerService();
         
    DEBUG_IOLog("VirtualSerialPort::start - OK\n");
    
    return true;
}


#pragma mark stop

void DriverClassName::stop(IOService *provider){
    DEBUG_IOLog("VirtualSerialPort::stop\n");
    
    fStopping = true;
    
    releaseResources();
    
    super::stop(provider);
}


#pragma mark acquirePort

IOReturn DriverClassName::acquirePort(bool sleep, void *refCon){
    DEBUG_IOLog("VirtualSerialPort::acquirePort\n");
    
    UInt32 	busyState = 0;
    
    retain(); 								// Hold reference till releasePort(), unless we fail to acquire
    while (true){
        busyState = (readPortState() & PD_S_ACQUIRED);
        if (!busyState){
            // Set busy bit (acquired), and clear everything else
            writePortState(PD_S_ACQUIRED | DEFAULT_STATE, STATE_ALL);
            break;
        } else {
            if (!sleep){
                release();
                return kIOReturnExclusiveAccess;
            } else {
                busyState = 0;
                IOReturn rtn = watchState(&busyState, PD_S_ACQUIRED, refCon);
                if ((rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess)){
                    continue;
                } else {
                    release();
                    return rtn;
                }
            }
        }
    }
    
    setStructureDefaults();
    ResetQueue(&fPort.TX);
    ResetQueue(&fPort.RX);
    
    writePortState(PD_RS232_S_CTS, PD_RS232_S_CTS);
    
    DEBUG_IOLog("VirtualSerialPort::acquirePort - OK\n");
    
    return kIOReturnSuccess;
}


#pragma mark releasePort

IOReturn DriverClassName::releasePort(void *refCon){
    DEBUG_IOLog("VirtualSerialPort::releasePort\n");
    
    UInt32 busyState = (readPortState() & PD_S_ACQUIRED);
    if (!busyState){
        if (fTerminate || fStopping){
            return kIOReturnOffline;
        }
        
        return kIOReturnNotOpen;
    }
    
    writePortState(0, STATE_ALL);   // Clear the entire state word
    
    fPort.WatchStateMask = 0;
    
    release();                      // Dispose of the self-reference we took in acquirePort()
    
    DEBUG_IOLog("VirtualSerialPort::releasePort - OK\n");
    
    return kIOReturnSuccess;
}


#pragma mark setState

IOReturn DriverClassName::setState(UInt32 state, UInt32 mask, void *refCon){
    DEBUG_IOLog("VirtualSerialPort::setState state:%u mask:%u\n",state,mask);
    
    if (fTerminate || fStopping){
        return kIOReturnOffline;
    }
    
    // Cannot acquire or activate via setState
    if (mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK))){
        return kIOReturnBadArgument;
    }
    
    if (readPortState() & PD_S_ACQUIRED ){
        // ignore any bits that are read-only
        mask &= (~fPort.FlowControl & PD_RS232_A_MASK) | PD_S_MASK;
        if (mask)
            writePortState(state, mask);
        
        return kIOReturnSuccess;
    }
    
    return kIOReturnNotOpen;
}


#pragma mark getState

UInt32 DriverClassName::getState(void *refCon){
    DEBUG_IOLog("VirtualSerialPort::getState\n");
    
    if (fTerminate || fStopping)
        return 0;
    
    checkQueues();
    
    return (readPortState() & EXTERNAL_MASK);
}


#pragma mark watchState

IOReturn DriverClassName::watchState(UInt32 *state, UInt32 mask, void *refCon){
    DEBUG_IOLog("VirtualSerialPort::watchState state:%u mask:%u\n",*state,mask);
    IOReturn 	ret = kIOReturnNotOpen;
    
    if (readPortState() & PD_S_ACQUIRED){
        ret = kIOReturnSuccess;
        mask &= EXTERNAL_MASK;
        ret = privateWatchState(state, mask);
        *state &= EXTERNAL_MASK;
    }
    
    return ret;
}


#pragma mark nextEvent - Not Used
// NOTE: Not used by this driver
UInt32 DriverClassName::nextEvent(void *refCon){
    //  DEBUG_IOLog("VirtualSerialPort::nextEvent\n");
    if (fTerminate || fStopping)
        return kIOReturnOffline;
    
    if (readPortState() & PD_S_ACTIVE)
        return kIOReturnSuccess;
    
    return kIOReturnNotOpen;
}


#pragma mark executeEvent

IOReturn DriverClassName::executeEvent(UInt32 event, UInt32 data, void *refCon){
    IOReturn	ret = kIOReturnSuccess;
    UInt32		state, delta;
    
    if (fTerminate || fStopping)
        return kIOReturnOffline;
    
    delta = 0;
    state = readPortState();
    
    if ((state & PD_S_ACQUIRED) == 0)
        return kIOReturnNotOpen;
    
    switch (event){
        case PD_E_ACTIVE:
            if ((bool)data){
                if (!(state & PD_S_ACTIVE)){
                    setStructureDefaults();
                    writePortState(PD_S_ACTIVE, PD_S_ACTIVE); 			// activate port
                }
            } else {
                if ((state & PD_S_ACTIVE)){
                    writePortState(0, PD_S_ACTIVE);                     // deactivate port
                }
            }
            break;
        case PD_RS232_E_XON_BYTE:
            fPort.XONchar = data;
            break;
        case PD_RS232_E_XOFF_BYTE:
            fPort.XOFFchar = data;
            break;
        case PD_E_SPECIAL_BYTE:
            fPort.SWspecial[ data >> SPECIAL_SHIFT ] |= (1 << (data & SPECIAL_MASK));
            break;
        case PD_E_VALID_DATA_BYTE:
            fPort.SWspecial[ data >> SPECIAL_SHIFT ] &= ~(1 << (data & SPECIAL_MASK));
            break;
        case PD_E_FLOW_CONTROL:
            fPort.FlowControl = data;
            break;
        case PD_E_DATA_LATENCY:
            fPort.DataLatInterval = long2tval(data * 1000);
            break;
        case PD_RS232_E_MIN_LATENCY:
            fPort.MinLatency = bool(data);
            break;
        case PD_E_DATA_INTEGRITY:
            if ((data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE)){
                ret = kIOReturnBadArgument;
            } else {
                fPort.TX_Parity = data;
                fPort.RX_Parity = PD_RS232_PARITY_DEFAULT;
            }
            break;
        case PD_E_DATA_RATE:
            data >>= 1;                     // For API compatiblilty with Intel.
            if ((data < MIN_BAUD) || (data > kMaxBaudRate)){
                ret = kIOReturnBadArgument;
            } else {
                fPort.BaudRate = data;
            }
            break;
        case PD_E_DATA_SIZE:
            data >>= 1;                     // For API compatiblilty with Intel.
            if ((data < 5) || (data > 8)){
                ret = kIOReturnBadArgument;
            } else {
                fPort.CharLength = data;
            }
            break;
        case PD_RS232_E_STOP_BITS:
            if ((data < 0) || (data > 20)){
                ret = kIOReturnBadArgument;
            } else {
                fPort.StopBits = data;
            }
            break;
        case PD_E_RXQ_FLUSH:
            break;
        case PD_E_RX_DATA_INTEGRITY:
            if ((data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY)){
                ret = kIOReturnBadArgument;
            } else {
                fPort.RX_Parity = data;
            }
            break;
        case PD_E_RX_DATA_RATE:
            if (data){
                ret = kIOReturnBadArgument;
            }
            break;
        case PD_E_RX_DATA_SIZE:
            if (data){
                ret = kIOReturnBadArgument;
            }
            break;
        case PD_RS232_E_RX_STOP_BITS:
            if (data){
                ret = kIOReturnBadArgument;
            }
            break;
        case PD_E_TXQ_FLUSH:
            break;
        case PD_RS232_E_LINE_BREAK:
            state &= ~PD_RS232_S_BRK;
            delta |= PD_RS232_S_BRK;
            writePortState(state, delta);
            break;
        case PD_E_DELAY:
            fPort.CharLatInterval = long2tval(data * 1000);
            break;
        case PD_E_RXQ_SIZE:
        case PD_E_TXQ_SIZE:
        case PD_E_RXQ_HIGH_WATER:
        case PD_E_RXQ_LOW_WATER:
        case PD_E_TXQ_HIGH_WATER:
        case PD_E_TXQ_LOW_WATER:
            break;
        default:
            ret = kIOReturnBadArgument;
            break;
    }
    
    debugEvent("executeEvent - ", event, data);
    if(client) client->sendPortInfo();
    return ret;
}


#pragma mark requestEvent

IOReturn DriverClassName::requestEvent(UInt32 event, UInt32 *data, void *refCon){
    IOReturn	ret = kIOReturnSuccess;
    
    if (fTerminate || fStopping) return kIOReturnOffline;
    if (data == NULL) return kIOReturnBadArgument;
    
    switch (event) {
        case PD_E_ACTIVE:               *data = bool(readPortState() & PD_S_ACTIVE);                    break;
        case PD_E_FLOW_CONTROL:         *data = fPort.FlowControl;                                      break;
        case PD_E_DELAY:                *data = (UInt32)tval2long(fPort.CharLatInterval)/1000;          break;
        case PD_E_DATA_LATENCY:         *data = (UInt32)tval2long(fPort.DataLatInterval)/1000;          break;
        case PD_E_TXQ_SIZE:             *data = GetQueueSize(&fPort.TX);                                break;
        case PD_E_RXQ_SIZE:             *data = GetQueueSize(&fPort.RX);                                break;
        case PD_E_TXQ_LOW_WATER:        *data = (UInt32)fPort.TXStats.LowWater;  ret = kIOReturnBadArgument;    break;
        case PD_E_RXQ_LOW_WATER:        *data = (UInt32)fPort.RXStats.LowWater;  ret = kIOReturnBadArgument;    break;
        case PD_E_TXQ_HIGH_WATER:       *data = (UInt32)fPort.TXStats.HighWater; ret = kIOReturnBadArgument;    break;
        case PD_E_RXQ_HIGH_WATER:       *data = (UInt32)fPort.RXStats.HighWater; ret = kIOReturnBadArgument;    break;
        case PD_E_TXQ_AVAILABLE:        *data = FreeSpaceinQueue(&fPort.TX);                            break;
        case PD_E_RXQ_AVAILABLE:        *data = UsedSpaceinQueue(&fPort.RX);                            break;
        case PD_E_DATA_RATE:            *data = fPort.BaudRate << 1;                                    break;
        case PD_E_RX_DATA_RATE:         *data = 0;                                                      break;
        case PD_E_DATA_SIZE:            *data = fPort.CharLength << 1;                                  break;
        case PD_E_RX_DATA_SIZE:         *data = 0;                                                      break;
        case PD_E_DATA_INTEGRITY:       *data = fPort.TX_Parity;                                        break;
        case PD_E_RX_DATA_INTEGRITY:    *data = 0;                                                      break;
        case PD_RS232_E_STOP_BITS:      *data = fPort.StopBits << 1;                                    break;
        case PD_RS232_E_RX_STOP_BITS:   *data = 0;                                                      break;
        case PD_RS232_E_XON_BYTE:       *data = fPort.XONchar;                                          break;
        case PD_RS232_E_XOFF_BYTE:      *data = fPort.XOFFchar;                                         break;
        case PD_RS232_E_LINE_BREAK:     *data = bool(readPortState() & PD_RS232_S_BRK);                 break;
        case PD_RS232_E_MIN_LATENCY:    *data = bool(fPort.MinLatency);                                 break;
        default :                       *data = 0;                               ret = kIOReturnBadArgument;    break;
    }
   
    debugEvent("requestEvent - ", event, *data);
    return ret;
}


#pragma mark enqueueEvent - Not Used. Events are passed to executeEvent

// Not used by this driver. Events are passed on to executeEvent for immediate action.
IOReturn DriverClassName::enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon){
    DEBUG_IOLog("VirtualSerialPort::enqueueEvent\n");
    
    if (fTerminate || fStopping) return kIOReturnOffline;
    
    return executeEvent(event, data, refCon);
}


#pragma mark dequeueEvent - Not Used

// Not used by this driver.
IOReturn DriverClassName::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon){
    //  DEBUG_IOLog("VirtualSerialPort::dequeueEvent\n");
    
    if (fTerminate || fStopping) return kIOReturnOffline;
    if ((event == NULL) || (data == NULL)) return kIOReturnBadArgument;
    if (readPortState() & PD_S_ACTIVE)  return kIOReturnSuccess;
    
    return kIOReturnNotOpen;
}


#pragma mark enqueueData - NOT WORKING

IOReturn DriverClassName::enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon){
    DEBUG_IOLog("VirtualSerialPort::enqueueData\n");
    *count = 0;
    return kIOReturnSuccess;
}


#pragma mark dequeueData

IOReturn DriverClassName::dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon){
    //  DEBUG_IOLog("VirtualSerialPort::dequeueData\n");
    
    // Check to make sure we have good arguments.
    if ((count == NULL) || (buffer == NULL) || (min > size)) return kIOReturnBadArgument;
    
    // If the port is not active then there should not be any chars.
    *count = 0;
    if (!(readPortState() & PD_S_ACTIVE)) return kIOReturnNotOpen;
        
    if (RXBufferLock){
        IOLockLock(RXBufferLock);
        *count = RemovefromQueue(&fPort.RX, buffer, size);
        if(*count)
            checkQueues();
        IOLockUnlock(RXBufferLock);
    }
    
    return kIOReturnSuccess;
}



#pragma mark other

void DriverClassName::initStructure(void){
    DEBUG_IOLog("VirtualSerialPort::initStructure\n");
    
    fPort.State = (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER);
    fPort.WatchStateMask = 0x00000000;
    fPort.serialRequestLock = 0;
}


bool DriverClassName::allocateResources(){
    DEBUG_IOLog("VirtualSerialPort::allocateResources\n");
    
    // Open the end point
    if (!fProvider->open(this)){
        return false;
    }
    
    if (!allocateRingBuffer(&(fPort.TX)) || !allocateRingBuffer(&(fPort.RX))){
        return false;
    }
    
    fPort.serialRequestLock = IOLockAlloc();	// init lock used to protect code on MP
    if (!fPort.serialRequestLock)
        return false;
    
    RXBufferLock = IOLockAlloc();
    if(!RXBufferLock)
        return false;

    return true;
}


void DriverClassName::releaseResources(void){
    DEBUG_IOLog("VirtualSerialPort::releaseResources\n");
    
    if (fProvider){
        fProvider->close(this);
        fProvider->release();
        fProvider = NULL;
    }
    
    if (fPort.serialRequestLock){
        IOLockFree(fPort.serialRequestLock);
        fPort.serialRequestLock = 0;
    }
    
    if(RXBufferLock){
        IOLockFree(RXBufferLock);
        RXBufferLock = 0;
    }
    
    freeRingBuffer(&fPort.TX);
    freeRingBuffer(&fPort.RX);
}



bool DriverClassName::createSerialStream(void){
    DEBUG_IOLog("VirtualSerialPort::createSerialStream\n");
    
    IORS232SerialStreamSync	*pNub = new IORS232SerialStreamSync;
    
    if (!pNub) return false;
    
    // Either we attached and should get rid of our reference
    // or we failed in which case we should get rid our reference as well.
    // This just makes sure the reference count is correct.
    
    bool ret = (pNub->init(0, &fPort) && pNub->attach(this));
    
    pNub->release();
    
    if (!ret) return false;
    
    // Set the name for this port and register it
    pNub->setProperty("IOTTYBaseName", kPortName);
    pNub->registerService();
    return true;
}
                           

void DriverClassName::setStructureDefaults(void){
    DEBUG_IOLog("VirtualSerialPort::setStructureDefaults\n");
    
    fPort.BaudRate = kDefaultBaudRate;			// 9600 bps
    fPort.CharLength = 8;                       // 8 Data bits
    fPort.StopBits = 1;                         // 1 Stop bit
    fPort.TX_Parity = PD_RS232_PARITY_NONE;     // No Parity
    fPort.RX_Parity = PD_RS232_PARITY_NONE;     // --ditto--
    fPort.MinLatency = false;
    fPort.XONchar = '\x11';
    fPort.XOFFchar = '\x13';
    fPort.RXOstate = IDLE_XO;
    fPort.TXOstate = IDLE_XO;
    fPort.FlowControl = (DEFAULT_AUTO | DEFAULT_NOTIFY);
   // fPort.FlowControlState = ;
    
    fPort.RXStats.BufferSize = kMaxCirBufferSize;
    fPort.RXStats.HighWater = (fPort.RXStats.BufferSize << 1) / 3;
    fPort.RXStats.LowWater = fPort.RXStats.HighWater >> 1;
    
    fPort.TXStats.BufferSize = kMaxCirBufferSize;
    fPort.TXStats.HighWater = (fPort.TXStats.BufferSize << 1) / 3;
    fPort.TXStats.LowWater = fPort.TXStats.HighWater >> 1;
    
    for (UInt32 tmp = 0; tmp < (256>>SPECIAL_SHIFT); tmp++){
        fPort.SWspecial[tmp] = 0;
    }
}
                           
                           
void DriverClassName::writePortState(UInt32 state, UInt32 mask){
    //  DEBUG_IOLog("VirtualSerialPort::writePortState\n");
    UInt32  delta;
    
    if (!fPort.serialRequestLock) return;
    
    IOLockLock(fPort.serialRequestLock);
    state = (fPort.State & ~mask) | (state & mask); // compute the new state
    delta = state ^ fPort.State;		    		// keep a copy of the diffs
    fPort.State = state;
    
    if(delta && client) client->sendPortState(state);

    // Wake up all threads asleep on WatchStateMask
    
    if (delta & fPort.WatchStateMask)
        thread_wakeup_with_result( &fPort.WatchStateMask, THREAD_RESTART );
    
    IOLockUnlock(fPort.serialRequestLock);
}

                           
UInt32 DriverClassName::readPortState(void){
    UInt32	returnState = 0;
    
    if (fPort.serialRequestLock){
        IOLockLock(fPort.serialRequestLock );
        returnState = fPort.State;
        IOLockUnlock(fPort.serialRequestLock);
    }
    
    return returnState;
}


IOReturn DriverClassName::privateWatchState(UInt32 *state, UInt32 mask){
    unsigned    watchState, foundStates;
    bool        autoActiveBit = false;
    IOReturn    rtn = kIOReturnSuccess;
    
    watchState  = *state;
    IOLockLock(fPort.serialRequestLock);
    
    // hack to get around problem with carrier detection
    
    if (*state | 0x40)	/// mlj ??? PD_S_RXQ_FULL?
    {
        fPort.State |= 0x40;
    }
    
    if (!(mask & (PD_S_ACQUIRED | PD_S_ACTIVE))){
        watchState &= ~PD_S_ACTIVE;	// Check for low PD_S_ACTIVE
        mask       |=  PD_S_ACTIVE;	// Register interest in PD_S_ACTIVE bit
        autoActiveBit = true;
    }
    
    for (;;){
        // Check port state for any interesting bits with watchState value
        // NB. the '^ ~' is a XNOR and tests for equality of bits.
        
        foundStates = (watchState ^ ~fPort.State) & mask;
        
        if (foundStates){
            *state = fPort.State;
            if (autoActiveBit && (foundStates & PD_S_ACTIVE)){
                rtn = kIOReturnIOError;
            } else {
                rtn = kIOReturnSuccess;
            }
            break;
        }
        
        // Everytime we go around the loop we have to reset the watch mask.
        // This means any event that could affect the WatchStateMask must
        // wakeup all watch state threads.  The two events are an interrupt
        // or one of the bits in the WatchStateMask changing.
        
        fPort.WatchStateMask |= mask;
        
        // note: Interrupts need to be locked out completely here,
        // since as assertwait is called other threads waiting on
        // &port->WatchStateMask will be woken up and spun through the loop.
        // If an interrupt occurs at this point then the current thread
        // will end up waiting with a different port state than assumed
        //  -- this problem was causing dequeueData to wait for a change in
        // PD_E_RXQ_EMPTY to 0 after an interrupt had already changed it to 0.
        
     //   assert_wait(fPort.WatchStateMask, true);	/* assert event */
        
        IOLockUnlock(fPort.serialRequestLock);
        rtn = thread_block(THREAD_CONTINUE_NULL);			/* block ourselves */
        IOLockLock(fPort.serialRequestLock);
        
        if (rtn == THREAD_RESTART){
            continue;
        } else {
            rtn = kIOReturnIPCError;
            break;
        }
    }
    
    // As it is impossible to undo the masking used by this
    // thread, we clear down the watch state mask and wakeup
    // every sleeping thread to reinitialize the mask before exiting.
    
    fPort.WatchStateMask = 0;
    
    thread_wakeup_with_result(&fPort.WatchStateMask, THREAD_RESTART);
    IOLockUnlock(fPort.serialRequestLock);
    
    return rtn;
}


void DriverClassName::checkQueues(void){
    //  DEBUG_IOLog("VirtualSerialPort::checkQueues\n");
    
    // Initialise the QueueState with the current state.
    UInt32 queuingState = readPortState();
    
    // Check to see if there is anything in the Transmit buffer.
    UInt32 used = UsedSpaceinQueue(&fPort.TX);
    UInt32 free = FreeSpaceinQueue(&fPort.TX);
    
    if (free == 0){
        queuingState |=  PD_S_TXQ_FULL;
        queuingState &= ~PD_S_TXQ_EMPTY;
    } else {
        if (used == 0){
            queuingState &= ~PD_S_TXQ_FULL;
            queuingState |=  PD_S_TXQ_EMPTY;
        } else {
            queuingState &= ~PD_S_TXQ_FULL;
            queuingState &= ~PD_S_TXQ_EMPTY;
        }
    }
    
    // Check to see if we are below the low water mark.
    if (used < fPort.TXStats.LowWater)
        queuingState |=  PD_S_TXQ_LOW_WATER;
    else
        queuingState &= ~PD_S_TXQ_LOW_WATER;
    
    if (used > fPort.TXStats.HighWater)
        queuingState |= PD_S_TXQ_HIGH_WATER;
    else
        queuingState &= ~PD_S_TXQ_HIGH_WATER;
    
    
    // Check to see if there is anything in the Receive buffer.
    used = UsedSpaceinQueue(&fPort.RX);
    free = FreeSpaceinQueue(&fPort.RX);
    
    if (free == 0){
        queuingState |= PD_S_RXQ_FULL;
        queuingState &= ~PD_S_RXQ_EMPTY;
    } else {
        if (used == 0){
            queuingState &= ~PD_S_RXQ_FULL;
            queuingState |= PD_S_RXQ_EMPTY;
        } else {
            queuingState &= ~PD_S_RXQ_FULL;
            queuingState &= ~PD_S_RXQ_EMPTY;
        }
    }
    
    // Check to see if we are below the low water mark.
    if (used < fPort.RXStats.LowWater)
        queuingState |= PD_S_RXQ_LOW_WATER;
    else
        queuingState &= ~PD_S_RXQ_LOW_WATER;
    
    if (used > fPort.RXStats.HighWater)
        queuingState |= PD_S_RXQ_HIGH_WATER;
    else
        queuingState &= ~PD_S_RXQ_HIGH_WATER;
    
    // Figure out what has changed to get mask.
    UInt32 deltaState = queuingState ^ readPortState();
    writePortState(queuingState, deltaState);
}


bool DriverClassName::allocateRingBuffer(CirQueue *Queue){
    DEBUG_IOLog("VirtualSerialPort::allocateRingBuffer\n");
    
    UInt8   *Buffer = (UInt8*)IOMalloc(kMaxCirBufferSize);
    
    InitQueue(Queue, Buffer, kMaxCirBufferSize);
    
    if (Buffer)
        return true;
    
    return false;
}


void DriverClassName::freeRingBuffer(CirQueue *Queue){
    DEBUG_IOLog("VirtualSerialPort::freeRingBuffer\n");
    
    if (Queue){
        if (Queue->Start){
            IOFree(Queue->Start, Queue->Size);
        }
        CloseQueue(Queue);
    }
}


# pragma mark Debug

void DriverClassName::debugEvent(char const *str, UInt32 event, UInt32 data){
    DEBUG_IOLog("%s",str);
    
    switch (event){
        case PD_E_ACTIVE:               DEBUG_IOLog("PD_E_ACTIVE\n");            break;
        case PD_RS232_E_XON_BYTE:       DEBUG_IOLog("PD_RS232_E_XON_BYTE\n");    break;
        case PD_RS232_E_XOFF_BYTE:      DEBUG_IOLog("PD_RS232_E_XOFF_BYTE\n");   break;
        case PD_E_SPECIAL_BYTE:         DEBUG_IOLog("PD_E_SPECIAL_BYTE\n");      break;
        case PD_E_VALID_DATA_BYTE:      DEBUG_IOLog("PD_E_VALID_DATA_BYTE\n");   break;
        case PD_E_FLOW_CONTROL:         DEBUG_IOLog("PD_E_FLOW_CONTROL\n");      break;
        case PD_E_DATA_LATENCY:         DEBUG_IOLog("PD_E_DATA_LATENCY\n");      break;
        case PD_RS232_E_MIN_LATENCY:    DEBUG_IOLog("PD_RS232_E_MIN_LATENCY\n"); break;
        case PD_E_DATA_INTEGRITY:       DEBUG_IOLog("PD_E_DATA_INTEGRITY\n");    break;
        case PD_E_DATA_RATE:            DEBUG_IOLog("PD_E_DATA_RATE\n");         break;
        case PD_E_DATA_SIZE:            DEBUG_IOLog("PD_E_DATA_SIZE\n");         break;
        case PD_RS232_E_STOP_BITS:      DEBUG_IOLog("PD_RS232_E_STOP_BITS\n");   break;
        case PD_E_RXQ_FLUSH:            DEBUG_IOLog("PD_E_RXQ_FLUSH\n");         break;
        case PD_E_RX_DATA_INTEGRITY:    DEBUG_IOLog("PD_E_RX_DATA_INTEGRITY\n"); break;
        case PD_E_RX_DATA_RATE:         DEBUG_IOLog("PD_E_RX_DATA_RATE\n");      break;
        case PD_E_RX_DATA_SIZE:         DEBUG_IOLog("PD_E_RX_DATA_SIZE\n");      break;
        case PD_RS232_E_RX_STOP_BITS:   DEBUG_IOLog("PD_RS232_E_RX_STOP_BITS\n");break;
        case PD_E_TXQ_FLUSH:            DEBUG_IOLog("PD_E_TXQ_FLUSH\n");         break;
        case PD_RS232_E_LINE_BREAK:     DEBUG_IOLog("PD_RS232_E_LINE_BREAK\n");  break;
        case PD_E_DELAY:                DEBUG_IOLog("PD_E_DELAY\n");             break;
        case PD_E_RXQ_SIZE:             DEBUG_IOLog("PD_E_RXQ_SIZE\n");          break;
        case PD_E_TXQ_SIZE:             DEBUG_IOLog("PD_E_TXQ_SIZE\n");          break;
        case PD_E_RXQ_HIGH_WATER:       DEBUG_IOLog("PD_E_RXQ_HIGH_WATER\n");    break;
        case PD_E_RXQ_LOW_WATER:        DEBUG_IOLog("PD_E_RXQ_LOW_WATER\n");     break;
        case PD_E_TXQ_HIGH_WATER:       DEBUG_IOLog("PD_E_TXQ_HIGH_WATER\n");    break;
        case PD_E_TXQ_LOW_WATER:        DEBUG_IOLog("PD_E_TXQ_LOW_WATER\n");     break;
        case PD_E_TXQ_AVAILABLE:        DEBUG_IOLog("PD_E_TXQ_AVAILABLE\n");     break;
        default:                        DEBUG_IOLog("unrecognized event\n");     break;
    }
}


# pragma mark
# pragma mark From Client

IOReturn DriverClassName::sendData(TRBufferStruct* inStruct, UInt32* sendCount){
    DEBUG_IOLog("VirtualSerialPort::send\n");
    
    if (RXBufferLock){
        IOLockLock(RXBufferLock);
        *sendCount = AddtoQueue(&fPort.RX, inStruct->buffer, inStruct->numBytes);
        checkQueues();
        writePortState(256,256);
        IOLockUnlock(RXBufferLock);
    }
    
    return kIOReturnSuccess;
}


IOReturn DriverClassName::getInfo(void){
    DEBUG_IOLog("VirtualSerialPort::getInfo\n");
    
    if(client){
        client->sendPortInfo();
        client->sendPortState(readPortState());
    }

    return kIOReturnSuccess;
}


