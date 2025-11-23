# NOP-Handler

A handler which serves reads and writes instantly, without touching the data.
Additionally, the size can be specified. This is useful for performance
testing, as it can be used to remove the overhead of the underlying I/O
solution.

## Installation

- Copy the correct NOP-Handler file for your system to L:NOP-Handler.
- Copy the NOP DOSDriver definition to SYS:Storage/DOSDrivers/ for
  manual mount or DEVS:DOSDrivers/ if you want it to automount.


## Usage

Specify no name to get an unlimited stream, or a number of Bytes followed by
an optional multiplier to get a stream limited to that size. The following
multipliers are supported:
```
K = 2^10 = 1024
M = 2^20 = 1048576
G = 2^30 = 1073741824
T = 2^40 = 1099511627776
```

## Examples

### Copy from various sizes to drive
```
> copy NOP:1 Work:testfile-1
> copy NOP:123 Work:testfile-123
> copy NOP:1K Work:testfile-1K
> copy NOP:1M Work:testfile-1M
> copy NOP:1G Work:testfile-1G
> list NOHEAD NODATES Work:testfile#?
testfile-123                    123 ----rwed
testfile-1                        1 ----rwed
testfile-1K                    1024 ----rwed
testfile-1G              1073741824 ----rwed
testfile-1M                 1048576 ----rwed
```

### Attempt to fill RAM: by copying from unlimited size
```
> copy NOP: RAM:testfile
Error in writing - disk is full
Destination file "RAM:testfile" removed.
```

## Comparing difference between I/O plus computation and just computation, on same computer, running different OSs

### A4000 CSPPC 68060@50MHz, 604e@200MHz, CSPPC SCSI, AmigaOS 3.9 + WarpOS 16.1
```
> UHC:C/time asum Work:test100M.bin
2f282b84e7e608d5852449ed940bfc51  Work:test100M.bin
9.077366s
> UHC:C/time asum NOP:100M
2354e80cdd59e14be436e528284206fe  NOP:100M
5.517848s
```

### A4000 CSPPC 68060@50MHz, 604e@200MHz, CSPPC SCSI, MorphOS 1.4
```
> UHC:C/time asum Work:test100M.bin
2f282b84e7e608d5852449ed940bfc51  Work:test100M.bin
9.358638s
> UHC:C/time asum NOP:100M
9b0dc2b68a2a9938a09bf19d588b2960  NOP:100M
4.998199s
```

### A4000 CSPPC 68060@50MHz, 604e@200MHz, CSPPC SCSI, AmigaOS 4.1 FE Update 3
```
> UHC:C/time asum Work:test100M.bin
2f282b84e7e608d5852449ed940bfc51  Work:test100M.bin
10.452084s
> UHC:C/time asum NOP:100M
5b6369aeb2cffd6a641e2f9af2bd894e  NOP:100M
4.983489s
```
