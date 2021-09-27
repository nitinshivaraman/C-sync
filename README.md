# C-sync
Source for C-sync protocol

Install the following dependencies: 
```sudo apt-get install build-essential binutils-msp430 gcc-msp430 msp430-libc msp430mcu mspdebug gcc-arm-none-eabi openjdk-8-jdk openjdk-8-jre ant libncurses5-dev```



##### Configuration and Compilation
* Project configuration has to be checked in */home/c-sync/examples/c-sync/project-conf.h* file
	1.  Ensure that the below lines have the same values (Rest of the values are left unchanged):
	```C
		#define MOD_NEIGHBOURS 1 // default 0, 1 for hardcoded neighbours to create topologies
		#define MOD_TYPE 3 // 1 for full network, 2 for chain, 3 for byzantine testing
		#define TEST_GTSP 0 // default 0, 1 for GTSP testing
		#define TEST_BYZ 1 // default 0, 1 for Byzantine fault testing```
* Open a terminal and navigate to */home/C-sync/examples/c-sync*
* Compile the code using **make clean;make all** and observe that *c-sync.sky* file is generated.
