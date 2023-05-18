#! /usr/bin/python

#Compute the number of "for" substrings.

import collections, os, string, re, sys

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("forCount\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                forCount = 0
                for line in fin:
                    if re.match(".*for.*\(.*\;.*\;.*\).*", line):
                        forCount += 1
                fout.write(str(forCount)+"\n")

main()
print sys.argv
