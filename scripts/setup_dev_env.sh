#!/bin/bash
# Developer environment setup script
#
# This script sets up a complete development environment for XOE:
# - Installs git hooks
# - Generates test certificates
# - Builds the project
# - Runs test suite
#
# Usage:
#   ./scripts/setup_dev_env.sh

set -e  # Exit on error

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " XOE Development Environment Setup"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Step 1: Install git hooks
echo "[1/5] Installing git hooks..."
if [ -f scripts/hooks/pre-push ]; then
    cp scripts/hooks/pre-push .git/hooks/
    chmod +x .git/hooks/pre-push
    echo "  ✓ Pre-push hook installed"
else
    echo "  ⚠ Warning: scripts/hooks/pre-push not found"
fi
echo ""

# Step 2: Generate test certificates
echo "[2/5] Generating test certificates..."
if [ ! -f certs/server.crt ]; then
    if [ -f scripts/generate_test_certs.sh ]; then
        ./scripts/generate_test_certs.sh
        echo "  ✓ Test certificates generated"
    else
        echo "  ⚠ Warning: scripts/generate_test_certs.sh not found"
        echo "     TLS tests will fail without certificates"
    fi
else
    echo "  ✓ Test certificates already exist"
fi
echo ""

# Step 3: Clean build
echo "[3/5] Cleaning previous build artifacts..."
make clean > /dev/null 2>&1
echo "  ✓ Build artifacts cleaned"
echo ""

# Step 4: Build project
echo "[4/5] Building project..."
if make; then
    echo "  ✓ Build successful"
else
    echo "  ✗ Build failed"
    exit 1
fi
echo ""

# Step 5: Run tests
echo "[5/5] Running test suite..."
if make test-verbose; then
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo " Setup Complete! ✓"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "You're ready to develop! Quick commands:"
    echo ""
    echo "  make           # Build the project"
    echo "  make test      # Run all tests"
    echo "  make check     # Comprehensive validation"
    echo "  make clean     # Remove build artifacts"
    echo ""
    echo "See docs/TESTING.md for detailed testing documentation."
    echo ""
else
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo " Setup Complete with Warnings ⚠"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "Build succeeded but some tests failed."
    echo "Review the test output above and fix failures before committing."
    echo ""
    exit 1
fi
