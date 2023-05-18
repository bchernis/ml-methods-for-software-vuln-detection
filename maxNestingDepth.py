#! /usr/bin/python

# Compute maximum nesting depth of curly braces.

import collections, os, sys

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("maxNestingDepth\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                depth = maxDepth = 0
                data=fin.read()
                for c in data:
                    if c == '{':
                        depth += 1
                    if c == '}':
                        depth -= 1
                    if depth > maxDepth:
                        maxDepth = depth
                fout.write(str(maxDepth)+"\n")

main()
print sys.argv
