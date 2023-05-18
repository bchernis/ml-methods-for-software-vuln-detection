#! /usr/bin/python

#Compute the string entropy of the function.

import collections, os, math, string, sys, re

def range_bytes (): return range(256)
def range_printable(): return (ord(c) for c in string.printable)

def H(data, iterator=range_bytes): #Entropy calculation
    if not data:
        return 0
    entropy = 0
    for x in iterator():
        p_x = float(data.count(chr(x)))/len(data)
        if p_x > 0:
            entropy += - p_x*math.log(p_x, 2)
    return entropy

def main():
    outputFile = 'stats/'+sys.argv[0].split('.')[1][1:]+sys.argv[1]+'.txt'
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open(outputFile,"w")
    fout.write("entropy\n")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                data = fin.read()
                if sys.argv[1] == "1": #If you used "1" as an argument, all alphanumeric characters wil be removed
                    data = re.sub("[0-9a-zA-Z]","",data)
                if sys.argv[1] == "2": #If you used "2" as an argument this script will first replace digits 2-9 with the top 8 c keywords and then remove all alphabetic (not alphanumeric) characters.
                    data = re.sub("[2-9]","",data)
                    data = data.replace("2","if").replace("3","int").replace("4","case").replace("5","return").replace("6","break").replace("7","char").replace("8","else").replace("9","for")
                    data = re.sub("[a-zA-Z]","",data)
                fout.write(str(H(data,range_printable))+"\n")

main()
print sys.argv
