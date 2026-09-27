# Plan: port zvirt from Zig to C

This is a first pass. The goal is readable C, so clarity wins over clever optimizations.

## Scope

The port covers:
- everything in `src/`
- `build.zig` and `test_runner.zig`
- the two guest programs in `test_initfs/*/init.zig`
- the `Justfile` and `.github/workflows/ci.yml`

The prebuilt test fixtures in `test_bins/*` stay as they are.

## Ground rules for readable code

1. **One C file for each Zig file.** `src/vmm/device/virtio/queue.zig` becomes `queue.c` plus `queue.h`
   in the same place, with the same function names. You can then read the Zig and C versions side by
   side, and the Zig tests remain the reference. There's no redesign unless a Zig feature forces one.
2. **One way to report errors.** Functions return `int`: 0 on success, `-errno` on failure. Outputs go
   through pointer arguments. Each `errdefer` becomes one `goto` cleanup label, undone in reverse
   order. No `__attribute__((cleanup))` and no clever macros.
3. **Names start with `zv_`.** Both the kvm layer and the vmm layer define `Vm` and `Vcpu`, and C has one
   namespace. A bare `kvm_` prefix would clash with the kernel's `struct kvm_*` types. So the types
   become `zv_kvm_vm`, `zv_kvm_vcpu`, `zv_vm`, `zv_vcpu`, and so on.
4. **Bit layouts use shift-and-mask macros, not C bitfields.** C leaves bitfield layout to the
   compiler, and these must match exactly. This applies to:
   - the UART registers (LCR, LSR, IER, IIR)
   - the PCI `AddressPort`
   - the epoll `EventToken` (29-bit id, fd, 3-bit source)
   - the virtio-block `Token` (48-bit status, 16-bit head)
5. **Binary layouts are checked at compile time.**
   - ACPI structs marked `align(1)` become `__attribute__((packed))` structs.
   - Each gets a `_Static_assert` on its size.
   - The `comptime` offset checks on `bootparam.h` become `_Static_assert(offsetof(...))`.
6. **Atomics keep their exact memory ordering.** The virtio ring index uses acquire/release. That's
   needed for correctness, not speed, so it stays as written with `<stdatomic.h>`.
7. **The `std.Io` parameter goes away everywhere.** Mutex locking can't fail in C, which removes a lot
   of `try` noise.
8. **No side effects inside `assert()`.** A Zig unwrap like `allocate_specific(4).?` becomes an
   explicit `if` followed by `abort()`, so it isn't compiled out in release builds.

## How Zig features map to C

| Zig | C |
|---|---|
| `union(enum)` (e.g. `ExitReason`, `FileEngine`, `Request`) | `enum` kind + `union` + `switch` |
| `IdAllocator(N)` (a generic type) | bitmap struct with a runtime `count` |
| `Lazy(T)` (futex-based) | `pthread_once` |
| `VirtioPci(Device)` (generic) | one struct with a `device_kind` enum and a union of block/net |
| `check_cfg_access` (`inline for` over `@typeInfo`) | explicit table of `{offsetof, sizeof}` for `virtio_pci_common_cfg` |
| `std.Treap` in `mmio_bus` | fixed array with linear search (≤10 devices plus BARs) |
| `std.Deque` (UART rx, net tx) | small hand-written ring buffer |
| `std.ArrayList` | `{ptr, len, cap}` struct with `realloc` growth |
| `allocPrint` for the kernel command line | `snprintf` into a bounded buffer |
| `std.Io.Event` (vCPU start) | mutex + condition variable |
| `MmioDevice` (`*anyopaque` + fn pointers) | the same thing in C: `void *ctx` plus function pointers |
| `std.fmt.parseInt(…, 0)` | `strtoull(…, 0)` with an end-pointer check |

**One design decision is forced.**
- `VirtioCore` embeds 2048 per-queue states inline. Each has its own arena allocator, and `kick()`
  returns an `ArrayList` of chains of up to 16 requests.
- A direct C translation with fixed arrays would cost around 200 MB.
- `kick()` becomes `zv_virtq_pop_chain(q, &chain)`, which returns one chain per call from a stack
  variable. That's the usual C pattern, it needs no allocation, and it's arguably easier to read.
- The per-queue arena goes away with it.

## Libraries

