#!/usr/bin/env python3
import sys
hexes = [ int(arg, 2) for arg in sys.argv[1:] ]
print(" ".join(("%02X" % h for h in hexes)))
