#!/usr/bin/env python3
"""
Compress an input file with gzip and convert it to a C header with a byte array.

Usage:
   python gzip_to_header.py inputfile
   python gzip_to_header.py inputfile --symbol help_html_gz
   python gzip_to_header.py inputfile --out-gz help.html.gz --out-h help_html_gz.h
"""

import argparse
import gzip
import re
from pathlib import Path


def derive_symbol_name(path: Path) -> str:
   """Derive a reasonable C symbol name from a file path."""
   base = path.stem
   # Replace anything non-alphanumeric with underscore
   base = re.sub(r'[^0-9A-Za-z]', '_', base)
   if not base:
       base = "blob"
   # Ensure it does not start with a digit
   if base[0].isdigit():
       base = "_" + base
   return base + "_gz"


def derive_include_guard(symbol: str) -> str:
   """Derive an include guard macro name from the symbol name."""
   guard = re.sub(r'[^0-9A-Za-z]', '_', symbol).upper()
   return guard + "_H"


def chunk_bytes(data: bytes, size: int):
   """Yield slices of 'data' with length 'size' (last chunk may be shorter)."""
   for i in range(0, len(data), size):
       yield data[i:i + size]


def compress_to_gzip(src: Path, dst: Path, level: int = 9) -> bytes:
   """Compress the contents of src to dst using gzip and return the compressed bytes."""
   raw = src.read_bytes()
   compressed = gzip.compress(raw, compresslevel=level)
   dst.write_bytes(compressed)
   return compressed


def write_header(dst: Path, symbol: str, data: bytes) -> None:
   """Write a C header file with the given symbol and byte data."""
   guard = derive_include_guard(symbol)
   with dst.open("w", encoding="utf-8", newline="\n") as f:
       f.write(f"/* Compressed binary generated from {dst.name.replace('.h', '')} */\n")
       f.write(f"#ifndef {guard}\n")
       f.write(f"#define {guard}\n\n")
       f.write(f"static const unsigned char {symbol}[] = {{\n")
       for line in chunk_bytes(data, 16):
           hex_bytes = ", ".join(f"0x{b:02x}" for b in line)
           f.write(f"    {hex_bytes},\n")
       f.write("};\n\n")
       f.write(f"static const unsigned int {symbol}_len = {len(data)}u;\n\n")
       f.write(f"#endif /* {guard} */\n")


def parse_args() -> argparse.Namespace:
   parser = argparse.ArgumentParser(
       description="Compress a file with gzip and emit a C header with the compressed bytes."
   )
   parser.add_argument(
       "input",
       help="Input file to compress and convert."
   )
   parser.add_argument(
       "--symbol",
       help="C symbol name for the byte array (default: derived from input filename)."
   )
   parser.add_argument(
       "--out-gz",
       help="Output gzip file path (default: <input>.gz)."
   )
   parser.add_argument(
       "--out-h",
       help="Output header file path (default: <input_basename>_gz.h)."
   )
   parser.add_argument(
       "--level",
       type=int,
       default=9,
       help="Gzip compression level (1-9, default: 9)."
   )
   return parser.parse_args()


def main() -> None:
   args = parse_args()
   src = Path(args.input).resolve()

   if not src.is_file():
       raise SystemExit(f"Input file does not exist or is not a file: {src}")

   symbol = args.symbol or derive_symbol_name(src)

   if args.out_gz:
       out_gz = Path(args.out_gz).resolve()
   else:
       out_gz = src.with_suffix(src.suffix + ".gz")

   if args.out_h:
       out_h = Path(args.out_h).resolve()
   else:
       out_h = src.with_name(derive_symbol_name(src) + ".h")

   compressed = compress_to_gzip(src, out_gz, level=args.level)
   write_header(out_h, symbol, compressed)

   print(f"Input:   {src}")
   print(f"Gzip:    {out_gz} ({len(compressed)} bytes)")
   print(f"Header:  {out_h}")
   print(f"Symbol:  {symbol}")
   print("Done.")


if __name__ == "__main__":
   main()