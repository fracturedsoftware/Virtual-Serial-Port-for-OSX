//
//  AppDelegate.h
//  VSPTester
//
//  Copyright © 2016 FracturedSoftware. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "Shared.h"


@interface AppDelegate : NSObject <NSApplicationDelegate>

@property io_connect_t  connect;

@property NSString  *bits0to7;
@property NSString  *bits8to15;
@property NSString  *bits16to23;
@property NSString  *bits24to31;

@property NSString  *charLength;
@property NSString  *stopBits;
@property NSString  *TXParity;
@property NSString  *RXParity;
@property NSString  *baudRate;
@property NSString  *minLatency;
@property NSString  *xon;
@property NSString  *xoff;
@property NSString  *flowControl;
@property NSString  *flowControlState;
@property NSString  *RXOstate;
@property NSString  *TXOstate;

@property NSString   *sendMessage;
@property NSString   *receiveMessage;

- (void)updatePortState:(UInt32)newState;
- (void)updatePortInfo:(PortInfoNotification *)info;
- (void)resetPortInfo;

- (IBAction)sendData:(id)sender;

@end

