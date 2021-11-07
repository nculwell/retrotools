#!/usr/bin/python3

import itertools

ACS_C64_COLOR_VALUES = [
  0x000000, # black
  0xFFFFFF, # white
  0xBB6A51, # orange/brown
  0xA9F3FF, # cyan
  0xCD6FD4, # pink
  0x89E581, # green
  0x6953F5, # purple
  0xFFFF7B, # yellow
]

C64_STANDARD_PALETTE = [
  0x000000, # 0 Black
  0xFFFFFF, # 1 White
  0x880000, # 2 Red
  0xAAFFEE, # 3 Cyan
  0xCC44CC, # 4 Violet / purple
  0x00CC55, # 5 Green
  0x0000AA, # 6 Blue
  0xEEEE77, # 7 Yellow
  0xDD8855, # 8 Orange
  0x664400, # 9 Brown
  0xFF7777, # A Light red
  0x333333, # B Dark grey / grey 2
  0x777777, # C Grey 2
  0xAAFF66, # D Light green
  0x0088FF, # E Light blue
  0xBBBBBB, # F Light grey / grey 3
]

def splitRgb(c):
    assert c == c & 0xFFFFFF
    r = (c >> 16) & 0xFF
    g = (c >>  8) & 0xFF
    b = (c >>  0) & 0xFF
    return (r, g, b)

def joinRgb(r, g, b):
    assert r == r & 0xFF
    assert g == g & 0xFF
    assert b == b & 0xFF
    return (r << 16) | (g << 8) | b

def blend(*colors):
    cs = [ c if type(c) == tuple or type(c) == list else (c, 1) for c in colors ]
    #print(cs)
    rgb = [ (splitRgb(c), n) for (c, n) in cs ]
    parts = sum([ n for (c, n) in cs ])
    r, g, b = (
        sum([ r * n for ((r,g,b), n) in rgb ]),
        sum([ g * n for ((r,g,b), n) in rgb ]),
        sum([ b * n for ((r,g,b), n) in rgb ]),
    )
    return tuple([ int(x) for x in (r/parts, g/parts, b/parts) ])

def fit(rgb):
    best = None
    bestFitness = 1e6
    #choices = [ list(range(16)) ] * 4
    for (a,b,c,d) in itertools.combinations_with_replacement(range(16), 4):
        blended = blend(*( C64_STANDARD_PALETTE[x] for x in (a,b,c,d) ))
        bj = joinRgb(*blended)
        fitness = abs(bj - rgb)
        #print("%06X"%rgb, "%06X"%bj, fitness)
        if fitness < bestFitness:
            best = (a,b,c,d)
            bestFitness = fitness
    return best, bestFitness

def main():
    csp = C64_STANDARD_PALETTE
    #print(blend(csp[0], (csp[1], 2)))
    #print(splitRgb(ACS_C64_COLOR_VALUES[2]))
    #print(blend( (csp[8]), (csp[9]) ))
    for c in ACS_C64_COLOR_VALUES:
        print("%06X" % c, *fit(c))

if __name__ == "__main__":
    main()

