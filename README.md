# Push Swap Tester

A modular CLI tester for the 42 `push_swap` project.

It validates:
- parser/error behavior (`EXPECT_ERROR` cases)
- sorting correctness with `checker`
- move-based scoring (`100 < 700`, `500 < 5500`)
- memory leaks with `valgrind`

It also provides:
- ASCII table output
- Unicode status symbols (`✔`, `✘`)
- live progress bar with `PASS/FAIL` counters
- final success/fail summary

---

## Project Structure

```text
push_swap_tester/
├── Makefile
├── includes/
│   ├── tester.h
│   ├── colors.h
│   └── config.h
├── src/
│   ├── engine/
│   │   ├── executor.c
│   │   ├── judge.c
│   │   └── ui.c
│   ├── utils/
│   │   ├── generator.c
│   │   └── str_utils.c
│   └── main.c
└── tests/
    └── cases.c
```

---

## Requirements

- Linux/macOS
- C compiler (`cc`)
- `make`
- `valgrind`
- your compiled `push_swap` binary
- a checker binary (`checker` / `checker_linux` / `checker_Mac`)

### Recommended Location

For the smoothest setup, keep both binaries in your push_swap project root:

- `push_swap` in project root
- `checker` in project root

Example:

```text
push_Swap/
├── push_swap
├── checker
└── ...
```

If binaries are not in root, use environment variables (`PUSH_SWAP_BIN`,
`PUSH_SWAP_CHECKER`) as shown below.

---

## Build

```bash
make
```

Rebuild from scratch:

```bash
make re
```

---

## Binary Detection (No Hardcoded Paths Needed)

The tester auto-detects binaries from common locations:
- `./push_swap`
- `../push_swap/push_swap`
- `../push_Swap/push_swap`

Checker candidates:
- `./checker`
- `../push_swap/checker`
- `../push_Swap/checker`
- `./checker_linux`
- `./checker_Mac`

You can also override paths with environment variables:

```bash
PUSH_SWAP_BIN=/absolute/path/to/push_swap \
PUSH_SWAP_CHECKER=/absolute/path/to/checker \
./push_swap_tester
```

---

## Usage

Default:

```bash
./push_swap_tester
```

With mode:

```bash
./push_swap_tester --mode simple
./push_swap_tester --mode medium
./push_swap_tester --mode complex
./push_swap_tester --mode adaptive
```

### Mode Behavior

- `simple`: manual test cases only (`tests/cases.c`)
- `medium`: manual + random `n=100`
- `complex`: manual + random `n=500`
- `adaptive`: manual + random `n=100` + random `n=500`

---

## Output Columns

- `RESULT`: PASS / FAIL
- `SORT`: checker status (`OK`, `KO`, `SKIP`)
- `LEAK`: valgrind status (`CLEAN`, `LEAK`, `CRASH`, `SKIP`)
- `CHARS`: input character count used for that test
- `SCORE`: move score (`x/5` for valid sorting tests)

---

## Typical Workflow

1. Build your `push_swap` project.
2. Place tester near your project (or set env path overrides).
3. Run:

```bash
./push_swap_tester --mode simple
```

4. If simple passes, run heavier suites:

```bash
./push_swap_tester --mode medium
./push_swap_tester --mode complex
```

---

## Troubleshooting

### `push_swap binary not found`

Set explicit path:

```bash
PUSH_SWAP_BIN=/path/to/push_swap ./push_swap_tester
```

### `checker` not found

Set explicit checker path:

```bash
PUSH_SWAP_CHECKER=/path/to/checker ./push_swap_tester
```

### `valgrind not found`

Install valgrind, then retry.

### You suspect tester vs project mismatch

Replay a failing case directly:

```bash
ARG="3 2 1 0 -1"
./push_swap "$ARG" | ./checker "$ARG"
```

If direct run returns `OK` but tester says `KO`, verify tester version and rebuild:

```bash
make re
```

---

## Notes

- Manual cases live in `tests/cases.c`.
- You can freely add/remove cases there.
- Exit code is non-zero when any test fails.
