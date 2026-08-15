# Run `just` (or `just --list`) to see all available recipes.

default:
    @just --list

# One-time setup: pulls in raylib and doctest (git submodules).
submodules:
    git submodule update --init --recursive

# Configure and build the sol app.
build:
    cmake -B build
    cmake --build build

# Build and run the sol app.
run: build
    ./build/sol

# Build and run the test suite via ctest.
test:
    cmake -B build
    cmake --build build --target sol_tests
    ctest --test-dir build --output-on-failure

# Run a single test case by name, e.g. just test-case "Button: a full click fires onClick and onActivate"
test-case name:
    cmake -B build
    cmake --build build --target sol_tests
    ./build/sol_tests --test-case="{{ name }}"

# Build and run the test suite under AddressSanitizer + LeakSanitizer (separate build dir).
sanitize:
    cmake -B build-sanitize -DSOL_SANITIZE=ON
    cmake --build build-sanitize --target sol_tests
    ./build-sanitize/sol_tests

# Remove all build output (both build/ and build-sanitize/).
clean:
    rm -rf build build-sanitize
