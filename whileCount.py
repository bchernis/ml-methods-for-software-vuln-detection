#! /usr/bin/python

#Compute the number of "while" substrings.

import collections, os, string, re, sys

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("whileCount\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                whileCount = 0
                for line in fin:
                    if re.match("\s*while.*\(.+\).*", line):
                        whileCount += 1
                fout.write(str(whileCount)+"\n")

main()
print sys.argv
