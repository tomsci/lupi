LuPi
====

Introduction
------------

LuPi is an embedded OS for ARM hardware, that deliberately does not do a lot of
stuff. It is an experiment on how far you can push a few concepts before it all
falls apart. These concepts are, roughly:

* The kernel should contain only functionality which cannot be implemented
user-side.
* Kernel simplicity is more imporant than maximum functionality.
* User-side code should only be in C if it cannot be implemented in Lua.
* Everything else should be written in Lua.

Further to those concepts, it is a microkernel architecture that (currently)
only runs on the Raspberry Pi. There is no x86 support, no device tree or
dynamic libraries or any kind, no paging (swap memory), and no file system.
And definitely no POSIX support or fork/exec.

Kernel design
-------------

The kernel is written in C. User-side processes are written in Lua 5.2 with a
minimum of C to glue them together (plus the Lua runtime written in C,
obviously).

Processes are isolated from each other using the MMU - one process per ARMv6
ASID. There are relatively small limits on max number of processes, max threads
per process, and memory per process. These limits simplify the kernel design by
allowing almost everything to be represented by hard-coded addresses or simple
macros rather than requiring dynamically-allocated objects to be tracked kernel-
side. ARMv6 tricks like ASIDs and TTBCR are used to minimise the amount of work
the kernel has to do.

There are no user-side executables or libraries - all executable code
is contained in the kernel binary. This removes the need for a runtime linker.

The kernel contains only code for managing Processes and threads and the MMU,
managing hardware at the lowest level, and handling IPC between processes. There
is no malloc in the kernel, and there are no kernel threads. This means the
kernel memory footprint is quite small, although in places memory savings could
be made but only at the cost of simplicity and legibility of the code - in these
cases generally the simpler design is preferred even if it is slightly less
memory efficient.

Currently all kernel code runs in privileged mode with interrupts disabled,
which means there is almost no locking required in the kernel, other than the
interrupt-disabling provided by default by the SVC instruction, although this
may change in the future for performance reasons.

As an example of the principle that anything that can be pushed out of the
kernel should be, timers are handled mostly user-side. At the lowest level the
system timers are configured in the kernel, but all of the functions of handling
the multiple timer-based requests of the rest of the system are instead
implemented in a user process.

All user-side processes are implemented as Lua modules. Some of the system
modules also contain native C code.

### Kernel data structures

Because there is no generic malloc-style allocator in the kernel, all kernel
memory is tracked at the page (4 KB) or section (1 MB) granularity.

Most kernel-side data structures are kept at a fixed address defined in
[memmap.h](k/inc/memmap.h). [mmu_init()](k/inc/mmu.h#mmu_init) sets up an
initial 1 MB section which is referred to as "section zero". This contains the
majority of the kernel data structures, including page tables for the other
sections, kernel and exception stacks, and the `PageAllocator` which tracks
physical memory allocation.

The first page of section zero is the SuperPage, which broadly is where we put
any small amount of data that doesn't need its own page or section. It also
contains the array of `Server` objects (which is why the limit on max number of
servers is so small).

`Process` objects each get one page (4 KB) in the section named
`KProcessesSection`, which contains all its metadata, with the exception of MMU
mappings which are too large to fit in a single page. The relative sizes of page
and section dictate why there is a 256 process limit in the current design (it's
also because there are 256 ASIDs in ARMv6 and one process per ASID maximises
context switch efficiency). All of a process's threads (represented by `Thread`
structs) must fit in this single page which is why there is a hard limit of
(currently) 48 threads per process.

