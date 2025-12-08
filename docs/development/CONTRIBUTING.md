# Contributing to XOE

## Project Philosophy

XOE is a testbed for human-LLM hybrid development. All contributions must be labeled to track development approach. No exceptions. If you can't be bothered to add a tag, you can't be bothered to contribute.

## Development Model

### Contribution Labels

All commits must include one of these labels:

- `[CLASSIC]` - Traditional hand-coded implementation without AI assistance
- `[CLASSIC-REVIEW]` - Traditional human-run code review
- `[LLM-ASSISTED]` - Code written with LLM assistance (pair programming style)
- `[LLM-ARCH]` - Software architect leveraging LLM for code generation while reviewing, adjusting, and confirming all plans
- `[LLM-REVIEW]` - LLM-powered code review and resulting fixes
- `[VIBE]` - Experimental or exploratory coding

### Example Commit Message

```
Add serial port error detection

Implement comprehensive error detection for serial ports including
parity errors, framing errors, and buffer overruns.

[LLM-ARCH]
```

## Branch Workflow

### Branch Naming Conventions

- `feature/<description>` - New features or enhancements
- `bugfix/<description>` - Bug fixes
- `refactor/<description>` - Code refactoring
- `test/<description>` - Test additions or improvements
- `docs/<description>` - Documentation changes

### Development Process

1. **Create a branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** with appropriate commit labels

3. **Push your branch**:
   ```bash
   git push -u origin feature/your-feature-name
   ```

4. **Create a pull request** to merge into `main`

5. **Complete the PR checklist** (see below)

**Note**: Repository rules require all changes (including documentation) to go through pull requests. Direct commits to `main` are not allowed.

## Pre-PR Checklist

Before submitting a pull request, ensure:

- [ ] Code builds successfully: `make`
- [ ] All tests pass: `make test`
- [ ] Code follows ANSI-C (C89) standards with `-std=c89 -pedantic`
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] All functions have complete documentation (purpose, parameters, return values, errors)
- [ ] Code has complete test coverage (see [TESTING.md](TESTING.md))
- [ ] Helper functions are reusable and well-documented
- [ ] Commits include contribution label ([CLASSIC], [LLM-ASSISTED], etc.)
- [ ] Changes align with Clean Code principles (see below)

## Coding Standards

### Language Standard

- **ANSI-C (C89)** with strict compliance: `-std=c89 -pedantic`
- Cross-platform support for Linux, macOS, and BSD (Unix/POSIX only, Windows not supported)
- No C99/C11 features

### Code Formatting

**Indentation**:
- Use **4 spaces** for indentation (no tabs)
- Consistent indentation for all code blocks

**Line Length**:
- Preferred: 80-100 characters (soft limit)
- Maximum: 120 characters (hard limit)
- Break long lines at logical points (after commas, operators, etc.)

**General Style**:
- Opening braces on same line for functions and control structures
- Consistent spacing around operators and after keywords
- Clear, readable code structure preferred over compact formatting

### Clean Code Principles

This project emphasizes:

- **Spaces for Alignment**: Tabs are banned for alignment purposes. Anything other than spaces for alignment will fail code review.
- **Maximum Code Width**: 120 characters maximum line length. Use appropriate line breaks to align variables cleanly and clearly.
- **Reusable Helper Functions**: Prefer creating helper functions that can be reused multiple times rather than duplicating code
- **Complete Documentation**: All code must be thoroughly documented with clear comments explaining purpose, parameters, return values, and any side effects
- **Full Test Coverage**: All code should have complete test coverage to ensure reliability and maintainability

### Documentation Requirements

- Function headers must document:
  - Purpose and behavior
  - Parameters (name, type, constraints)
  - Return values (success/error codes)
  - Error conditions and handling
- Complex algorithms or business logic should include inline comments
- Non-obvious code sections should be explained

### Testing Requirements

- All functions should have corresponding unit tests
- Edge cases and error conditions must be tested
- Test coverage should be comprehensive and maintainable
- See [TESTING.md](TESTING.md) for complete testing guide

## Running Tests

Before submitting a PR, ensure all tests pass:

```bash
make check    # Comprehensive validation (recommended)
```

Individual test targets:

```bash
make test-build        # Compile tests only
make test-unit         # Run unit tests
make test-integration  # Run integration tests
make test              # Run all tests
make test-verbose      # Detailed output
```

See [TESTING.md](TESTING.md) for complete testing documentation.

## Code Review Process

All pull requests undergo code review before merging:

1. **Automated Checks**: Build and test suite must pass
2. **Code Review**: Maintainer reviews for:
   - Code quality and clarity
   - Adherence to coding standards
   - Proper documentation
   - Test coverage
   - Architecture alignment
3. **Feedback**: Address review comments and update PR
4. **Approval**: Once approved, PR will be merged

See [CODE_REVIEW.md](CODE_REVIEW.md) for detailed architecture analysis.

## Questions

Open an issue. Read the docs first. They exist for a reason.

## License

Contributions are licensed under GPLv3. If that's a problem, this isn't the project for you.
