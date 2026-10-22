# qemu-riscv32-uart

Firmware bare-metal para RISC-V de 32 bits que corre en `qemu-system-riscv32 -machine virt`, sin SDK, sin libc y sin hardware físico.
*Bare-metal 32-bit RISC-V firmware for `qemu-system-riscv32 -machine virt`, with no SDK, no libc and no physical hardware.*

[English below](#english)

## Español

### Qué hace

- **Arranque propio** (`src/start.S`): el hart 0 configura `gp` y `sp`, pone `.bss` en cero y salta a `main`. Los otros harts quedan en `wfi`. El linker script está en `link.ld`.
- **Driver NS16550A por polling** (`src/uart.c`): divisor de baudios, FIFO, `printf` propio y lectura de línea con eco y backspace. También tiene un modo loopback interno (MCR bit 4).
- **Interrupciones del timer CLINT** a 100 Hz. `mtime`/`mtimecmp` de 64 bits se leen y escriben en partes de 32 bits sin carreras.
- **Traps sincrónicos**: `ecall` e instrucciones ilegales de 16 y 32 bits, avanzando `mepc` según el largo real de la instrucción. Cualquier otro trap imprime `mcause`/`mepc`/`mtval` y sale con código 100.
- **Salida hacia el host** con el dispositivo `sifive_test` de QEMU: `exit` termina QEMU con estado 0 y `fail N` con estado N. Así CI puede leer el resultado sin parsear texto.
- **Helpers freestanding** (`src/fmt.c`): `snprintf` reducido, parseo de `int32` con detección de overflow y división 64/32 sin libgcc ni compiler-rt (RV32 no tiene división de 64 bits).
- **Shell** por UART con `ping`, `add a b`, `ticks`, `uptime`, `traps`, `echo`, `help`, `exit` y `fail n`.

Al arrancar corre 5 autotests en el propio target: `fmt`, `ecall`, `illegal`, `loopback` (UART) y `timer` (5 ticks, con una cota de tiempo medida con `mtime`).

```
rv32 uart demo (hart 0, misa=401411ad)
  fmt: ok
  ecall: ok
  illegal: ok
  loopback: ok
  timer: ok
selftest: 5/5 passed
ready
> add 2 40
42
```

### Requisitos y uso

`clang` (con target riscv32), `ld.lld` y `qemu-system-riscv32` (paquete `qemu-system-misc`). Probado con clang 19 / QEMU 10.0 en Debian, y en CI con los paquetes de Ubuntu 24.04.

```sh
make            # build/firmware.elf
make run        # consola interactiva; Ctrl-A x para salir
make test       # tests del host + tests en QEMU
make size
```

### Tests

1. **Host** (`tests/test_host.c`, con gcc y clang bajo ASan + UBSan):
   - `fmt_snprintf` contra el `snprintf` de libc, incluidos truncado y `INT_MIN`;
   - parseo de `int32` en sus bordes y con overflow;
   - `fmt_udiv64` contra la división nativa con 48 casos fijos y 100 000 aleatorios;
   - el ring buffer SPSC, incluido el wrap de los índices de 32 bits.
2. **Autotests en el target**: la build `firmware-auto.elf` corre los 5 autotests y sale por `sifive_test`; CI falla si el estado no es 0.
3. **Integración por consola** (`tests/run_qemu.py`, 5 escenarios, cada uno con un QEMU nuevo):
   - comandos y errores (overflow, uso inválido, comando desconocido), contadores de traps;
   - edición de línea con backspace y una línea de 300 bytes truncada sin crash;
   - el timer avanza (≥ 10 ticks en 300 ms) y `uptime`;
   - `fail 7` → QEMU sale con estado 7;
   - tres comandos enviados de una vez sin perder bytes.

### Tamaño

clang 19 `-O2`: `.text` 3 922 B, `.rodata` 576 B, `.bss` 24 B, más 16 KiB de stack reservados en el linker script.

### Limitaciones

- La UART va por polling. El ring SPSC (`include/ring.h`) está probado en el host, pero todavía no está conectado a una RX por interrupción vía PLIC.
- Usa solo modo máquina; no hay modo supervisor ni PMP.
- Lo probé solo en QEMU. En hardware real (por ejemplo, un SoC SiFive) cambian el mapa de memoria, el reloj de la UART y el mecanismo de salida.
- El `printf` reducido soporta `%d %u %x %c %s %p %%` con ancho y relleno con ceros; no soporta `-`, `l` ni flotantes.

Licencia: Apache-2.0.

---

## English

### What it does

- **Own startup code** (`src/start.S`): hart 0 sets up `gp` and `sp`, zeroes `.bss` and jumps to `main`. The other harts park in `wfi`. The linker script is `link.ld`.
- **Polled NS16550A driver** (`src/uart.c`): baud divisor, FIFOs, a small `printf`, and line input with echo and backspace. It also has an internal loopback mode (MCR bit 4).
- **CLINT timer interrupts** at 100 Hz. The 64-bit `mtime`/`mtimecmp` are read and written as 32-bit halves without races.
- **Synchronous traps**: `ecall` and 16- and 32-bit illegal instructions, advancing `mepc` by the real instruction length. Any other trap prints `mcause`/`mepc`/`mtval` and exits with status 100.
- **Exit to the host** through QEMU's `sifive_test` device: `exit` ends QEMU with status 0 and `fail N` with status N, so CI can read the result without parsing text.
- **Freestanding helpers** (`src/fmt.c`): a small `snprintf`, `int32` parsing with overflow detection, and a 64/32 division with no libgcc or compiler-rt (RV32 has no 64-bit divide).
- **UART shell** with `ping`, `add a b`, `ticks`, `uptime`, `traps`, `echo`, `help`, `exit` and `fail n`.

At boot it runs 5 on-target self-tests: `fmt`, `ecall`, `illegal`, `loopback` (UART) and `timer` (5 ticks, with a time bound measured through `mtime`).

### Requirements and usage

`clang` (with the riscv32 target), `ld.lld` and `qemu-system-riscv32` (package `qemu-system-misc`). Tested with clang 19 / QEMU 10.0 on Debian, and in CI with Ubuntu 24.04 packages. `make`, `make run`, `make test` and `make size` work as described above.

### Tests

1. **Host** (`tests/test_host.c`, gcc and clang under ASan + UBSan):
   - `fmt_snprintf` against libc's `snprintf`, including truncation and `INT_MIN`;
   - `int32` parsing at its edges and on overflow;
   - `fmt_udiv64` against native division with 48 fixed cases and 100,000 random ones;
   - the SPSC ring buffer, including 32-bit index wrap.
2. **On-target self-tests**: the `firmware-auto.elf` build runs the 5 self-tests and exits through `sifive_test`; CI fails on a non-zero status.
3. **Console integration** (`tests/run_qemu.py`, 5 scenarios, each with a fresh QEMU):
   - commands and errors (overflow, bad usage, unknown command), trap counters;
   - line editing with backspace and a 300-byte line truncated without a crash;
   - the timer advances (≥ 10 ticks in 300 ms) and `uptime`;
   - `fail 7` → QEMU exits with status 7;
   - three commands sent in one burst with no lost bytes.

### Size

clang 19 `-O2`: `.text` 3,922 B, `.rodata` 576 B, `.bss` 24 B, plus 16 KiB of stack reserved in the linker script.

### Limitations

- The UART is polled. The SPSC ring (`include/ring.h`) is tested on the host but not yet wired to an interrupt-driven RX through the PLIC.
- Machine mode only; no supervisor mode or PMP.
- I tested it only on QEMU. Real hardware (for example a SiFive SoC) has a different memory map, UART clock and exit mechanism.
- The small `printf` supports `%d %u %x %c %s %p %%` with width and zero padding; no `-`, `l` or floats.

License: Apache-2.0.
