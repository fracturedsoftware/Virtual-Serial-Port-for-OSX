# README #

## Please note this is a work in progress. Not everything is working.##

This is a partially working example of a virtual serial port for OSX. While it is possible to create virtual serial ports using the 'socat' utility, the ports created are not recognised by most serial terminal software as they don't appear in the IORegistry and don't have a name such as /dev/cu.xxx or /dev/tty.xxx. This project instead makes a proper serial driver kernel extension. It will eventually enable software such as an an emulated hardware device with a serial port (DCE) to communicate with a standard serial tool (DTE) such as the terminal.

#### The project has three components: ####

A kernel extension - kext - that creates a virtual serial port (VirtualSerialPort) that will appear as a serial device in /dev with the name /dev/cu.VirtualSerialPort and /dev/tty.VirtualSerialPort. These ports will be found by standard Serial Software.

A user client (VSPUserClient) for communicating with the app (eg. emulated hardware) that loads the virtual serial port. This user client is contained within the kext and built with the VirtualSerialPort.

A tester app that acts as a 'very' basic hardware device that communicates with the VirtualSerialPort through the VSPUserClient.

#### The project enables a setup like so:####

Terminal or serial app <----> /dev/tty.VirtualSerialPort <--> VirtualSerialPort <--> VSPUserClient <----> emulated hardware etc.

### How do I get set up? ###

The project comes with usage instructions and an important READ FIRST document. The project was built for OSX 10.11 but would probably work on slightly older systems.

You can download a zip file of the whole project from [the downloads page](https://bitbucket.org/peterzegelin/virtual-serial-port/downloads?tab=downloads).

### Project Status ###

This is very much a work in progress but very nearly 'working' for at least simple cases. If you follow the Usage.txt instructions you should be able to send simple messages to your terminal from the VSPTester. Unfortunately I have been unable to work out why messages won't flow in the opposite direction from the terminal to the VSPTester. It may be a flow control problem or something else completely. Unfortunately, I am not familiar enough with the details of serial port communication to figure out what is preventing data flowing to the port. Whatever it is, the method that should handle this, VirtualSerialPort::enqueueData, is never called, and I can't work out why. So if you can figure it out please let me know!

### Who do I talk to? ###

If you have any ideas on how to get this fully working please contact peter@fracturedsoftware.com.