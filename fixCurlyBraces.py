#! /usr/bin/python

import re, sys

with open(sys.argv[1], 'r') as fin:
    data=fin.read()

print re.sub('\s*{','{',data)
