# Welcome to LiteX-CNC!

This project aims to make a generic CNC firmware and driver for FPGA cards which are supported by LiteX. Configuration of the board and driver is done using json-files. The supported boards are the Colorlight boards 5A-75B and 5A-75E, as these are fully supported with the open source toolchain.

> **Supported Python version** <br>
> LitexCNC is not (yet) supported on Python 3.13. This is due to an issue in a dependency. As soon as this dependency
has been fixed, the Python requirement will be eased.

> **RV901T** <br>
> Although the RV901T is also supported by Litex, the firmware cannot be automatically build with LitexCNC, as it requires the Xilinx-software to compile the Verilog to a bit-stream. LitexCNC can be used to create the Verilog and the driver will work when the bit-stream is loaded on the board. However, there is a gap in the toolchain not covered. There are known issues with the compantibility of Litex with Xilinx.

The idea of this project was conceived by ColorCNC by *romanetz* on the official LinuxCNC and the difficulty to obtain a MESA card.

## Acknowledgements
This project would not be possible without:
- ColorCNC by *romanetz* [link](https://forum.linuxcnc.org/27-driver-boards/44422-colorcnc?start=0);
- HostMot2 (MESA card driver) as the structure of the driver has been adopted.

## Quick start
LitexCNC can be installed using pip:
```shell
pip install litexcnc
```

> **NOTE: Suffix `[cli]` deprecated!** <br>
> In previous versions the suffix `[cli]` was required. This requirement has been removed since version 1.2
> and deprecated since version 1.3. The suffix will be removed in version 1.4.

After installation of LitexCNC the following scripts are available to the user:
```shell
litexcnc install_driver
litexcnc install_litex
litexcnc install_toolchain
litexcnc build_driver
```

For a full overview on how to use the commands, please refer to the [documentation](https://litex-cnc.readthedocs.io/en/stable/).
