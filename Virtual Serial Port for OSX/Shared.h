
// Data structure passed between the tool and the user client. This structure and its fields need to have
// the same size and alignment between the user client, 32-bit processes, and 64-bit processes.
// To avoid invisible compiler padding, align fields on 64-bit boundaries when possible
// and make the whole structure's size a multiple of 64 bits.


// VSPUserClient method dispatch selectors.
enum {
    kClientOpen,
    kClientClose,
    kClientGetInfo,
    kSendData,
    kNumberOfMethods // Must be last 
};


#define kMessageBufferSize  64
typedef struct{
    UInt64 numBytes;
    UInt8  buffer[kMessageBufferSize];
}TRBufferStruct;


//  Notifications
enum{
    kPortStateID,
    kPortInfoID
};


typedef struct{
    mach_msg_header_t   messageHeader;
    UInt64  CharLength;
    UInt64  StopBits;
    UInt64  TX_Parity;
    UInt64  RX_Parity;
    UInt64  BaudRate;
    UInt64  MinLatency;
    UInt64  XONchar;
    UInt64  XOFFchar;
    UInt64  FlowControl;			// notify-on-delta & auto_control
    UInt64  FlowControlState;       // tx flow control state, one of PAUSE_SEND if paused or CONTINUE_SEND if not blocked
    UInt64  RXOstate;    			// Indicates our receive state.
    UInt64  TXOstate;               // Indicates our transmit state, if we have received any Flow Control.
}PortInfoNotification;


typedef struct{
    mach_msg_header_t   messageHeader;
    UInt64  PortState;
}PortStateNotification;

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_IOLog(args...)	IOLog (args)
#define DEBUG_putc(c)		conslog_putc(c)
#ifdef ASSERT
#warning DEBUG and ASSERT are defined - resulting kexts are not suitable for deployment
#else
#warning DEBUG is defined - resulting kexts are not suitable for deployment
#endif
#else
#define DEBUG_IOLog(args...)
#define DEBUG_putc(c)
#ifdef ASSERT
#warning ASSERT is defined - resulting kexts are not suitable for deployment
#endif
#endif
#ifdef ASSERT
#define assert(ex)				\
{						\
if (!(ex)) {				\
IOLog( __FILE__ ":%d :", __LINE__ );	\
Debugger("assert(" #ex ") failed");	\
}						\
}
#define	_KERN_ASSERT_H_
#endif