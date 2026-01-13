# Mynta Core Documentation

## Directory Structure

| Directory | Contents |
|-----------|----------|
| [`api/`](api/) | RPC, REST, ZMQ interfaces |
| [`building/`](building/) | Build instructions for all platforms |
| [`consensus/`](consensus/) | Consensus rules, BIPs, activation |
| [`features/`](features/) | Feature documentation (atomic swaps, etc.) |
| [`internal/`](internal/) | Internal development docs |
| [`masternode/`](masternode/) | Masternode setup and operation |
| [`network/`](network/) | Network configuration (Tor, DNS seeds) |
| [`operations/`](operations/) | Node operation and maintenance |
| [`release/`](release/) | Release notes and process |

## Quick Links

### Getting Started
- [Building from Source](building/build-unix.md)
- [Configuration](api/raven-conf.md)
- [Running a Node](operations/init.md)

### Masternodes
- [DIP3 Specification](masternode/MIP-001-DETERMINISTIC-MASTERNODES.md)
- [Operator Guide](masternode/OPERATOR_GUIDE.md)

### Development
- [Developer Notes](developer-notes.md)
- [JSON-RPC Interface](api/JSON-RPC-interface.md)
- [REST Interface](api/REST-interface.md)

### Consensus
- [Advanced Consensus Implementation](consensus/ADVANCED_CONSENSUS_IMPLEMENTATION.md)
- [Activation Safety Analysis](consensus/ACTIVATION_SAFETY_ANALYSIS.md)
- [BIP Support](consensus/bips.md)

### Security
- [Security Policy](SECURITY.md)
- [Payment Validation Audit](consensus/PAYMENT_VALIDATION_AUDIT.md)

## Platform-Specific Build Docs

| Platform | Document |
|----------|----------|
| Linux | [`building/build-unix.md`](building/build-unix.md) |
| Ubuntu | [`building/build-ubuntu.md`](building/build-ubuntu.md) |
| macOS | [`building/build-osx.md`](building/build-osx.md) |
| Windows | [`building/build-windows.md`](building/build-windows.md) |
| FreeBSD | [`building/build-freebsd.md`](building/build-freebsd.md) |
| OpenBSD | [`building/build-openbsd.md`](building/build-openbsd.md) |
| Raspberry Pi | [`building/build-rasberrypi.md`](building/build-rasberrypi.md) |