| Need | Choice | Alternatives considered | Why |
|---|---|---|---|
| KVM, virtio, tun, bootparam definitions | **Kernel uapi headers** (`linux/kvm.h`, `linux/virtio_*.h`, `linux/if_tun.h`, `asm/bootparam.h`) | — | These are exactly what `@cImport` already uses, so there's no new dependency. Code gets simpler too: `run->unnamed_0.io` becomes `run->io`. |
| Threads, locks, one-time init | **pthreads** | C11 `<threads.h>` | vCPUs are stopped with `pthread_kill(SIGUSR1)`, which needs pthreads anyway. It's also what TSan understands best. |
| Async disk I/O | **liburing** | raw `io_uring` syscalls; libaio | The code already uses `std.os.linux.IoUring`, whose API mirrors liburing, so the port is nearly line-for-line. Raw syscalls mean hand-managing ring memory, which is the opposite of readable. libaio is older and weaker. 2.15 is installed locally, but Ubuntu 24.04 in CI ships an older one (believed 2.5, to be confirmed). Stick to basic calls: `get_sqe`, `prep_read/write`, `submit`, `peek_cqe`, `register_eventfd`. |
| Command-line parsing | **`getopt_long`** (libc) | argtable3; hand-written parser | Nine options don't justify a dependency, and it removes the zig-cli fetch. |
| Test harness | **Small hand-written runner** (~150 lines) | cmocka, Criterion, greatest/utest.h | None of these is installed. The deciding factor is that `test_runner.zig` fails a test if any error is logged, checks leaks per test, and prints `START/PASS/FAIL/SUMMARY` lines for CI. The sigaction test and the fd-leak detector also depend on per-process state. A runner that forks once per test, filters by name, and prints the same lines gives all of that without a dependency, and the CI output stays the same. |
| Leak and safety checks | **ASan + LSan + UBSan** in Debug, **TSan** job kept | valgrind | They replace Zig's `testing.allocator` leak check and win back some of Zig's bounds and overflow checks. |
| mmap and fd leak detectors | **Ported as-is** (`test_utils/mmap.c`, fd count via `/proc/self/fd`) | — | ASan doesn't track `mmap`. The hash map becomes a small array. |
| SHA-256 (host tests and the guest program) | **Vendored public-domain `sha256.c`** | OpenSSL; calling `sha256sum` | The static guest program needs it too. One small file is easiest for both sides. |
| PRNG for the disk-write test | **Port Zig's `DefaultPrng`** into a shared header | — | The host test and the guest program must produce the same byte stream. Believed to be xoshiro256++ seeded through SplitMix64; confirm against Zig's source and add a known-output test vector generated by the Zig code before deleting it. |
| Logging | **~30-line macro header** with a scope name and a runtime level | log.c, zlog | This matches `std.log.scoped` plus `--log-level`, and the test runner can hook error-level logs. |
| Build | **CMake** | Make; Meson (not installed) | CMake does sanitizer variants, `ctest` filtering, and the static guest target cleanly. A plain Makefile would work but becomes messy with debug/tsan/release trees. |
| Static guest programs | **`clang -static`** against glibc | musl-gcc (not installed); nolibc (not present) | Each program runs alone inside a cpio initramfs. Ubuntu's `libc6-dev` and Arch's glibc both ship `libc.a`. |

## Places where Zig was protecting us

Zig's bounds-checked slices were quietly guarding guest-controlled input: virtio descriptors,
MMIO/PIO offsets, and PCI config access. In C:
- `zv_guest_memory_as_slice(mem, gpa, len, &out)` is the only way to reach guest memory.
- Every offset from the guest is range-checked explicitly before it's used as an index.
- One existing bug stays as-is for now: `contains_range` in `memory.zig` can overflow on
  `addr + size`. It's translated unchanged and flagged with a `TODO`, keeping this pass a pure
  translation.

## Phases

Each phase is done when the matching Zig tests pass in C. The Zig tree keeps building alongside until
the port catches up.

1. **Scaffolding.** `csrc/` tree, CMake with clang, the log header, the test runner, the mmap and fd leak detectors.
2. **`utils/`.** eventfd, epoll, idalloc, mac, tap. Each keeps its unit tests. `lazy.zig` has no C
   file; see the conventions section.
3. **`kvm/`.** ioctl wrapper, KVM system, VM, vCPU, CPUID.
4. **Guest setup.** Guest memory, bzImage loader, x86 layout, GDT, page tables, ACPI tables (RSDT,
   XSDT, MADT).
