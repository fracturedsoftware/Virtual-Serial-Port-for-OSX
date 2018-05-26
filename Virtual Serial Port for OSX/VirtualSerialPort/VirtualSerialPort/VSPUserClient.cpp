//
//  Copyright © 2016 FracturedSoftware. All rights reserved.
//  Based on code from various sources
//

#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <libkern/OSByteOrder.h>
#include "VSPUserClient.h"


#define super IOUserClient

// Even though we are defining the convenience macro super for the superclass, you must use the actual class name
OSDefineMetaClassAndStructors(VSPUserClient, IOUserClient)


#pragma mark User Client Method Dispatch Table.

const IOExternalMethodDispatch UserClientClassName::sMethods[kNumberOfMethods] = {
	{   // kClientOpen
		(IOExternalMethodAction) &UserClientClassName::sOpenUserClient,	// Method pointer.
		0,																		// No scalar input values.
		0,																		// No struct input value.
		0,																		// No scalar output values.
		0																		// No struct output value.
	},  {   // kClientClose
		(IOExternalMethodAction) &UserClientClassName::sCloseUserClient,	// Method pointer.
		0,																		// No scalar input values.
		0,																		// No struct input value.
		0,																		// No scalar output values.
		0																		// No struct output value.
    },  {   // kClientGetInfo
        (IOExternalMethodAction) &UserClientClassName::sGetInfo,            // Method pointer.
        0,																		// No scalar input values.
        0,																		// No struct input value.
        0,																		// No scalar output values.
        0																		// No struct output value.
    },	{   // kSendData
        (IOExternalMethodAction) &UserClientClassName::sSendData,        // Method pointer.
        0,																		// No scalar input values.
        sizeof(TRBufferStruct),                                                 // Size of input struct.
        1,																		// One scalar output value.
        0                                                                       // No struct output value.
    }
};


IOReturn UserClientClassName::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
												   IOExternalMethodDispatch* dispatch, OSObject* target, void* reference){
	IOLog("%s[%p]::%s(%d, %p, %p, %p, %p)\n", getName(), this, __FUNCTION__,
		  selector, arguments, dispatch, target, reference);
        
    if (selector < (uint32_t) kNumberOfMethods){
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target) {
            target = this;
        }
    }
        
	return super::externalMethod(selector, arguments, dispatch, target, reference);
}


#pragma mark initWithTask

// initWithTask is called as a result of the user process calling IOServiceOpen.
bool UserClientClassName::initWithTask(task_t owningTask, void* securityToken, UInt32 type){
    
	bool success = super::initWithTask(owningTask, securityToken, type);
	
	IOLog("%s[%p]::%s(%p, %p, %u)\n", getName(), this, __FUNCTION__, owningTask, securityToken, (unsigned int)type);

    fTask = owningTask;
    fProvider = NULL;
    m_notificationPort = 0;
        
    return success;
}


#pragma mark start

// start is called after initWithTask as a result of the user process calling IOServiceOpen.
bool UserClientClassName::start(IOService* provider){
	IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    // Verify that this user client is being started with a provider that it knows how to communicate with.
	fProvider = OSDynamicCast(DriverClassName, provider);
    bool success = (fProvider != NULL);
    
    if (success){
		success = super::start(provider);
	}
    fProvider->client = this;
	
    return success;
}


#pragma mark clientClose

// clientClose is called as a result of the user process calling IOServiceClose.
IOReturn UserClientClassName::clientClose(void){
	IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    // Defensive coding in case the user process called IOServiceClose without calling closeUserClient first.
    (void) closeUserClient();
    
	// Inform the user process that this user client is no longer available. This will also cause the
	// user client instance to be destroyed.
	//
	// terminate would return false if the user process still had this user client open.
	// This should never happen in our case because this code path is only reached if the user process
	// explicitly requests closing the connection to the user client.
	bool success = terminate();
	if (!success) {
		IOLog("%s[%p]::%s(): terminate() failed.\n", getName(), this, __FUNCTION__);
	}
    
    return kIOReturnSuccess;
}


IOReturn UserClientClassName::sCloseUserClient(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments){
    return target->closeUserClient();
}


IOReturn UserClientClassName::closeUserClient(void){
    IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    IOReturn	result = kIOReturnSuccess;
    
    if (fProvider == NULL) {
        // Return an error if we don't have a provider. This could happen if the user process
        // called closeUserClient without calling IOServiceOpen first.
        result = kIOReturnNotAttached;
        IOLog("%s[%p]::%s(): returning kIOReturnNotAttached.\n", getName(), this, __FUNCTION__);
    }
    else if (fProvider->isOpen(this)) {
        // Make sure we're the one who opened our provider before we tell it to close.
        fProvider->close(this);
        fProvider->client = NULL;
    }
    else {
        result = kIOReturnNotOpen;
        IOLog("%s[%p]::%s(): returning kIOReturnNotOpen.\n", getName(), this, __FUNCTION__);
    }
    
    return result;
}


// willTerminate is called at the beginning of the termination process. It is a notification
// that a provider has been terminated, sent before recursing up the stack, in root-to-leaf order.
//
// This is where any pending I/O should be terminated. At this point the user client has been marked
// inactive and any further requests from the user process should be returned with an error.
bool UserClientClassName::willTerminate(IOService* provider, IOOptionBits options){
	IOLog("%s::%s\n", getName(), __FUNCTION__);
	
	return super::willTerminate(provider, options);
}


// didTerminate is called at the end of the termination process. It is a notification
// that a provider has been terminated, sent after recursing up the stack, in leaf-to-root order.
bool UserClientClassName::didTerminate(IOService* provider, IOOptionBits options, bool* defer){
	IOLog("%s::%s\n", getName(), __FUNCTION__);
	
	// If all pending I/O has been terminated, close our provider. If I/O is still outstanding, set defer to true
	// and the user client will not have stop called on it.
	closeUserClient();
	*defer = false;
	
	return super::didTerminate(provider, options, defer);
}


