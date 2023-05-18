#! /usr/bin/python

import collections, os, sys, re

#Counts the number of characters in the function

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+sys.argv[1]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        fout.write("numChars\n")
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                data=fin.read()
                if sys.argv[1] == "1":
                    data = re.sub("[0-9a-zA-Z]","",data)
                if sys.argv[1] == "2":
                    data = re.sub("[2-9]","",data)
                    data = data.replace("2","if").replace("3","int").replace("4","case").replace("5","return").replace("6","break").replace("7","char").replace("8","else").replace("9","for")
                    data = re.sub("[a-zA-Z]","",data)
                fout.write(str(len(data))+"\n")
    print sys.argv

main()
