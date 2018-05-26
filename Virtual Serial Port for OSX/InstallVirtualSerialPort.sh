#!/bin/bash
# don't forget to change this script so it will cd to 'your' desktop.
# kextunload will show an error if the kext has not been loaded before but that is OK.

sudo kextunload -b com.fracturedsoftware.VirtualSerialPort
cd /Users/name/Desktop
sudo cp -R VirtualSerialPort.kext /tmp
chown -R root:wheel /tmp/VirtualSerialPort.kext
sudo kextload /tmp/VirtualSerialPort.kext