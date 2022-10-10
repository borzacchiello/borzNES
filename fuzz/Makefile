all: cartridge-load system-exec cpu-exec

cartridge-load:
	AFL_USE_ASAN=1 afl-cc -DAFL -g cartridge_harness.c ../src/alloc.c ../src/cartridge.c -o cartridge-load-fuzz

system-exec:
	AFL_USE_ASAN=1 afl-cc -DAFL -g system_harness.c \
		../src/mappers/*.c ../src/game_window.c ../src/window.c ../src/apu.c ../src/6502_cpu.c ../src/alloc.c ../src/cartridge.c ../src/mapper.c ../src/memory.c ../src/ppu.c ../src/system.c -o system-exec-fuzz \
		-lSDL2 -lSDL2_ttf

cpu-exec:
	AFL_USE_ASAN=1 afl-cc -DAFL -g cpu_harness.c \
		../src/mappers/*.c ../src/game_window.c ../src/window.c ../src/apu.c ../src/6502_cpu.c ../src/alloc.c ../src/cartridge.c ../src/mapper.c ../src/memory.c ../src/ppu.c ../src/system.c -o cpu-fuzz \
		-lSDL2 -lSDL2_ttf

clean:
	rm -f cartridge-load-fuzz system-exec-fuzz cpu-fuzz
