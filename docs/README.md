# XOE Documentation

XOE (X over Ethernet) project documentation. If you're reading this, you probably need to tunnel something ancient through something modern. Good luck.

## Quick Start

- [Project Overview](../README.md) - Getting started, features, and quick start guide
- [Deployment Guide](guides/DEPLOYMENT.md) - Installation, configuration, and production deployment
- [Security Guide](guides/SECURITY.md) - TLS configuration and security best practices

## For Developers

- [Testing Guide](development/TESTING.md) - Test framework, writing tests, running tests
- [Code Review](development/CODE_REVIEW.md) - Architecture analysis and code quality
- [Contributing](development/CONTRIBUTING.md) - How to contribute to XOE

## Design Documents

- [Serial Connector](design/SERIAL_CONNECTOR.md) - Serial bridge architecture and design

## Archive

Historical and completed documentation:

- [Implementation Status](archive/IMPLEMENTATION_STATUS.md) - Historical implementation tracking
- [Directory Reorganization](archive/DIRECTORY_REORGANIZATION.md) - Completed directory structure reorg

## Documentation Structure

```
docs/
├── README.md                      # This file - documentation index
├── guides/                        # User and operator guides
│   ├── DEPLOYMENT.md
│   └── SECURITY.md
├── development/                   # Developer documentation
│   ├── TESTING.md
│   ├── CODE_REVIEW.md
│   └── CONTRIBUTING.md
├── design/                        # Design documents
│   └── SERIAL_CONNECTOR.md
└── archive/                       # Historical documents
    ├── IMPLEMENTATION_STATUS.md
    └── DIRECTORY_REORGANIZATION.md
```

## Contributing to Documentation

1. Follow the existing structure
2. Be concise. Nobody reads walls of text.
3. Include code examples
4. Update this index when adding new documents
5. Follow the git workflow in [CONTRIBUTING.md](development/CONTRIBUTING.md)

## Issues

Open a GitHub issue. Read [CONTRIBUTING.md](development/CONTRIBUTING.md) first.