Processes are limited to 256MB of address space because the current kernel
design does not compact the page table layout - ie the page table for a given
address of a given process is always at the same location in the kernel - and
the kernel virtual address map only allows 1 MB for each user process's page
tables (1 MB equals 256 pages, meaning 256 sections' worth of memory). A more
compact page table layout would lift that restriction at the cost of it being
more complicated to walk the page table. And in practice 256 MB per process
should be sufficient while we're running on a Raspberry Pi with only 512 MB RAM
total! (There's probably a use-case somewhere that requires a large amount of
virtual address space without also needing a lot of physical memory but never
mind.)

Each process's PDE is given its own page, but the second-level page tables only
go up to 256 MB, meaning 3 KB of the PDE is unused and currently wasted. The ARM
TTBCR is configured to prevent the MMU from accessing the PDE beyond the 1 KB
needed to map 256 MB, so those 3 kilobytes really are spare, and might be put to
use at some point.

Another result of the TTBCR configuration is that the bottom 1 KB of the
*supervisor* PDE (ie the one pointed to by TTBR0) will never be accessed by the
MMU either (because it will always look at the current process's user PDE in
TTBR1 instead). We stuff the shared page mappings in this 1 KB region - just to
be safe we ensure the bottom few bits of every word is still zero which would
prevent the MMU from getting upset even if it were accessed.

### Thread scheduling

The thread scheduler is currently extremely simple; it uses a round-robin
ready list and no concept of priorities. Threads run until their timeslice is
used up or until they block. Blocking can only be done in an SVC call, which
reschedules directly back to user mode when the thread unblocks. This,
combined with there being no preemption while threads are in supervisor mode,
means that there is never a need to save a supervisor register set, which
saves space in the kernel `Thread` objects. There are no kernel threads except
for the DFC thread, which is handled as a special case (and cannot block because
we don't handle preemption of threads running a privileged mode).

### DFCs and interrupts

Interrupts are enabled during SVC calls, as well as during normal user thread
execution. This means certain operations are done using atomic operations or by
calling [kern_disableInterrupts()](k/scheduler.c#kern_disableInterrupts).
Currently due to the lack of working icache, the atomic operations compile down
to disabling interrupts anyway.

In order to have some level of real-time guarantee on interrupts, and for
example avoid excessive clock drift, the interrupt handler cannot do anything
lengthy such as call thread_requestComplete() while in IRQ mode, because that
will block other IRQs. To handle this we have Deferred Function Calls (DFCs)
which can be scheduled by IRQ code. Any DFCs scheduled by IRQ code run
immediately after the interrupt handler completes. Similar to how SVCs are
currently handled, DFCs run with interrupts enabled, but they cannot be
preempted. They use a dummy Thread structure without associated Process, and
execute using a custom stack located at `KDfcThreadStack`.

DFCs can also be scheduled by SVC code, in which case they will get picked up
after the next interrupts fires. Not ideal but adequate for now.

The 'kernel' stack is only used during boot-up before the first process has
started, and during rescheduling.

Build process
-------------

LuPi is built using a custom Lua script, `build.lua`. There are a couple of
different build targets which correspond to config files
`build/<target>/buildconfig.lua`. Intermediate object files are located at
`bin/obj-<target>/` and build products at `bin/<target>/`.

Syntax:

	./build/build.lua [options] [<target>] [...]
		-m | --modules      Precompile Lua modules with luac.
		-l | --listing      Create assembly listings.
		-p | --preprocess   Preprocess sources only.
		-v | --verbose      Verbose mode.
		-j <number>         Run <number> of compiles in parallel.
		-b | --bootmode <n> Set the boot mode.
		-i | --incremental  Enable incremental build.

	Supported targets:
		clean	Removes all built products.
		pi		Build for the Raspberry Pi.
		tilda	Build for the TiLDA MkE 0.33
		hosted	Build a subset of the code as a native executable (unsupported,
				often in a state of brokenness).
		luac	Builds the luac compiler, must have been run to use the
				 --modules option.
		doc		Generates the HTML documentation.

Targets are built in the order specified on the command line, so if you specify
`clean`, it should be first. Eg:

	./build/build.lua -m clean pi

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

There are two implicit assumptions that the build script makes, related to the
linking options for the kernel. These must match the definitions used inside the
kernl itself. Firstly, that BSS for user processes will always be located at
address `0x7000`. This corresponds to `KUserBss` in `memmap.h`. Secondly, that
the kernel code segment will be located at `0xF8008000`, which is
`KKernelCodeBase` in `memmap.h`. Note this is the _virtual_ address, which is
not the same as the _physical_ address where the bootloader initially loads the
kernel image. The early boot code is especially careful not to assume code is
located at `0xF8008000` prior to it enabling the MMU.

The system requirements for running build.lua are:

* Lua 5.2 must be on your `PATH`, and must support `io.popen()`.
* For pi builds, `arm-none-eabi-gcc` must be on your `PATH`, and must support
  gcc 4.x syntax.
* The default system shell must support `find -path`, `mkdir -p`, `rm`.
* To support incremental builds, the shell must also support `xargs`,
  `stat -f "%m %N"` and `find -prune`.

### Coding standards

The One True Style.

* Indentation with tabs (at four spaces per tab) except in strings that can be
  printed at runtime, which should use spaces. LFs only.
* Aim for 80 character lines unless it looks worse to split it up.
* Documentation comments should be maximum 80 characters.
* Documentation comments should start at the margin, ie don't put a `*` at the
  beginning of every line.
* Variable and function names should be lowercaseCamelCase.
* C: constants should normally be declared as macros of the form
  `#define KMyConstant 1234`.
* Lua: constant-style variables should be CapitalisedCamelCase.
* Struct (C) and metatable (Lua) definitions should be CapitalisedCamelCase.
* C: functions may use a pseudo-namespace-style prefix with an underscore, such
  as `runloop_checkRequestPending()`.
* C: basic `#define`s should be `SHOUTY_CASE` or `KSomeConstant` style.
* C: Function-like and variable-like macros can be in CapitalisedCamelCase if
  they look better like that.
* C: block open curly brackets `{` on the same line as starts the block.
* C: block close curly brackets `}` have same number of indents as the line
  where the `{` occurred.
* C: `if` statements must use curly brackets unless statement is on same line.
* C: `while` statements must use curly brackets. Always. Don't make me come
  down there.
* C: C files must be C99 compliant. Therefore member initialisation like
  `MyStruct s = { .member = 1 };` is encouraged.
* C: the `*` goes with the type. No I don't care that this is technically
  incorrect. Don't write misleading code declaring multiple pointers in a single
  statement and you won't have to care either.
* C: For "pointer-to-const" the `const` can go before the type. For any other
  constness (const-pointer-to-nonconst, const-pointer-to-const etc) put it to
  the right of the thing that is being consted, eg
  `char const * const doublyConstPtr = NULL;`.
* Lua: indent one tab for each block or list of table entries.
* Note: the examples below probably show up with spaces instead of tabs in the
  HTML, because HTML doesn't handle tabs well.

C example:

	struct Something {
		int member;
		char* someOtherMember;
	};

	void someFn(Something* obj) {
		const char* somePointer = obj->someOtherMember;
		if (statement) {
			doSomething();
		} else {
			doSomethingElse();
		}
	}

Lua example:

	Something = class {
		member = 0,
		someOtherMember = "hello",
	}

	function someFn()
		if statement then
			doSomething()
		end
	end

Other constraints:

* C: Global or global static non-const data should only be used extremely
  sparingly (eg by third-party code like `malloc.c`) because there is only 4KB
  total allocated for all global data. Global data of any sort is not permitted
  in the kernel, variables should be stored in the superpage or be accessible
  via a macro expanding to a fixed address.
* C: Fully-const global data is permitted.
* C: Any non-const global data that is used must be uninitialised. Initialised
  data will probably not even be relocated properly let alone get the value you
  specify.

### Documentation

The documentation is built by running `./build/build.lua doc`, and the resulting
HTML is located in `bin/doc/`. All `*.md` files are automatically included in
the generated docs, as are any source files (`*.lua`, `*.c`, `*.h`) that contain
documentation comments. These are indicated in a similar way to javadoc or
doxygen, by a `**` at the beginning of a multiline comment in the source file.
The difference however is that the comments must be written in [markdown][], and
that the syntax has been extended to support Lua-style comment syntax as well as
C-style. C files and headers use the following syntax:

	/**
	Comment describing the following function definition/declaration goes
	here. All the usual _markdown_ can be used.
	*/
	void someFunction() {
		// ...
	}

	/**
	Standalone snippets of documentation are ok too, providing they are
	followed by an empty line (to distinguish them from function docs).
	*/

	// More code here etc...

Lua code can be documented in exactly the same way, except that the comment
delimeter is `--[[** ]]` instead of `/** */`. Note that in either language,
there is no indentation or excess `*` characters prefixing the documentation
lines. Lua modules that define native functions can document them in the Lua
code by using a placeholder of the form `--native function foo()`. Functions
defined in the module's C code that aren't exposed to the Lua side can be
documented in the C file just like any other C function.

	--[[**
	Documentation for a Lua function.
	]]
	function someLuaFunction()
		-- ...
	end

	--[[**
	Documentation for a function that is defined in C - note this
	documentation goes in the .lua file not the .c file!
	]]
	--native function someNativeFunction()

Documentation may contain links to other areas of the docs. An anchor is created
for all documented symbols, the format of which is the symbol name without
brackets or arguments. Lua member functions have the `.` or `:` replaced with an
underscore `_`. This means that from the top-level `README.md` (ie the file that
generated README.html) we can link to the documentation for the function
`RunLoop:queue(obj)` located in `modules/runloop.lua` with the following syntax
`[link text]``(modules/runloop.lua#RunLoop_queue)`, which produces the link:

> [link text](modules/runloop.lua#RunLoop_queue)

Note that link paths are to the source file containing the documentation (ie not
to the generated `.html` file), relative to the source file containing the link.
You will need to add the applicable number of `../` as necessary. The
documentation build target will error if it encounters a broken link in any of
the documentation.

[markdown]: http://daringfireball.net/projects/markdown/

IPC mechanism
-------------

IPC is implemented using a client-server model. All client requests are
asynchronous, except for `ipc.connect()`.

Each client-server connection is represented by a shared page, a page of memory
mapped in both client and server which has the same address in both processes.
The maximum number of shared pages in the system is (currently) 256. The kernel
tracks the owner (the client) and the server for each of these pages. The
beginning of each shared page is an `IpcPage` struct which tracks the
`IpcMessages` used by the connection.

Servers are represented kernel-side by a `Server` object in
`TheSuperPage->servers`. The max number of servers is (currently) 32, although
it would be straightforward to increase this number. Currently only one server
can be running in any given thread (because I haven't got round to sorting out
server IDs - and in practice is unlikely to be an issue). Servers are identified
by a FourCC code. The current list of system servers is:

* `time` - The timer server

To connect to a server, the client creates a new shared page, then calls
`exec_connectToServer()`. (At the Lua level, `ipc.connect(name)`). This blocks
the client thread until the server's `handleServerMsg()` function has run.

Messages between client and server are represented as a pair of `AsyncRequests`
encapsulated in an `IpcMessage` in the relevant shared page. The client
completes the `request` AsyncRequest to send a message to the server, and the
server replies to it by completing the corresponding `response` AsyncRequest.
All data exchange between client and server is done using the shared page.
Because shared pages are guaranteed to be mapped at the same virtual address in
both processes, there need not be any special handling of page colouring
restrictions.

Currently it is up to the users of the IPC mechanism to manage how arguments are
laid out in the shared page, and how lifetimes, fragmentation etc are managed.
This is not ideal and once it starts being used in earnest this will probably
be pushed down into the IPC layer.

Boot sequence
-------------

The bootloader is expected to handle loading the kernel into memory, and on the
Pi it must be configured to do so at physical address `0x8000` and to use this
address as the entry point. This is the default configuration anyway. As on
linux, the MMU must be disabled, and the CPU must be in Supervisor Mode with
interrupts disabled. We also expect the bootloader to be atags-compliant and
pass a pointer to atags data in r2, as well as a machine ID in r1 (which we
currently ignore).

The kernel entry-point `_start()` is defined in `piboot.c` and the build script
ensures this is placed at the beginning of the kernel image. When called by the
bootloader it sets up the early stack (growing downwards from `0x8000`),
configures the exception vectors and exits Secure Mode before calling
`mmu_init()`. At this point the MMU is still disabled.

`mmu_init()` sets up some minimal mappings for Section Zero and for where the
kernel code will be once the MMU is enabled, which is `0xF8008000`
(`KKernelCodeBase` which is within section zero). These mappings are the minimum
required to be able to continue executing code once the MMU is enabled.

Once this returns and the MMU is minimally configured, the entry-point enables
the MMU. That done, the early stack pointer is replaced by the MMU-enabled
address (it uses the same actual physical memory) and the Pi-specific entry
point finshes by calling the more generic `Boot()` function, located in
`boot.c`.

`Boot()` first enables the ARM data cache (and instruction cache if I can ever
get it working!) before initialising the default uart by calling `uart_init()`.
This done it prints the LuPi version. It then begins to setup the kernel data
structures like the PageAllocator, the SuperPage and the Process pages. It also
uses the atags data passed in by the bootloader to establish the amount of RAM
available and board revision. Then it calls `irq_init()`.

Setting up IRQs is SoC-specific therefore `irq_init()` is defined in
`build/pi/irq.c`. This configures the hardware timers on the BCM2835 which are
used to implement system ticks.

What happens once the IRQ setup is complete depends on the `bootMode` option
that was given to build.lua when the kernel was built. If the `bootMode` is `2`
then the boot sequence is paused and a menu is shown on the serial port allowing
you to set the bootMode at runtime, then boot is continued in the same was as if
the `bootMode` had been set at build time. If the `bootMode` is `0` (which is
the default if it wasn't specified on the build.lua command-line) then `Boot()`
sets up the first process and calls `process_start()`. The first process is
always `init`. There are other boot modes which do specific things, see
`k/bootMenu.c` for more details.

### Process creation

`process_init(Process* p, const char* processName)` is called (still in
Supervisor mode) and configures the kernel Process object, then sets up the page
table mappings for the new process. Initially only one page of memory is mapped
user-side, which is for the BSS global data. There isn't much global data used
anywhere so we hard-code a single page for this task, located at address
`KUserBss`.

The kernel function `process_start()` calls `switch_process()` to ensure that
we can access the new process's address space, and then finishes setting up
the BSS page by zeroing it and copying the process name into `user_ProcessName`.
It then stores the user-mode stack pointer into a convenient non-banked register
(`r0`) and switches the process to user mode. There are two final instructions
in the kernel file `process.c`, which are to configure the user-mode `sp` and to
branch to `newProcessEntryPoint` in the user source file `ulua.c`.

Note that because it directly drops to user mode and starts executing the
process entry point, the function `process_start()` never returns. Therefore it
can only be called by code that can handle that, such as in `Boot()` and the
the exec handler for `KExecCreateProcess`.

### User-mode process startup

The first thing that `newProcessEntryPoint()` must do is establish what process
it is supposed to be. It does this by reading the process name from the BSS
variable `user_ProcessName`, which was configured earlier by the kernel code in
`process_start()`. The process name is always the name of a Lua module whose
`main()` function is where the fun really starts. For example the first process,
`init`, starts by calling the Lua function `main()` located in `init.lua`.

The next step is to create the Lua runtime, which is done by
`newLuaStateForModule()`. This function calls `luaL_newstate()` and then
customises the environment for our needs. We include Doug Lea's [malloc][]
(dlmalloc) which is what Lua uses to allocate memory. dlmalloc is configured in
its most basic mode using `sbrk` to allocate more physical memory. This gives a
non-sparse heap growing upwards from `KUserHeapBase` (`0x8000`).

[malloc]: http://g.oswego.edu/dl/html/malloc.html

Once the lua environment is set up, the `main()` function is called and the
process is fully started.

Differences between LuPi and standard Lua 5.2
---------------------------------------------

The Lua environment used for user-side processes is broadly a standard 5.2
setup, with none of the 5.1 compatibility options enabled. There are however
several customisations.

### Numbers are ints

The Lua number type is compiled to be a 32-bit integer. This means there is no
floating-point computation available to Lua modules, and division will always
round to the nearest integer. 64-bit integers are supported in C code, and in
Lua in a limited fashion via the `int64` module.

One problem with using 32-bit signed integers occurs when you need to represent
32-bit unsigned values. A custom implementation of `strtol` is used to ensure
that `tonumber("0x80000000")` returns -1 (ie `0x80000000`) rather than the
standard-mandated `INT_MAX`, ie `0x7FFFFFFF`. This means that even though it
will wrap to be a negative number, it will at least contain the correct bit
pattern. The unfortunate side effect of this is that a statement like
`0x80000000 < 0x7FFFFFFF` will evaluate to `true`. As a result there are some
helper functions defined in the misc module - see [roundDownUnsigned][] and
[lessThanUnsigned][].

[roundDownUnsigned]: modules/misc.lua#roundDownUnsigned
[lessThanUnsigned]: modules/misc.lua#lessThanUnsigned

### No io or os

The `io` and `os` tables are not available. They rely on a lot of standard C
stuff that is not implemented in LuPi, and since there is no filesystem some of
them can't even _be_ implemented. Therefore, the `file` functions are not
available either.

### Modules always get a separate environment

Any module loaded using the standard `require("modulename")` syntax will
automatically get a new table for its `_ENV`, and this will always be what is
returned from `require`. This means you don't have to mess around with contrived
syntax in your module to avoid polluting the global namespace. `_G` is available
still for determined polluters. The standard 5.2 modules and functions (except
for io and os as described above) are available.

### The lupi table

There is a new global table with the functions described below:

*	`lupi.createProcess(processName)`

	Creates a new process as described above in Process Creation.

*	`lupi.getProcessName()`

	Returns the name of the current process.

*	`lupi.getUptime()`

	Returns the number of milliseconds since boot as an
	[Int64](modules/int64.lua).

### Modules with native C code

All Lua modules have to be specified at ROM build time, by an entry in
`build.lua`'s `luaModules` table. As part of the build process all the modules
are embedded in the main binary as a const static C array. This is used at
runtime to locate and load the modules in lieu of a full filesystem.

Modules that require native code should set `native = "path/to/nativecode.c"` in
their entry in `luaModules`.

A C function with signature `int init_module_MODULENAME(lua_State* L)` must
exist in the file that `native` refers to. This function will be called the
first time a module is `require`d in a given process. It is called as a
`lua_CFunction`, with one argument which is the `_ENV` for the module. It is
called after the `_ENV` table has been constructed, but before any of the code
in MODULENAME.lua has run. It should not return anything. The normal use of such
an init function is to populate the module's `_ENV` with some native functions,
usually via a call to `luaL_setfuncs(lua_State* L, const luaL_Reg* l, int nup)`.

The `modules` directory may contain subdirectories, for example to group a
module with its native C code or associated resources:

	modules/
	    timerserver/
            init.lua
            server.lua
            timers.c

To load a module, take the path of the lua file relative to the modules
directory, replace the directory separators with a dot and remove the '.lua'
suffix. For example given the above structure, to load `init.lua` you would say
`require "timerserver.init"`. As a convenience, you may simply write
`require "timerserver"` which will look for the following files in order,
stopping when it finds one:

* modules/timerserver.lua
* modules/timerserver/timerserver.lua
* modules/timerserver/init.lua

Only if none of these files exist will the require throw an error. Since in this
example the only matching file is `modules/timerserver/init.lua`, this is the
module that would be loaded by `require "timerserver"`.
