
## CAN Bootloader Firmware

Developed and maintained by Vojtěch Michal (vojta.michall@email.cz, Discord `Vojtěch Michal#7362`)<br/>
during season 2020/2021 at eForce FEE Prague Formula originally for **FSEX** and **D1** - the pair of fastest and most elegant
formulae competing in Formula Student.

The bootloader has actively used ports to STM32F105, STM32F412 and STM32F767. Other ports should be straight forward to
add as the whole project is focused on extensibility as much as possible. 

### Documentation
Refer to files located in `./doc` for guide how to integrate the bootloader into your project.

Check out the team wiki https://eforce1.feld.cvut.cz/wiki/eforce:els:software:bootloader for description of the BL CAN
protocol and internals. This information is not intended for casual user, but hackers could enjoy it.

For a documentation how to interact with the bootloader using python on a PC, refer to file `./API/flash.py`. This python
script is used for all bootloader related operations and contains help that should answer all your questions.

### Building the bootloader

Run `./compile.sh x` where 'x' is replaced by the number of MCU family you want to build for (e.g. `./compile.sh 4` to build for STM32F4).

### Flashing

The bootloader is a standalone binary located in the flash memory beside the application firmware. Therefore it is essential that you 
have both firmwares flashed and that you **NEVER PERFORM A MASS ERASE** of MCU's flash memory.

### Code style

C++20 is used in the codebase. Getting a recent compiler is a must.

Application code is found in _./Bootloader, but it would be useless without significant support in _./CANdb_, _./Drivers_, _./BSP_ as well as _./startup_.