5. **First working VM.** `Vm`, `VCpu`, the IO bus, UART 16550, CMOS/RTC. Milestone tests, in order:
   COM1 prints "H", Linux reaches shutdown, the console appears, login and reboot work, SMP with 16
   CPUs.
6. **Virtio block over MMIO.** The synchronous file engine, then the liburing engine, the virtqueue,
   virtio-mmio, block. Tests: read/write for sync and async, one CPU and SMP.
7. **PCI.** PCI bus, bridge, config space, BAR allocator, MSI-X, virtio-pci. Tests: the PCI variants
   of the block tests.
8. **Virtio-net over a tap device.** Tests: the net tests over MMIO and PCI with arping.
9. **CLI.** `main.c` using `getopt_long`, with the memory, drive and net parsers made testable (this
   resolves the `TODO` in `main.zig`).
10. **Tooling and cleanup.**
    - Port the guest programs to static C.
    - Update the Justfile's `initrds` recipe and the hardcoded `zig-out/initrds/*.img` paths in the
      tests.
    - Switch CI from `zig fmt` to `clang-format --dry-run -Werror`, keeping the Debug, TSan and
      Release jobs.
    - Delete the Zig code.

## Conventions (settled in phase 1)

Every later phase follows these, so check new code against them.

- **File mapping.** `foo.zig` becomes `csrc/<same dir>/foo.{c,h}`. A module's `root.zig` that
  contains real code becomes `<module>/<module>.{c,h}`, e.g. `test_utils/root.zig` becomes
  `test_utils/test_utils.{c,h}`.
  `test_runner.zig` becomes `csrc/test_runner.{c,h}`.
- **Tests live in `foo_test.c` next to `foo.c`.** This is the one exception to file-for-file parity.
  Zig keeps `test` blocks inside the source file, but in C that would compile the tests into the
  production library.
- **Registration is explicit.** Each `*_test.c` ends with a table of `{name, function}` and exports
  one `struct zv_test_suite`. There is no `__attribute__((constructor))` magic.
- **One test executable per Zig test step in `build.zig`.** Each one has a `tests_main.c` that
  lists its suites. `test_utils` exists so far. `utils`, `kvm`, `vmm` and `exe` come in later
  phases.
- **Test names and filters.** A full test name is `"<suite>: <test name>"`, where the suite is the
  Zig module path, e.g. `test_utils.mmap: mmap detector works`. Filters are command-line arguments
  to the test executable. Each one matches as a substring, like `-Dtest-filter`.
- **Test runner behaviour** (same as `test_runner.zig`):
  - Each test runs in a forked child with the log level set to warn.
  - A test fails on a failed `ZV_EXPECT`, a crash or signal, any sanitizer report, any logged
    error, or a memory leak (debug preset only).
  - Output lines are `START`/`PASS`/`SKIP`/`FAIL`/`SUMMARY`, and the exit code is 1 if anything
    failed.
- **Includes are relative to `csrc/`**, e.g. `#include "utils/log.h"`. This mirrors Zig's module
  imports. Order: own header, then project headers, then system headers.
- **Logging.** Each file does `#define LOG_SCOPE "vmm"` and calls
  `zv_log_err(LOG_SCOPE, "fmt", ...)`. Output looks like Zig's default logger:
  `error(vmm): message`.
- **A `root.zig` that only re-exports other files gets no C file** (e.g. `utils/root.zig`). The rule
  above applies only when `root.zig` contains real code. Callers include the specific header they
  need.
- **`lazy.zig` gets no C file.** `Lazy(T)` is used once, for `kvm_system` in `vmm/root.zig`. A
  `pthread_once` callback takes no argument, so a generic wrapper can't be built on it. Phase 5
  writes the `pthread_once` pattern at that call site and caches the init function's return code,
  which keeps the "failed evaluation is cached" behaviour. The three Lazy tests would only test
  libc, so they are not ported.
