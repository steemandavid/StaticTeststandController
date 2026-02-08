#!/usr/bin/env python3
"""
Increment the build number in version.h before each build.
"""

import os
import re
import sys

VERSION_FILE = os.path.join(os.path.dirname(__file__), '..', 'main', 'version.h')

def increment_build():
    """Read version.h, increment BUILD_NUMBER, and write back."""
    if not os.path.exists(VERSION_FILE):
        print(f"Error: {VERSION_FILE} not found", file=sys.stderr)
        return 1

    with open(VERSION_FILE, 'r') as f:
        content = f.read()

    # Find and increment BUILD_NUMBER
    match = re.search(r'#define BUILD_NUMBER\s+(\d+)', content)
    if not match:
        print("Error: BUILD_NUMBER not found in version.h", file=sys.stderr)
        return 1

    old_build = int(match.group(1))
    new_build = old_build + 1

    # Replace the build number
    new_content = re.sub(
        r'#define BUILD_NUMBER\s+\d+',
        f'#define BUILD_NUMBER    {new_build}',
        content
    )

    with open(VERSION_FILE, 'w') as f:
        f.write(new_content)

    # Also extract major.minor for display
    major_match = re.search(r'#define VERSION_MAJOR\s+(\d+)', content)
    minor_match = re.search(r'#define VERSION_MINOR\s+(\d+)', content)
    major = major_match.group(1) if major_match else '?'
    minor = minor_match.group(1) if minor_match else '?'

    print(f"Version: v{major}.{minor}.{new_build}")
    return 0

if __name__ == '__main__':
    sys.exit(increment_build())
