#!/usr/bin/env python

import csv
import sys
from StringIO import StringIO

DATA_STR = """
"raw",2,6,11,10,10,10,11,16,18,21,15,7,7,4,3,3,1,1,
"calibrationData",1.11,0.96,0.88,0.83,0.86,1.15,0.42,0.38,0.33,0.35,0.56,0.88,0.96,0.98,0.93,0.89,0.81,0.69,
"calibrated",4.61,6.16,6.99,2.47,0.56,0.88,4.61,6.16,4.61,6.99,6.16,2.47,6.99,2.47,0.56,0.88,0.56,0.88,
"""

def str2csv(s):
    buff = StringIO(s)
    return csv.reader(buff)

def main():
    pass

if __name__ == '__main__':
    main()
    sys.exit(0)

r = str2csv(DATA_STR.strip())
#l = [i for i in r]
raw = [int(i) for i in r.next()[1:19]]
data = [float(i) for i in r.next()[1:19]]
cal = [float(i) for i in r.next()[1:19]]