- **Return values.**
  - Production code returns 0 or `-errno`, with outputs through pointer arguments.
  - A function that produces a count returns it directly, like `read(2)`. The return type is
    `ssize_t` or `int`: `>= 0` is the count, `< 0` is `-errno`. Examples: `zv_epoll_pwait`, the tap
    read/write functions.
  - A Zig optional (`?T`) becomes the same `int` convention. The function returns 0 and fills the
    out pointer when there is a value. For "no value" it returns a specific errno that the header
    documents, e.g. `-ENOSPC` for a full id allocator or `-EBUSY` for an id already taken.
  - A Zig optional *parameter* becomes a by-value struct with a `NONE` kind, e.g. `?IoResult` in
    `run_once` becomes `struct zv_kvm_io_result` with `ZV_KVM_IO_RESULT_NONE`. It can be stored
    between calls, which the phase 5 vCPU loop does.
  - Test-only helpers (`test_utils/`) return a bool or a count. They abort when the environment is
    broken (e.g. `/proc/self/fd` won't open, or out of memory).
- **A file that only wraps `@cImport` gets no C file** (e.g. `kvm/abi.zig`). Each C file includes
  the uapi header (`<linux/kvm.h>`) directly.
- **`zv_kvm_tests` has no Zig counterpart.** `build.zig` has no kvm test step; the Zig kvm code is
  only exercised through the vmm tests. The C kvm smoke tests are new, the one exception to "one
  test executable per Zig test step". Tests create every KVM object themselves: a VM belongs to the
  process that created it, so nothing may open `/dev/kvm` in the runner's parent process (this
  includes the `pthread_once` KVM getter in phase 5).
- **Error paths close what Zig leaks.** `Kvm.init` leaves `/dev/kvm` open when the version check
  fails, and `Vm.create_vcpu` leaves the vCPU fd open when its mmap fails. The `goto` cleanup closes
  both. This is intentional; other known bugs (like `contains_range`) are still translated as-is.
- **The kvm layer never logs**, as in Zig. A logged error fails any test that hits it, and callers
  (e.g. the vCPU loop on an unknown exit) decide how serious an error is.
- **Closing fds in `deinit`.** Zig's `std.debug.assert(close(...) == 0)` becomes
  `int rc = close(fd); assert(rc == 0); (void)rc;`, and the fd field is then set to -1.
- **glibc's `struct epoll_event` is packed on x86_64.** Read and write its `data` field by value,
  never through a pointer, or `-Waddress-of-packed-member` fails the build.
- **Strict C17 pitfalls** (these fail under `-Wpedantic -Werror`):
  - Write `= {0}`, not `= {}`.
  - Use `_Noreturn`, `__typeof__`, and `[]` flexible array members.
  - Don't use statement expressions or `__auto_type`.
  - Variadic macros take the format string inside `__VA_ARGS__`, so a call never has zero variadic
    arguments.
- **Formatting.** `csrc/.clang-format` gives 4-space indent, 100 columns, and Linux-style function
  braces. Every file must pass `clang-format --dry-run -Werror`.

### Building and testing

```sh
cd csrc
cmake --preset debug          # debug = ASan+UBSan, tsan = ThreadSanitizer, release = -O3
cmake --build --preset debug
ctest --preset debug
cd ..
build/debug/zv_test_utils_tests mmap    # run one executable with a name filter
```

Tests run from the `zvirt/` root, because they open fixtures such as `test_bins/bzImage` by
relative path, like the Zig tests. `ctest` sets this working directory itself. When running a test
executable by hand, run it from `zvirt/`.

Build output goes to `zvirt/build/<preset>/` (git-ignored). The presets use Ninja. CI will need
`clang`, `cmake` and `ninja-build` installed once the C job is added.

## Decisions

- **Compiler: clang.** CMake sets `CMAKE_C_COMPILER=clang`, and CI installs `clang`. ASan, UBSan
  and TSan come from clang as well. clang-format already fits the plan for formatting checks.
- **Language: C17.** Flags are `-std=c17 -D_GNU_SOURCE`, which keeps the language itself ISO C17.
  `_GNU_SOURCE` exposes the Linux and POSIX APIs (`pipe2`, `eventfd`, `pthread_kill`, …) that
  strict mode hides. clang still accepts `__attribute__((packed))` under `-std=c17`.
- **Warnings:** `-Wall -Wextra -Wpedantic -Werror` in Debug and CI.
- **Build: CMake.** It has Debug (ASan+UBSan), TSan and Release variants. Guest programs are built
  with `clang -static`.
- **Layout: `csrc/` next to `src/`.** Both trees build and test side by side until the port is done.
  Then `csrc/` is renamed to `src/` in phase 10.
