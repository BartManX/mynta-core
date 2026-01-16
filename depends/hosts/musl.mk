# musl libc host configuration for fully static Linux builds
#
# Usage:
#   make HOST=x86_64-linux-musl -j$(nproc)
#
# Requires:
#   musl-cross-make toolchain installed at /opt/musl-toolchain
#   or toolchain binaries in PATH with x86_64-linux-musl- prefix
#
# This configuration produces fully static binaries that:
#   - Do not depend on glibc
#   - Do not require any shared libraries at runtime
#   - Are portable across all Linux distributions
#

# Inherit from Linux configuration
musl_CFLAGS=-pipe -O2
musl_CXXFLAGS=$(musl_CFLAGS)

musl_release_CFLAGS=-O2
musl_release_CXXFLAGS=$(musl_release_CFLAGS)

musl_debug_CFLAGS=-O1 -g
musl_debug_CXXFLAGS=$(musl_debug_CFLAGS)

# Static linking flags - critical for fully static builds
musl_LDFLAGS=-static -static-libgcc -static-libstdc++

# Toolchain detection
# Try to find toolchain in standard locations
MUSL_TOOLCHAIN_PATH ?= /opt/musl-toolchain

# Check if toolchain is in PATH or at standard location
ifneq ($(shell which x86_64-linux-musl-gcc 2>/dev/null),)
# Toolchain is in PATH
x86_64_musl_CC=x86_64-linux-musl-gcc
x86_64_musl_CXX=x86_64-linux-musl-g++
x86_64_musl_AR=x86_64-linux-musl-ar
x86_64_musl_RANLIB=x86_64-linux-musl-ranlib
x86_64_musl_NM=x86_64-linux-musl-nm
x86_64_musl_STRIP=x86_64-linux-musl-strip
else ifneq ($(wildcard $(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-gcc),)
# Toolchain at standard location
x86_64_musl_CC=$(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-gcc
x86_64_musl_CXX=$(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-g++
x86_64_musl_AR=$(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-ar
x86_64_musl_RANLIB=$(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-ranlib
x86_64_musl_NM=$(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-nm
x86_64_musl_STRIP=$(MUSL_TOOLCHAIN_PATH)/bin/x86_64-linux-musl-strip
else
# Fallback - assume generic cross prefix (requires toolchain in PATH)
$(warning musl toolchain not found. Install via Build/scripts/install-musl-toolchain.sh)
x86_64_musl_CC=x86_64-linux-musl-gcc
x86_64_musl_CXX=x86_64-linux-musl-g++
x86_64_musl_AR=x86_64-linux-musl-ar
x86_64_musl_RANLIB=x86_64-linux-musl-ranlib
x86_64_musl_NM=x86_64-linux-musl-nm
x86_64_musl_STRIP=x86_64-linux-musl-strip
endif

# AArch64 (ARM64) musl support
aarch64_musl_CC=aarch64-linux-musl-gcc
aarch64_musl_CXX=aarch64-linux-musl-g++
aarch64_musl_AR=aarch64-linux-musl-ar
aarch64_musl_RANLIB=aarch64-linux-musl-ranlib
aarch64_musl_NM=aarch64-linux-musl-nm
aarch64_musl_STRIP=aarch64-linux-musl-strip

# CMake system name for packages that use cmake
musl_cmake_system=Linux
