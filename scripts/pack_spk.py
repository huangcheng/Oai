#!/usr/bin/env python3
"""
Pack a sprite pack directory into a .spk file (ZIP archive).
"""

import zipfile
import os
import sys
from pathlib import Path

def pack_sprite_pack(pack_dir: str, output_path: str):
    """
    Pack a sprite pack directory into a .spk file.
    """
    pack_path = Path(pack_dir)
    if not pack_path.exists():
        print(f"Error: Pack directory not found: {pack_dir}")
        return False
    
    manifest_path = pack_path / "manifest.json"
    if not manifest_path.exists():
        print(f"Error: No manifest.json found in: {pack_dir}")
        return False
    
    print(f"Packing: {pack_dir}")
    print(f"Output: {output_path}")
    
    with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zf:
        # Add all files in the pack directory
        for root, dirs, files in os.walk(pack_path):
            for file in files:
                file_path = Path(root) / file
                # Calculate relative path from pack directory
                rel_path = file_path.relative_to(pack_path)
                print(f"  Adding: {rel_path}")
                zf.write(file_path, rel_path)
    
    # Verify
    size = os.path.getsize(output_path)
    print(f"\nDone! Created: {output_path} ({size:,} bytes)")
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: pack_spk.py <pack_directory> [output.spk]")
        print("Example: pack_spk.py assets/packs/clippy clippy.spk")
        sys.exit(1)
    
    pack_dir = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    
    # Default output name from pack directory
    if not output_path:
        pack_name = Path(pack_dir).name
        output_path = f"{pack_name}.spk"
    
    pack_sprite_pack(pack_dir, output_path)

if __name__ == "__main__":
    main()
