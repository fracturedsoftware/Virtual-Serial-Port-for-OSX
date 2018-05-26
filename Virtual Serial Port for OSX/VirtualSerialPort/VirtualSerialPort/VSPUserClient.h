//
//  Copyright © 2016 FracturedSoftware. All rights reserved.
//  Based on code from various sources
//

#ifndef VSP_CLIENT_H
#define VSP_CLIENT_H

#include <IOKit/IOUserClient.h>
#include "VirtualSerialPort.h"


class VirtualSerialPort;

#define UserClientClassName VSPUserClient

class UserClientClassName : public IOUserClient{

    OSDeclareDefaultStructors(VSPUserClient)
    
protected:
    VirtualSerialPort*  fProvider;
    task_t              fTask;
    mach_port_t         m_notificationPort;

    static const IOExternalMethodDispatch	sMethods[kNumberOfMethods];
      
public:

    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual IOReturn clientClose(void) override;
	virtual bool willTerminate(IOService* provider, IOOptionBits options) override;
	virtual bool didTerminate(IOService* provider, IOOptionBits options, bool* defer) override;
	
    // for sending data back to VSPTester
    IOReturn sendPortInfo(void);
    IOReturn sendPortState(UInt32 state);
    
    // only for testing
    virtual bool terminate(IOOptionBits options = 0) override;
    virtual bool finalize(IOOptionBits options) override;
    virtual IOReturn clientDied(void) override;

protected:
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
                                    IOExternalMethodDispatch* dispatch, OSObject* target, void* reference) override;
    static  IOReturn sOpenUserClient(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
    virtual IOReturn openUserClient(void);
    
    static  IOReturn sCloseUserClient(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
    virtual IOReturn closeUserClient(void);
    
    static  IOReturn sGetInfo(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
    virtual IOReturn getInfo(void);

    static  IOReturn sSendData(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
    virtual IOReturn send(TRBufferStruct* inStruct, UInt32* sendCount);
    
    // register a notification callback from VSPTester
    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon) override;
};

#endif