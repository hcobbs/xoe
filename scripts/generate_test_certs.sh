#!/bin/bash
#
# Generate self-signed certificates for testing xoe TLS server
#
# This script creates a self-signed certificate and private key for
# development and testing purposes. For production use, obtain
# certificates from a trusted Certificate Authority (e.g., Let's Encrypt).
#

set -e  # Exit on error

CERT_DIR="./certs"
DAYS=365

echo "Generating self-signed TLS certificates for xoe server..."

# Create certificate directory if it doesn't exist
mkdir -p "$CERT_DIR"

# Generate 2048-bit RSA private key
echo "Generating private key..."
openssl genrsa -out "$CERT_DIR/server.key" 2048

# Generate self-signed certificate
echo "Generating self-signed certificate..."
openssl req -new -x509 -key "$CERT_DIR/server.key" \
    -out "$CERT_DIR/server.crt" -days $DAYS \
    -subj "/C=US/ST=State/L=City/O=XOE/CN=localhost"

# Set appropriate permissions
chmod 600 "$CERT_DIR/server.key"
chmod 644 "$CERT_DIR/server.crt"

echo ""
echo "Certificates generated successfully in $CERT_DIR/"
echo "  server.key - Private key (keep this secure!)"
echo "  server.crt - Self-signed certificate"
echo ""
echo "Valid for $DAYS days."
echo ""
echo "To view certificate details:"
echo "  openssl x509 -in $CERT_DIR/server.crt -text -noout"
echo ""
echo "These are self-signed certificates for TESTING ONLY."
echo "For production, use certificates from a trusted CA."
