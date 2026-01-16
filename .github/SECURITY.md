# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.0.x   | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it by:

1. **DO NOT** open a public issue
2. Email the maintainers directly or use GitHub's private vulnerability reporting feature
3. Include a detailed description of the vulnerability and steps to reproduce

We aim to respond within 48 hours and will work with you to understand and address the issue.

## Security Measures

### Binary Hardening

The CISV library is built with the following security flags:

- `-fstack-protector-strong`: Stack buffer overflow protection
- `-D_FORTIFY_SOURCE=2`: Buffer overflow detection in string/memory functions
- `-fstack-check`: Runtime stack overflow checking
- `-Wl,-z,relro,-z,now`: Full RELRO (read-only relocations)
- `-fPIE -pie`: Position Independent Executable (CLI only)

### Input Validation

- CLI arguments are validated with bounds checking (no `atoi()` usage)
- Python bindings validate delimiter, quote, and escape characters
- File path validation prevents device files, symlinks to sensitive files
- Memory limits prevent denial of service attacks

### CI/CD Security

- GitHub Actions should be pinned to full SHA hashes for supply chain security
- Secrets are passed via environment variables, not command line arguments
- API credentials use header-based authentication where possible

## GitHub Actions SHA Pinning

For production deployments, pin GitHub Actions to full SHA commit hashes:

```yaml
# Instead of:
- uses: actions/checkout@v4

# Use:
- uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683 # v4.2.2
```

### Finding SHA Hashes

To find the SHA for an action version:

```bash
# Get SHA for a specific tag
git ls-remote --tags https://github.com/actions/checkout.git | grep 'v4.2.2'

# Or visit the releases page and click on the commit hash
```

### Current Recommended SHAs (verify before use)

| Action | Version | SHA (verify at source) |
|--------|---------|------------------------|
| actions/checkout | v4.2.2 | `11bd71901bbe5b1630ceea73d27597364c9af683` |
| actions/cache | v5.0.1 | `9255dc7a253b0ccc959486e2bca901246202afeb` |
| actions/setup-node | v6 | Check releases page |
| actions/upload-artifact | v4 | Check releases page |

**Note**: Always verify SHA hashes at the official repository before using them.

## Cryptographic Functions

**WARNING**: The `cisv_transform_hash_sha256` function in `transformer.c` is a **MOCK implementation** that does NOT provide real cryptographic hashing. It is intended for API demonstration only.

For real cryptographic operations, integrate:
- OpenSSL (libcrypto)
- libsodium
- mbedTLS

## Known Limitations

1. **Not constant-time**: Parsing operations are optimized for performance, not constant-time execution. Do not use for timing-sensitive security applications.

2. **Memory-mapped files**: Large files use mmap() which may expose data in core dumps or via /proc filesystem.

3. **Thread safety**: Parser instances are NOT thread-safe. Each thread should use its own parser instance.
