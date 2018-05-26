//
//  AppDelegate.m
//  VSPTester
//
//  Copyright © 2016 FracturedSoftware. All rights reserved.
//

#import "AppDelegate.h"
#include <string>
#include <iostream>

using namespace std;

NSArray *parity = @[@"DEFAULT", @"NONE", @"ODD", @"EVEN", @"MARK", @"SPACE", @"ANY"];


void MyDriverRequestCallback (CFMachPortRef port, void *msg, CFIndex size, void *info){
    AppDelegate *delegate = (__bridge AppDelegate*)info;
    
    SInt32 messageID = ((mach_msg_header_t*)msg)->msgh_id;
    
    if(messageID == kPortStateID){
        PortStateNotification *notify = (PortStateNotification *)msg;
        [delegate updatePortState:(UInt32)notify->PortState];
        if(notify->PortState == 0)
            [delegate resetPortInfo];
    }else{
        PortInfoNotification *notify = (PortInfoNotification *)msg;
        [delegate updatePortInfo:notify];
    }
}


@implementation AppDelegate


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    kern_return_t	kernResult;
    io_service_t	service;
    io_iterator_t 	iterator;
    
    [self updatePortState:0];
    self.sendMessage = @"Hello from VSPTester!";
    self.receiveMessage = @"Not Working";

    // This creates an io_iterator_t of all instances of our driver that exist in the I/O Registry.
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("VirtualSerialPort"), &iterator);
    if (kernResult != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kernResult);
        return;
    }
    
    service = IOIteratorNext(iterator);
    // Release the io_iterator_t now that we're done with it.
    IOObjectRelease(iterator);
    
    kernResult = IOServiceOpen(service, mach_task_self(), 0, &_connect);
    if (kernResult != KERN_SUCCESS){
        fprintf(stderr, "IOServiceOpen returned 0x%08x\n", kernResult);
        return;
    }
    // Call our user client's openUserClient method.
    kernResult = IOConnectCallScalarMethod(_connect, kClientOpen, NULL, 0, NULL, NULL);
    if (kernResult != KERN_SUCCESS) {
        fprintf(stderr, "OpenUserClient returned 0x%08x.\n\n", kernResult);
        return;
    }
    printf("OpenUserClient was successful.\n\n");
    
    [self registerNotificationCallback];
    
    kernResult = IOConnectCallScalarMethod(_connect, kClientGetInfo, NULL, 0, NULL, NULL);

    return;
}


- (void)registerNotificationCallback{
    CFMachPortContext      portContext;
    CFMachPortRef          notificationPort = NULL;
    CFRunLoopSourceRef     runLoopSource = NULL;
    kern_return_t          kr;
    
    // Set up the CFMachPortContext structure that is needed when creating the mach port.
    portContext.version = 0;
    portContext.info = (__bridge void*)self; // Aribtrary pointer provided to the callback
    portContext.retain = NULL;
    portContext.release = NULL;
    portContext.copyDescription = NULL;
    
    // Create a mach port.
    notificationPort = CFMachPortCreate(kCFAllocatorDefault, MyDriverRequestCallback, &portContext,  NULL);
    
    if (!notificationPort)
        return;
    
    // Create a run loop source for the mach port.
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, notificationPort, 0);
    // Install the run loop source on the run loop that corresponds to the current thread.
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);

    // Pass the notification port to the driver.
    kr = IOConnectSetNotificationPort(_connect, 0, CFMachPortGetPort(notificationPort), 0);
}


