# Contributing to CRUMBS

Thank you for your interest in contributing to CRUMBS! This guide covers how to build, test, and contribute to the project.

## Getting Started

### Prerequisites

**For all platforms:**

- Git
- C compiler (gcc, clang, or MSVC)
- CMake 3.13 or later

**For Arduino development:**

- Arduino IDE or PlatformIO
- AVR toolchain (included with Arduino)

**For Linux development:**

- linux-wire library ([installation guide](platform-setup.md#install-linux-wire-dependency))

### Cloning the Repository

```bash
git clone https://github.com/feastorg/CRUMBS.git
cd CRUMBS
```

---

## Building CRUMBS

### CMake Build (Linux/Native)

**Basic build:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**With Linux HAL enabled:**

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCRUMBS_ENABLE_LINUX_HAL=ON \
    -DCRUMBS_BUILD_EXAMPLES=ON

cmake --build build --parallel
```

**With tests:**

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCRUMBS_ENABLE_TESTS=ON

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

**Install system-wide:**

```bash
sudo cmake --install build --prefix /usr/local
```

### Arduino Build

CRUMBS is designed to work seamlessly with Arduino IDE and PlatformIO:

**Arduino IDE:**

1. Copy CRUMBS folder to `~/Arduino/libraries/`
2. Restart Arduino IDE
3. Open any example from File → Examples → CRUMBS

**PlatformIO:**

```ini
# platformio.ini
lib_deps =
    cameronbrooks11/CRUMBS@^0.12.0
```

Or for local development (see [Local Development](#local-development-with-platformio) below).

---

## Testing

### Unit Tests

CRUMBS includes comprehensive unit tests for core functionality:

```bash
# Build with tests enabled
cmake -S . -B build -DCRUMBS_ENABLE_TESTS=ON
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
./build/test_encode_decode
./build/test_crc_failure
```

**Test coverage:**

- `test_encode_decode` — Frame encoding/decoding
- `test_crc_failure` — CRC validation and error handling
- `test_handlers` — Handler registration and dispatch
- `test_msg_helpers` — Message builder/reader helpers
- `test_version` — Version macro validation
- `test_context` — Context initialization
- `test_peripheral_flow` — Peripheral receive/reply flow
- `test_reply_flow` — SET_REPLY pattern
- `test_set_reply` — Request opcode handling
- `test_scan_fake` — Scanner logic (simulated I²C)
- `test_reply_handler` — Reply handler registration, dispatch, and fall-through

### Integration Tests

Integration tests require physical I²C hardware:

**Arduino integration:**

1. Upload `examples/core_usage/arduino/basic_peripheral/` to peripheral device
2. Upload `examples/core_usage/arduino/basic_controller/` to controller device
3. Connect I²C (SDA/SCL) between devices
4. Verify LED blinks in response to commands

**Linux integration:**

1. Connect Arduino peripheral to Raspberry Pi I²C bus
2. Build and run `examples/core_usage/linux/simple_controller/`
3. Verify communication via serial monitor and Linux output

### Test-Driven Development

When adding new features:

1. **Write test first** (create `tests/test_<feature>.c`)
2. **Verify test fails** (no implementation yet)
3. **Implement feature** (in `src/core/` or `src/hal/`)
4. **Verify test passes**
5. **Add documentation** (update relevant docs)

---

## Documentation

### Documentation Standards

We follow a concise in-source documentation style. See [docs/doxygen-style-guide.md](docs/doxygen-style-guide.md) for complete guidelines.

**Key requirements:**

- All public functions must have docblocks
- Use Doxygen-compatible format
- Include `@brief`, `@param`, `@return` as needed
- Keep descriptions focused and concise

**Example:**

```c
/**
 * @brief Encode a CRUMBS message into wire format
 *
 * @param msg Message to encode
 * @param buffer Output buffer (min 31 bytes)
 * @param buffer_len Size of output buffer
 * @return Encoded frame length (4–31 bytes), or 0 on error
 */
size_t crumbs_encode_message(const crumbs_message_t *msg,
                             uint8_t *buffer,
                             size_t buffer_len);
```

### Documentation Check

Before submitting a PR, run the documentation checker:

```bash
./scripts/doccheck.sh
```

This runs Doxygen and reports warnings for missing/incomplete documentation.

**CI behavior:**

- Documentation check runs on all PRs (non-blocking)
- Warnings reported but don't fail build
- Goal: Incrementally improve documentation quality

### User Documentation

User-facing documentation lives in `docs/`:

- `protocol.md` — Wire format specification
- `api-reference.md` — Complete API documentation
- `architecture.md` — Design and concepts
- `platform-setup.md` — Installation guides
- `examples.md` — Learning path
- `create-a-family.md` — Guide for custom device families

When changing public APIs:

1. Update relevant documentation files
2. Add cross-references where appropriate
3. Update code examples if needed
4. Check for broken links

---

## Local Development with PlatformIO

When developing changes to CRUMBS itself, test locally before publishing:

**Development workflow:**

1. Edit example's `platformio.ini`:

   ```ini
   # Change from registry version
   lib_deps =
       cameronbrooks11/CRUMBS@^0.12.0

   # To local symlink
   lib_deps =
       symlink://../../../../
   ```

2. Make changes to CRUMBS source

3. Build example to test:

   ```bash
   cd examples/core_usage/arduino/basic_peripheral
   pio run
   ```

4. Revert `platformio.ini` before committing:

   ```ini
   lib_deps =
       cameronbrooks11/CRUMBS@^0.12.0
   ```

**Path explanation:**

- From: `examples/core_usage/arduino/basic_peripheral/`
- To repo root: `../../../../`
- PlatformIO expects `library.json` at target

**Clean build after switching:**

```bash
pio run --target clean
pio run
```

---

## Code Style

### C Code Style

**General principles:**

- Follow Linux kernel style (K&R variant)
- 4-space indentation (no tabs)
- 80-column line limit (flexible for readability)
- Consistent naming: `crumbs_snake_case` for public APIs

**Function naming:**

```c
// Public API: crumbs_ prefix
crumbs_encode_message()
crumbs_controller_send()

// Internal helpers: crumbs_ prefix, descriptive
crumbs_compute_crc8()
crumbs_validate_payload_length()
```

**Error handling:**

```c
// Return 0 on success, negative on error
int crumbs_do_something(void) {
    if (bad_condition) {
        return -1;  // Generic error
    }
    if (specific_problem) {
        return -2;  // Specific error code
    }
    return 0;  // Success
}
```

### Header Organization

**Public headers** (`src/*.h`):

- Minimal includes (only what's necessary)
- Well-documented with Doxygen comments
- Stable API (avoid breaking changes)

**Internal headers** (`src/core/*.h`, `src/hal/*.h`):

- Platform-specific or implementation details
- May change between versions
- Document key structures and algorithms

### Platform Guards

Use platform guards for platform-specific code:

```c
// In HAL implementation files
#ifdef ARDUINO
    #include <Wire.h>
    // Arduino-specific code
#endif

#ifdef __linux__
    #include <linux/i2c-dev.h>
    // Linux-specific code
#endif
```

---

## Contributing Workflow

### 1. Create an Issue

Before starting work:

- Check existing issues for duplicates
- Create new issue describing problem/feature
- Discuss approach with maintainers

### 2. Fork and Branch

```bash
# Fork on GitHub, then clone
git clone https://github.com/YOUR_USERNAME/CRUMBS.git
cd CRUMBS

# Create feature branch
git checkout -b feature/your-feature-name
```

**Branch naming:**

- `feature/` — New features
- `fix/` — Bug fixes
- `docs/` — Documentation only
- `refactor/` — Code restructuring

### 3. Make Changes

**Best practices:**

- Small, focused commits
- One logical change per commit
- Write descriptive commit messages
- Include tests for new features
- Update documentation

**Commit message format:**

```text
Short summary (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain what and why, not how (code shows how).

Fixes #123
```

### 4. Test Your Changes

```bash
# Run unit tests
cmake -S . -B build -DCRUMBS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Check documentation
./scripts/doccheck.sh

# Test on target platform (Arduino/Linux)
# ... build and run examples ...
```

### 5. Submit Pull Request

**Before submitting:**

- [ ] All tests pass
- [ ] Documentation updated
- [ ] No compiler warnings
- [ ] Code follows style guide
- [ ] Commit history is clean

**PR description should include:**

- What changed and why
- Link to related issue(s)
- Testing performed
- Breaking changes (if any)

### 6. Address Review Feedback

- Respond to comments promptly
- Make requested changes
- Push updates to same branch
- Request re-review when ready

---

## Maintenance Guidelines

### For Library Maintainers

**Core principles:**

- Keep core strictly C (no C++ in `src/core/`)
- HALs may use platform idioms (Arduino C++, Linux ioctl)
- No dynamic allocation in core
- Maintain backward compatibility when possible

**Adding HAL support:**

1. Create `src/hal/<platform>/` directory
2. Implement write and read primitives
3. Add platform guards to prevent cross-contamination
4. Update CMake to conditionally build HAL
5. Add examples for new platform
6. Document setup in `platform-setup.md`

**CRC implementation:**

The project includes a CRC-8 generator script:

```bash
# Generate CRC-8 implementations
python scripts/generate_crc8.py

# Output: dist/crc/c99/ (multiple variants)
# Default: nibble table (16-byte, good balance)
```

**Variants:**

- `full` — 256-byte table (fastest)
- `nibble` — 16-byte table (default, balanced)
- `bitwise` — No table (slowest, minimal memory)

**Staging to source:**

```bash
# Script stages selected variant to src/crc/
python scripts/generate_crc8.py  # --no-stage to skip
```

### Release Process

1. **Update version numbers:**
   - `library.json` — PlatformIO registry
   - `library.properties` — Arduino registry
   - `src/crumbs_version.h` — C macros

2. **Update CHANGELOG.md:**
   - List all changes since last release
   - Categorize: Added, Changed, Fixed, Removed
   - Note breaking changes prominently

3. **Tag release:**

   ```bash
   git tag -a vX.Y.Z -m "Release vX.Y.Z"
   git push origin vX.Y.Z
   ```

4. **Publish to registries:**
   - GitHub releases (automatic from tag)
   - PlatformIO registry (via library.json)
   - Arduino Library Manager (via library.properties)

### CI/CD Pipeline

**Automated checks:**

- CMake build (Linux, macOS, Windows)
- Unit test execution
- Documentation generation
- Arduino library validation

**Future additions:**

- Integration tests with hardware simulator
- Code coverage reporting
- Performance benchmarks

---

## Where to Start

### First-Time Contributors

**Good first issues:**

- Documentation improvements
- Example enhancements
- Test additions
- Bug fixes (labeled `good first issue`)

**Areas to explore:**

- `src/core/crumbs_core.c` — Core protocol logic
- `src/crumbs.h` — Public API
- `src/hal/arduino/` — Arduino integration
- `src/hal/linux/` — Linux integration
- `examples/` — Usage patterns

### Reading the Code

**Start here:**

1. `src/crumbs.h` — Public API overview
2. `src/core/crumbs_core.c` — Message handling
3. `src/hal/arduino/crumbs_i2c_arduino.cpp` — Arduino HAL
4. `examples/core_usage/arduino/basic_peripheral/` — Simple example

**Follow the data flow:**

1. Controller builds message (`crumbs_msg_init`, `crumbs_msg_add_*`)
2. Encode to wire format (`crumbs_encode_message`)
3. Send via HAL (`crumbs_controller_send` → write_fn)
4. Peripheral receives (`crumbs_peripheral_handle_receive`)
5. Decode and validate (`crumbs_decode_message`)
6. Dispatch to handlers or callbacks

---

## Questions?

- **Issues:** Open an issue on GitHub
- **Discussions:** Use GitHub Discussions for general questions
- **Security:** Email maintainers directly (see SECURITY.md if present)

We appreciate your contributions!
