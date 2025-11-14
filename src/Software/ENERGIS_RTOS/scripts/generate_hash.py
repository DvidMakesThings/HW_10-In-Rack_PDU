#!/usr/bin/env python3
import hashlib
import sys

password = sys.argv[1] if len(sys.argv) > 1 else "admin"
hash_obj = hashlib.sha256(password.encode('utf-8'))
hex_hash = hash_obj.hexdigest()

print(f"Password: {password}")
print(f"SHA-256: {hex_hash}")
print("\nC Array:")
bytes_list = [hex_hash[i:i+2] for i in range(0, len(hex_hash), 2)]
for i in range(0, 32, 8):
    print("    " + ", ".join([f"0x{b}" for b in bytes_list[i:i+8]]) + ("," if i < 24 else ""))