- (void)updatePortState:(UInt32)newState{
    
    bitset<32> stateBits (newState);
    string data = stateBits.to_string<char, string::traits_type, string::allocator_type>();
    
    cout << "PortState = " << data << endl << endl;
   
    bitset<8> bits (newState & 0x000000FF);
    string str = bits.to_string<char, string::traits_type, string::allocator_type>();
    self.bits0to7 = [NSString stringWithUTF8String:str.c_str()];
    
    bits = (newState >> 8) & 0x000000FF;
    str = bits.to_string<char, string::traits_type, string::allocator_type>();
    self.bits8to15 = [NSString stringWithUTF8String:str.c_str()];
    
    bits = (newState >> 16) & 0x000000FF;
    str = bits.to_string<char, string::traits_type, string::allocator_type>();
    self.bits16to23 = [NSString stringWithUTF8String:str.c_str()];
    
    bits = (newState >> 24) & 0x000000FF;
    str = bits.to_string<char, string::traits_type, string::allocator_type>();
    self.bits24to31 = [NSString stringWithUTF8String:str.c_str()];
}


- (void)updatePortInfo:(PortInfoNotification *)info{
    
    self.charLength = [NSString stringWithFormat:@"%llu",info->CharLength];
    self.stopBits = [NSString stringWithFormat:@"%llu",info->StopBits >> 1];
    self.TXParity = parity[info->TX_Parity];
    self.RXParity = parity[info->RX_Parity];
    self.baudRate = [NSString stringWithFormat:@"%llu",info->BaudRate];
    self.minLatency = (info->MinLatency)? @"YES" : @"NO";
    self.xon = [NSString stringWithFormat:@"%llu",info->XONchar];
    self.xoff = [NSString stringWithFormat:@"%llu",info->XOFFchar];
    self.flowControl = [NSString stringWithFormat:@"%llu",info->FlowControl];
    self.flowControlState = [NSString stringWithFormat:@"%llu",info->FlowControlState];
    self.RXOstate = [NSString stringWithFormat:@"%llu",info->RXOstate];
    self.TXOstate = [NSString stringWithFormat:@"%llu",info->TXOstate];
}


- (void)resetPortInfo{
    
    self.charLength = @"";
    self.stopBits = @"";
    self.TXParity = @"";
    self.RXParity = @"";
    self.baudRate = @"";
    self.minLatency = @"";
    self.xon = @"";
    self.xoff = @"";
    self.flowControl = @"";
    self.flowControlState = @"";
    self.RXOstate = @"";
    self.TXOstate = @"";
}


- (IBAction)sendData:(id)sender{
    TRBufferStruct  sendStruct;
    size_t			structSize = sizeof(TRBufferStruct);
    uint64_t        numBytesSent;
    uint32_t        outputCount = 1;
    kern_return_t   result;
    
    NSString *str = [NSString stringWithString:_sendMessage];
    str = [str stringByAppendingString:@"\n"];
    
    UInt64 len = str.length;
    if(len > kMessageBufferSize - 1)
        len = kMessageBufferSize - 1;
    
    strncpy((char *)sendStruct.buffer, str.UTF8String, len);
    sendStruct.numBytes = len;
    
    result = IOConnectCallMethod(_connect, kSendData,
                                     NULL,              // array of scalar (64-bit) input values.
                                     0,                 // the number of scalar input values.
                                     &sendStruct,       // a pointer to the struct input parameter.
                                     structSize,        // the size of the input structure parameter.
                                     &numBytesSent,     // array of scalar (64-bit) output values.
                                     &outputCount,      // pointer to the number of scalar output values.
                                     NULL,              // pointer to the struct output parameter.
                                     NULL               // pointer to the size of the output structure parameter.
                                     );
   
    if (result == KERN_SUCCESS)
        printf("send was successful.\n");
    else
        fprintf(stderr, "send returned 0x%08x.\n\n", result);
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    kern_return_t kernResult = IOConnectCallScalarMethod(_connect, kClientClose, NULL, 0, NULL, NULL);
    
    if (kernResult == KERN_SUCCESS)
        printf("CloseUserClient was successful.\n\n");
    else
        fprintf(stderr, "MyCloseUserClient returned 0x%08x.\n\n", kernResult);
    
    kernResult = IOServiceClose(_connect);
    if (kernResult == KERN_SUCCESS)
        printf("IOServiceClose was successful.\n\n");
    else
        fprintf(stderr, "IOServiceClose returned 0x%08x\n\n", kernResult);
}

@end