IOReturn UserClientClassName::sOpenUserClient(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments){
    return target->openUserClient();
}


IOReturn UserClientClassName::openUserClient(void){
  	IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    IOReturn result = kIOReturnSuccess;
    
    if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called openUserClient without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
        result = kIOReturnNotAttached;
	}
    else if (!fProvider->open(this)) {
		// The most common reason this open call will fail is because the provider is already open
		// and it doesn't support being opened by more than one client at a time.
		result = kIOReturnExclusiveAccess;
	}
        
    return result;
}

#pragma mark For Testing

// We override stop only to log that it has been called to make it easier to follow the user client's lifecycle.
void UserClientClassName::stop(IOService* provider){
    IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    super::stop(provider);
}


// We override terminate only to log that it has been called to make it easier to follow the user client's lifecycle.
// Production user clients will rarely need to override terminate. Termination processing should be done in
// willTerminate or didTerminate instead.
bool UserClientClassName::terminate(IOOptionBits options){
    IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    bool success = super::terminate(options);
    
    return success;
}


// We override finalize only to log that it has been called to make it easier to follow the user client's lifecycle.
// Production user clients will rarely need to override finalize.
bool UserClientClassName::finalize(IOOptionBits options){
    IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    bool success = super::finalize(options);
    
    return success;
}


// clientDied is called if the client user process terminates unexpectedly (crashes).
// We override clientDied only to log that it has been called to make it easier to follow the user client's lifecycle.
// Production user clients need to override clientDied only if they need to take some alternate action if the user process
// crashes instead of exiting normally.
IOReturn UserClientClassName::clientDied(void){
    IOLog("%s::%s\n", getName(), __FUNCTION__);
    
    // The default implementation of clientDied just calls clientClose.
    return super::clientDied();
}


#pragma mark
#pragma mark Send Message

IOReturn UserClientClassName::sSendData(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments){
    IOLog("VSPUserClient::sSendData\n");
    
    return target->send((TRBufferStruct*)arguments->structureInput,(uint32_t*) &arguments->scalarOutput[0]);
}


IOReturn UserClientClassName::send(TRBufferStruct* inStruct, UInt32* sendCount){
    
    return fProvider->sendData(inStruct,sendCount);
}


#pragma mark GetInfo

IOReturn UserClientClassName::sGetInfo(UserClientClassName* target, void* reference, IOExternalMethodArguments* arguments){
    IOLog("VSPUserClient::sGetInfo\n");
    
    return target->getInfo();
}


IOReturn UserClientClassName::getInfo(void){
    
    return fProvider->getInfo();
}


#pragma mark Notifications

IOReturn UserClientClassName::registerNotificationPort (mach_port_t port, UInt32 type, io_user_reference_t refCon){
    DEBUG_IOLog("VSPUserClient::registerNotificationPort\n");
    
    m_notificationPort = port;
    return kIOReturnSuccess;
}


IOReturn UserClientClassName::sendPortState(UInt32 state){
    DEBUG_IOLog("VSPUserClient::portStateNotification\n");
    PortStateNotification   notification;
    IOReturn                result;
    
    if (m_notificationPort == MACH_PORT_NULL) return kIOReturnError;
    
    // Set up the standard mach_msg_header_t fields.
    notification.messageHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    notification.messageHeader.msgh_size = sizeof(PortStateNotification);
    notification.messageHeader.msgh_remote_port = m_notificationPort;
    notification.messageHeader.msgh_local_port = MACH_PORT_NULL;
    notification.messageHeader.msgh_reserved = 0;
    
    // Fill in the port info
    notification.messageHeader.msgh_id = kPortStateID;
    notification.PortState = state;
    
    // Send the request to user space
    result = mach_msg_send_from_kernel(&notification.messageHeader, sizeof(PortStateNotification));
    return result;
}



IOReturn UserClientClassName::sendPortInfo(void){
    DEBUG_IOLog("VSPUserClient::portInfoNotification\n");
    PortInfoNotification    notification;
    IOReturn                result;
    
    if (m_notificationPort == MACH_PORT_NULL) return kIOReturnError;
    
    // Set up the standard mach_msg_header_t fields.
    notification.messageHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    notification.messageHeader.msgh_size = sizeof(PortInfoNotification);
    notification.messageHeader.msgh_remote_port = m_notificationPort;
    notification.messageHeader.msgh_local_port = MACH_PORT_NULL;
    notification.messageHeader.msgh_reserved = 0;
    
    // Fill in the port info
    notification.messageHeader.msgh_id = kPortInfoID;
    notification.CharLength = fProvider->fPort.CharLength;
    notification.StopBits = fProvider->fPort.StopBits;
    notification.TX_Parity = fProvider->fPort.TX_Parity;
    notification.RX_Parity = fProvider->fPort.RX_Parity;
    notification.BaudRate = fProvider->fPort.BaudRate;
    notification.MinLatency = fProvider->fPort.MinLatency;
    notification.XONchar = fProvider->fPort.XONchar;
    notification.XOFFchar = fProvider->fPort.XOFFchar;
    notification.FlowControl = fProvider->fPort.FlowControl;
    notification.FlowControlState = fProvider->fPort.FlowControlState;
    notification.RXOstate = fProvider->fPort.RXOstate;
    notification.TXOstate = fProvider->fPort.TXOstate;
    
    // Send the request to user space
    result = mach_msg_send_from_kernel(&notification.messageHeader, sizeof(PortInfoNotification));
    return result;
}

