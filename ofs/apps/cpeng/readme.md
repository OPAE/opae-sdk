# hps #

## SYNOPSIS ##

`hps OPTIONS SUBCOMMAND SUBCOMMAND_OPTIONS`

## DESCRIPTION ##
`hps` is an application to aid in the development, deployment, and debugging of
an HPS (hard processor system) on an FPGA that is designed with one. The current
version of the hps program assumes an AFU (accelerator functional unit) is
configured into the FPGA that can be discovered/accessed via an OPAE library and
used for communicating with the HPS. When invoked with one of its subcommands,
hps will enumerate the AFU for that subcommand before executing it.


## OPTIONS ##
  -h,--help

    Print this help message and exit

  -p,--pci-address \<address\>

    Use <address> in the filter for locating the AFU where <address> must be in
    the following format: [<domain>:]<bus>:<device>.<function>

  -l,--log-level \<level\>

    stdout logging level. Must be one of:
    {trace,debug,info,warning,error,critical,off}
    Default is 'info'.

  -s,--shared

    open in shared mode, default is off
  -t,--timeout \<timeout\>

    Program timeout in milliseconds. Default is 60000.

## SUBCOMMANDS ##
  `cpeng`

    Run copy engine command. The copy engine command is used to program copy
    engine AFU registers to copy an image file from the host into the FPGA DDR.
    The copy engine AFU is logic that can be used to copy an HPS image into the
    FPGA DDR. When the HPS boots, the first stage boot loader will load an image
    from a specific offset in DDR that will be used to transition into the second
    stage boot loader and subsequently boot into the embedded Linux that is also
    part of this image.

  `cpeng` options

  -h,--help

    Print this help message and exit

  -f,--filename \<filename\>


    Path to image file to copy. Default is 'hps.img'

  -d,--destination \<offset\>

    DDR Offset. Default is 0.

  -t,--timeout \<cpeng timeout\>

    Timeout of cpeng command in microseconds.
    Default is 60000000.

  -c,--chunk \<size\>

    Split the copy into chunks of size <size>. 0 indicates no chunks.

## EXAMPLES ##
The following example loads the image from a file called 'hps.img' into
offset 0x0000 in one chunk
```console
hps cpeng

```
The following example loads the image from a file called 'hps_01.bin' into
offset 0x1000 in 1Kb chunks.
```console
hps cpeng -f hps_01.bin -d 0x1000 -c 1024
```



