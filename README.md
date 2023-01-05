# AYCWABTU

## overview

AYCWABTU is a proof of concept for a brute force control word  
calculation tool for the common scrambling algorithm used in digital video  
broadcasting.

AYCWABTU is not useful for live decryption of pay TV channels because the  
search for one key needs much more time than the key renewal interval. Majority  
of channels change keys multiple times a minute and AYCWABTU needs months to  
brute force one key. AYCWABTU is intended as proof of concept, and is not  
intended to be used for illegal purposes. The author does not accept  
responsibility for ANY damage incurred by the use of it.

It uses parallel bit slice technique. Other csa parallel bit slice  
implementations (like libdvbcsa) are meant for stream processing.  
They encrypt or decrypt many packets with one key.  
AYCWABTU uses parallel bit slice for decrypting one packet with many keys.

## features

* fast brute force key calculation due to bit sliced crack algorithm (sse2 and 32 bit versions available)
* open source. License: GPL
* read three encrypted data packets from ts file with many checks for valid data
* writes a small probe ts file with these packets for sharing and distributed attack
* test frame included to make sure, it really finds the keys. Also suitable for other brute force tools
* written in C. Developed in Visual Studio 2013, tested with gcc 4.8.2+cygwin 2.844
* much potential for speed improvements

## usage

Have an encrypted transport stream file *file.ts* you want to know the key for and run

>    aycwabtu -t file.ts  

Aycwabtu will check if the file is valid and start searching for the key.  
If will print the current speed in Mcw/s (10^6 control words per second).  
A speed of 8.925 Mcw/s will take one year to search the whole key space of 2^48 keys.  
Optional command line parameters -a and -o may be used to set start and stop key. Might be  
useful for distributed key search.  
During search, aycwabtu will write a *resume* file that allows to continue an interrupted search.  
Aycwabtu will write the found key to file *keyfound.cwl* which may be used with tsdec to decrypt content.
Program can be interrupted by Ctrl-C.  
Note: on windows, aycwabtu needs the cygwin1.dll provided by cygwin (https://cygwin.com) -  
if this dll is missing: error while loading shared libraries.


## to do list

* speed improvement, see ideas below
* multi threading
* support for 256 bits parallel with advanced vector extensions AVX
* detect CPU features at runtime and execute appropriate code
* OpenCL support
* optimize the block sbox boolean equations. Only slightly faster with 128 bits. See da_diett.pdf Chpt. 3.1

## speed optimization ideas

* most important: OpenCL support (not only CUDA, please!). 
* check, why aycw_block_sbox(&sbox_out) fails in gcc, possible speedup ~19%
* block decrypt first (does not depend on stream). Then stream afterwards, stop XORing immediately 
  if foreseeable there is no PES header

## building

Aycwabtu uses CMake (Version 3.22 or newer) as build system generator, so you can use any IDE/build system CMake supports.
Recommended is VS code along with ninja.
Aycwabtu was tested with gcc (Ubuntu 11.3.0-1ubuntu1~22.04)


Linux prerequisites:
sudo apt install git gcc cmake cmake-qt-gui ninja-build

Windows  prerequisites:
Install MSYS2 from https://github.com/msys2/msys2-installer/releases/download/2022-10-28/msys2-x86_64-20221028.exe.
Add to PATH: C:\msys64\mingw64\bin and c:\msys64\usr\bin

Open terminal and run cmake-gui.
Where is the source code:               ./src
Where to build the binaries:            build
Specify generator for this project      Ninja


## developers

https://code.visualstudio.com/docs/cpp/config-mingw


* after changing the code, run tests with SELFTEST enabled to make sure the algorithm still works. It's too easy to break things.
* run "make check"
* test all the batch size implementations
* share your benchmark values in the pull request
* publish all your work please, AYCWABTU is released under GPL

## credits

* FFdecsa, Copyright 2003-2004, fatih89r
* libdvbcsa, http://www.videolan.org/developers/libdvbcsa.html
* ANALYSIS OF THE DVB COMMON SCRAMBLING ALGORITHM, Ralf-Philipp Weinmann and Kai Wirt, Technical University of Darmstadt Department of Computer Science Darmstadt, Germany
* On the Security of Digital Video Broadcast Encryption, Markus Diett
* http://en.wikipedia.org/wiki/Common_Scrambling_Algorithm
* Breaking DVB-CSA, Erik Tews, Julian Waelde, Michael Weiner, Technische Universitaet Darmstadt
* TSDEC - the DVB transport stream offline decrypter, http://sourceforge.net/projects/tsdec/
* http://csa.irde.to   This page disappeared unfortunately but it still accessible here https://web.archive.org/web/20040903151642/http://csa.irde.to/
* and last not least my good friend johnf

***
"Sorry, it is hard to understand and modify but it was harder to design and implement!!!"        fatih89r

Have fun.
