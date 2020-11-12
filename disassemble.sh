echo "Disassembled on $(date)" > build/disasm
echo "Git commit SHA: $(git rev-parse HEAD)" >> build/disasm
arm-none-eabi-objdump -j .isr_vector -j .data -j .bss -j .text -j .executed_from_flash -C -t -d build/Bootloader.elf >> build/disasm