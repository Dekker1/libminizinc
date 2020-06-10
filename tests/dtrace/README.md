# Dynamic tracing in MiniZinc

MiniZinc has preliminary support for dynamic tracing. This allows developers to
enable inserting custom tracing scripts at certain crucial points during
execution without any the overhead when tracing is not required.

This tracing is supported through DTrace. DTrace is only available for MacOS
and BSD. For Linux there is an alternative called
[SystemTap](https://sourceware.org/systemtap/). SystemTap uses the same
resources as DTrace, but uses different scripts.

## DTrace

[DTrace](http://dtrace.org/guide/preface.html) provides a library for dynamic
tracing of events. This document describes the implementation of DTrace within
the MiniZinc compiler. In particular, it focuses on how to use and extend the DTrace
implementation within MiniZinc.


### Executing DTrace scripts with MiniZinc

You can find various example DTrace scripts in `tests/dtrace/`. To run these
scripts, you can use a command in the form of `[script] -c "[command]"`. You need
to use a MiniZinc compiler compiled with the DTrace to use DTrace.

You can also execute a script using the dtrace command. This is necessary if the
script does not contain a "Shebang". You can do it using the following command:
`dtrace -s [script] -c "[command]"`.

#### MacOS 10.11+

MacOS El Capitan for the first time included "System Integrity Protection". This
feature does not allow for all DTrace features to be used. This includes custom
DTrace providers. To get around this problem, the following steps can be
followed:

1. Boot your Mac into Recovery Mode.
2. Open the Terminal from the Utilities menu.
3. Then use the following commands:

```bash

csrutil clear # restore the default configuration first

csrutil enable --without dtrace # disable dtrace restrictions *only*

```

After completing these steps your Mac should be able to use DTrace after a
restart. For more information visit the following article:
[link](http://internals.exposed/blog/dtrace-vs-sip.html).

### Writing scripts for DTrace with MiniZinc

The following paragraph will inform you on the MiniZinc provider and its probes
for DTrace. We refer to the [DTrace
documentation](http://dtrace.org/guide/preface.html) for more information on
the syntax and possibilities.

The MiniZinc provider for DTrace consists of DTrace probes implemented in the
Runtime. A binary compiled with DTrace enabled will allow the user access to
the information of the probes. You can find a list of all probes and their
arguments in
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d).
The following is a toy example DTrace script:

```

pony$target:::gc-start
{
  @ = count();
}

END
{
  printa(@);
}

```

This script increases a counter every time the "GC Start" probe is *fired* and
prints it result at the end. Note the way in which we access the probe. It
consists of two parts: the first part is the provider and the second part,
after `:::` is the name of the probe. The provider is during runtime appended
by the process ID. In this example we use the `$target` variable to find the
process ID.  Another option is to use `pony*`, but note that this could match
many processes.  The name of the probe are also different from the names in
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d).
In Dtrace scripts you have to replace the `__` in the names by `-`.

To make these scripts executable, like the ones in the examples, we use the
following "Shebang": `#!/usr/bin/env dtrace -s`. Extra options can be added to
improve its functionality.


# SystemTap

SystemTap is the Linux alternative to DTrace. Although DTrace is available for
Linux the licensing and philosophical differences have caused Linux developers
to create an alternative tracing framework that is compatible with the GPLv2,
thus SystemTap was created. SystemTap and DTrace operate in a similar fashion
but since then the frameworks have been developed independently. Complete
SystemTap documentation can be found
[here](https://sourceware.org/systemtap/documentation.html)

## Requirements

*   Linux Kernel with UPROBES or UTRACE.

  If your Linux kernel is version 3.5 or higher, it already includes UPROBES. To
  verify that the current kernel supports UPROBES natively, run the following
  command:

  ```
  # grep CONFIG_UPROBES /boot/config-`uname -r`
  ```

  If the kernel supports the probes this will be the output:
  ```
  CONFIG_UPROBES=y
  ```

  If the kernel's version is prior to 3.5, SystemTap automatically builds the
  UPROBES module. However, you also need the UTRACE kernel extensions required
  by the SystemTap user-space probing to track various user-space events. This
  can be verified with this command:

  ```
  # grep CONFIG_UTRACE /boot/config-`uname -r
  ```
  If the < 3.5 kernel supports user tracing this will be the output:

  ```
  CONFIG_UTRACE=y
  ```

  Note: UTRACE does not exist on kernel versions > 3.5 and has been replaced by
  UPROBES

*   SystemTap > 2.6

    You need the `dtrace` commandline tool to generate header and object files
    that are needed in the compilation and linking of MiniZinc compiler. This is
    required on Linux systems. At the time of writing this the version of
    SystemTap that these probes have been implemented and tested with is 2.6, on
    Debian 8 Stable. Later versions should work. Earlier versions might not work
    and must be tested independently. In debian based systems, SystemTap can be
    installed with apt-get

        ```
        $sudo apt-get install systemtap systemtap-sdt-dev
        ```

## Executing SystemTap scripts with MiniZinc

You can find various example SystemTap scripts in `tests/dtrace/`. To run
these scripts, a sample command would be of the form:

```
# stap [script path] -c "[command]"
```

NB: stap must be run as root (or sudo) since it compiles the SystemTap script
into a loadable kernel module and loads it into the kernel when the stap script
is running.

You need to use a PonyC compiler compiled with the DTrace support to use SystemTap.

## Writing scripts for SystemTap in MiniZinc

SystemTap and DTrace use the same syntax for creating providers and thus we
direct to the [DTrace documentation](http://dtrace.org/guide/preface.html) for
more information on the syntax and possibilities.

The MiniZinc provider for SystemTap consists of DTrace probes implemented in
the Runtime. A binary compiled with DTrace enabled will allow the user access
to the information of the probes, which work with SystemTap scripts. You can
find a list of all probes and their arguments in
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d).
The following is a toy example of a SystemTap script:

```
global gc_passes

probe process.mark("gc-start")
{
  gc_passes <<< $arg1;
}

probe end
{
  printf("Total number of GC passes: %d\n", @count(gc_passes));
}

```

This simple example will hook into the executable that the -c parameter provides
and searches for the probe named "gc-start". Once execution of the executable is
started, the script increases a counter every time the "gc-start" probe is
accessed and at the end of the run the results of the counter are printed

In SystemTap you can use the DTrace syntax of "gc-start" but you may also call
them like they are in the
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d)
file, "gc__start".

All available probes in an executable can be listed like this:

```
# stap -L 'process("<name_and_path_of_executable>").mark("*")'
```

This is useful to confirm that probes are ready to be hooked in.

## Extending dynamic tracing in MiniZinc 

You can extend the dynamic tracing implementation by adding more probes or
extra information to existing probes.  All probes are defined in
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d).
Usually their names of their module and the event that triggers them. To
install Probes in C use the macros defined in
`include/minizinc/support/dtrace.h`.  To fire a probe in the C code use the
macro `DTRACEx`; where `x` is the number of arguments of  the probe.  There is
also a macro `DTRACE_ENABLED`; its use allows for code to be only executed when
a probe is enabled.

### Adding a probe

The first step to add a probe is to add its definition into
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d).
We have split the names of the probes into two parts: its category and the
specific event. The name is split using `__`. We also add a comment to a probe
explaining when it's fired and the information contained in its arguments.

After adding the probe, we use it in the C code using the macros. Note that the
split in the name is only a single underscore in the C code. The name of the
probe should also be capitalised. We can call the probe defined as `gc__start`
using the statement `DTRACE1(GC_START,  scheduler)`. In this statement
`scheduler` is the data used as the first argument.

Then once the probe has been placed in all the appropriate places, we are ready
to recompile. Make sure to use the DTrace flag while compiling. Recompiling will
create the appropriate files using the system installation of DTrace.

### Extending a probe

We can extend a probe by adding an extra argument to the probe. Like adding a
probe, the probe definition needs to be appended in
[`include/minizinc/support/dtrace_probes.d`](../../include/minizinc/support/dtrace_probes.d)
Note that this extra information needs to be available **everywhere** the probe
is used.

Once you've added the argument, you need to change **all** instances where the
probe is in use. Note that you have to both add the new argument and change the
`DTRACE` macro. Then you can recompile the MiniZinc compiler to use your new
arguments.

*Do not forget that if you change the order of the arguments for any of the
existing nodes, you also need to change their usage in the example scripts.*
