
for d in build/*/ ; do
    echo "Disassembled on $(date)" > $d/disasm
    echo "Git commit SHA: $(git rev-parse HEAD)" >> $d/disasm
    arm-none-eabi-objdump -j .isr_vector -j .data -j .bss -j .text -j .executed_from_flash -C -t -d $d/Bootloader.elf >> $d/disasm
done
