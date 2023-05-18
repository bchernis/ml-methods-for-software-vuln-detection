#! /usr/bin/python

#Compute the number of "->"

import collections, os, sys

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("arrowCount\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                arrowCount = 0
                state = 0
                data=fin.read()
                for c in data:
                    if state == 0 and c == '-':
                        state = 1
                        continue
                    if state == 1:
                        if c == '>':
                            arrowCount += 1
                        state = 0
                fout.write(str(arrowCount) + "\n")

main()
print sys.argv
