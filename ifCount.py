#! /usr/bin/python

#Compute the number of "if" substrings.

import collections, os, string, re, sys

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("ifCount\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                ifCount = 0
                for line in fin:
                    if re.match("\s*if.*\(.+\).*", line):
                        ifCount += 1
                fout.write(str(ifCount)+"\n")

main()
print sys.argv
