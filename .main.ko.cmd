savedcmd_main.ko := ld -r -m elf_x86_64 -z noexecstack --build-id=sha1  -T /home/yanovskyy/Documents/linux-6.14.3/scripts/module.lds -o main.ko main.o main.mod.o .module-common.o
