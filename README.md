# LuPi

## Introduction

LuPi is an embedded OS for ARM hardware, that deliberately does not do a lot of
stuff. It is an experiment on how far you can push a few concepts before it all
falls apart. These concepts are, roughly:

* The kernel should contain only functionality which cannot be implemented
user-side.
* Kernel simplicity is more important than maximum functionality.
* User-side code should only be in C if it cannot be implemented in Lua.
* Everything else should be written in Lua.

Further to those concepts, it is a microkernel architecture that (currently)
only runs on the Raspberry Pi and the [TiLDA MKe][mke]. There is no x86 support, no
device tree or dynamic libraries or any kind, no paging (swap memory), and no
file system. And definitely no POSIX support or fork/exec.

[mke]: https://badge.emfcamp.org/wiki/Main_Page#EMF2014_-_TiLDA_MKe

## Project layout

    bin/        - Where final binaries are built to
    build/      - One subdirectory per build target
      build.lua - The main build script
      tilda/    - all project files specific to the TiLDA hardware
      pi/       - All Pi 1 and 2 specific files
      pi3/      - In progress, non-functional Pi 3 AARCH64 support
    k/          - All the kernel code
    lua/        - The code comprising the Lua runtime
    modules/    - Lua modules and 'apps'
    testing/    - Various junky bits of temporary test code
    userinc/    - Headers for presenting a vaguely C-like environment for
                  building Lua
    usersrc/    - Cut-down implementations of the subset of std C required to
                  build Lua

## Build process

LuPi is built using a custom Lua script, `build.lua`. There are a couple of
different build targets which correspond to config files
`build/<target>/buildconfig.lua`. Intermediate object files are located at
`bin/obj-<target>/` and build products at `bin/<target>/`.

For syntax, see [build.lua syntax](build/build.lua#Syntax).

Targets are built in the order specified on the command line, so if you specify
`clean`, it should be first. Eg:

    ./build/build.lua -m clean luac pi

The command-line parsing also supports combining short options together, so the
following are all equivalent:

    ./build/build.lua -i -m -j 64 pi
    ./build/build.lua -im -j64 pi
    ./build/build.lua -imj 64 pi
    ./build/build.lua -imj64 pi

Incremental builds are enabled by specifying the `-i` or `--incremental`
command-line options. When enabled, the build will output an additional file
`bin/obj-<target>/dependencyCache.lua`. This file contains information about the
dependencies of all the object files as well as the command-line used to create
them. If the next build also enables the incremental option, this file is loaded
and a file is only recompiled if it or any of its dependencies have changed
since the last build (ie if the last modified date of any of the sources is
more recent than that of the object file). Sources are also rebuilt if the
command line used to build them has changed (for example, due to a different
bootMode being specified). A non-incremental build will not calculate the
dependencies and will delete dependencyCache.lua if it exists.

The system requirements for running build.lua are:

* Lua 5.3 must be on your `PATH`, and must support `io.popen()`.
* For pi builds, `arm-none-eabi-gcc` must be on your `PATH`, and must support
  gcc 4.x syntax.
* The default system shell must support `find -path`, `mkdir -p`, `rm`.
* To support incremental builds, the shell must also support `xargs`,
  `stat -f "%m %N"` and `find -prune`.

## Building the code - Tilda

From the main directory, run build.lua - see the [build process](#Build_process)
section for more info, but these are the usual options. We first clean all and
build `luac` which is required by the use of the `-m` option.

    ./build/build.lua clean doc luac
    ./build/build.lua -limj64 tilda --bootmode 6

This creates a kernel.img in `bin/tilda/kernel.img`. This must be flashed onto
the device using `bossac`. I extracted this from my Arduino IDE installation but
it looks like it can be also be downloaded from
<https://sourceforge.net/projects/b-o-s-s-a/>.

First you'll need to put the board into flash mode by shorting the "Erase" pins
while pressing the "Reset" button. Then plug in the USB cable and the device
should enumerate (on OS X) as /dev/cu.usbmodemNNNN. Flash the image using:

    bossac -i --port=/dev/cu.usbmodem1411 -U true -e -w -b bin/tilda/kernel.img -R

Once flashing is complete, power cycle the board and it should boot into the GUI
boot menu.

## Internal details

For more details about the kernel design, see [here](doc/internals.md).

## License

Except as noted below, this software is &copy; 2014-2016 Tom Sutcliffe.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### markdown.lua

[markdown.lua](build/doc/markdown.lua#License) is Copyright &copy; 2008 Niklas
Frykholm.

### Lua

Lua is Copyright &copy; 1994–2016 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### malloc.c

This is a version (aka dlmalloc) of malloc/free/realloc written by Doug Lea and
released to the public domain, as explained at
http://creativecommons.org/publicdomain/zero/1.0/

### memcmp_arm.S, memcpy_arm.S

Copyright (C) 2008 The Android Open Source Project

    All rights reserved.
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

### memcmp_thumb2.s, memcpy_thumb2.s

Copyright (c) 2009 Apple Inc. All rights reserved.

    @APPLE_LICENSE_HEADER_START@
    
    This file contains Original Code and/or Modifications of Original Code
    as defined in and that are subject to the Apple Public Source License
    Version 2.0 (the 'License'). You may not use this file except in
    compliance with the License. Please obtain a copy of the License at
    http://www.opensource.apple.com/apsl/ and read it before using this
    file.
    
    The Original Code and all software distributed under the License are
    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
    INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
    Please see the License for the specific language governing rights and
    limitations under the License.
    
    @APPLE_LICENSE_HEADER_END@
