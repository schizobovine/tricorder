#!/usr/bin/env python

import sys

def main():

    if len(sys.argv) != 4:
        print "Usage: %s <RED> <GREEN> <BLUE>" % (sys.argv[0],)
        sys.exit(1)

    r = int(sys.argv[1])
    g = int(sys.argv[2])
    b = int(sys.argv[3])
    color24 = [r, g, b]

    for color in color24:
        if color < 0 or color > 256:
            print "value out of range: %d" % (color,)
            sys.exit(2)

    r /= 256.0
    g /= 256.0
    b /= 256.0
    print "(%3.0f%%, %3.0f%%, %3.0f%%)" % (r*100, g*100, b*100)

    color16 = 0
    color16 |= int(r * (1 << 5)) << 11
    color16 |= int(g * (1 << 6)) << 5
    color16 |= int(b * (1 << 5))

    print "dec", color16
    print "dec", color16
    print "bin", bin(color16)
    print "hex", hex(color16)

if __name__ == '__main__':
    main()
    sys.exit(0)
