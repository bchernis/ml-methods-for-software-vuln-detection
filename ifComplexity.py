#! /usr/bin/python

#Compute average if statement complexity (# "else" + # "else if") / # "if"
import collections, os, string, re, sys

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("ifComplexity\n")        
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                ifCount = elseifCount = elseCount = 0
                for line in fin:
                    if re.match("\s*else if.*\(.+\).*", line):
                        elseifCount += 1
                    elif re.match("\s*else.*\(.+\).*", line):
                        elseCount += 1
                    elif re.match("\s*if.*\(.+\).*", line):
                        ifCount += 1                        
                #fout.write(str(elseifCount)+"\n")
                if ifCount == 0:
                    fout.write("0\n")
                else:
                    fout.write(str((float(elseifCount)+float(elseCount))/float(ifCount))+"\n")

main()
print sys.argv
