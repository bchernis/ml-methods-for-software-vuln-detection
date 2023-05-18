#! /usr/bin/python

#Compute the number of DISTINCT characters in the function

import collections, os, sys, re

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+sys.argv[1]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("charDiversity\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                distinctChars = []
                data=fin.read()
                if sys.argv[1] == "1":
                    data = re.sub("[0-9a-zA-Z]","",data)
                if sys.argv[1] == "2":
                    data = re.sub("[2-9]","",data)
                    data = data.replace("2","if").replace("3","int").replace("4","case").replace("5","return").replace("6","break").replace("7","char").replace("8","else").replace("9","for")
                    data = re.sub("[a-zA-Z]","",data)
                for c in data:
                    if c not in distinctChars:
                        distinctChars.append(c)
                fout.write(str(len(distinctChars))+"\n")

main()
print sys.argv
