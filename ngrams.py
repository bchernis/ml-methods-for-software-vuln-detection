#! /usr/bin/python

#Examples of usage:
#ngrams.py c 5 10000 #Compute top 10000 most frequent character 5-grams
#ngrams.py w 2 200 #Compute top 200 most frequent word 2-grams

import collections, os, sys

def getNGrams(input_list,n):
    return zip(*[input_list[i:] for i in range(n)])

def main():
    print sys.argv
    option = sys.argv[1]
    n = int(sys.argv[2])
    k = int(sys.argv[3])
    outputFile = "_".join(sys.argv).replace(".py","")+".txt"
    if option != "w" and option != "c":
        print "Please enter a valid first argument."
        exit()
       
	#Combine text from all files, and calculate n-gram stats. 
    listAll = []
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open("stats/"+outputFile,"w")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                if option == "c":
                    while True:
                        c = fin.read(1)
                        if c and ord(c) != 13: #Prevent weird character
                            listAll.append(c)
                        else:
                            break
                elif option == "w":
                    for line in fin:
                        for word in line.split():
                            listAll.append(word)
    ngramsAll = collections.Counter(getNGrams(listAll,n))
	
    for key in ngramsAll:
        ngramsAll[key] = float(ngramsAll[key])/len(listAll)
    ngramsTopK = dict(sorted(ngramsAll.iteritems(), reverse=True, key=lambda (k,v): (v,k))[:k]) #Identify top k most frequent n-grams.
    for root, dirs, filenames in os.walk(indir): #Extract n-gram stats for each individual file
        filenames.sort()
        headerRow = ""
        for key in ngramsTopK:
			#N-grams statistics will be written to a table. The rows will represent files, and the columns will represent the n-grams. The first row (stored in headerRow variable) will show which n-grams statistics are being calculated for
            #Example: Write "3C" for a character 3-gram
            headerRow += str(n)
            if option == "w":
                headerRow += "W"
            elif option == "c":
                headerRow += "C"
            for i in range(0,n): #For each item in n-gram tuple
                #headerRow += "\\\\"+headerRow.replace("\n","<nl>")#Double backslash, not quadruple. And replace newline with <nl> for implementation purposes
                headerRow += "//"+key[i].replace("\n","<nl>").replace("\t","<tab>").replace(" ","<space>") #Remove whitespace for implementation purposes
            headerRow += "// "
        fout.write(headerRow+"\n")
        for fileName in filenames:
            listCurrent = []
            with open(indir+'/'+fileName,'r') as fin:
                if option == "c":
                    while True:
                        c = fin.read(1)
                        if c:
                            listCurrent.append(c)
                        else:
                            break
                elif option == "w":
                    for line in fin:
                        for word in line.split():
                            listCurrent.append(word)
            ngramsCurrent = collections.Counter(getNGrams(listCurrent,n)) #Extract n-grams from current file
            for key in ngramsTopK:
                if key in ngramsCurrent:
                    fout.write(str(float(ngramsCurrent[key])/len(listCurrent))+" ")
                    #fout.write(str(float(ngramsCurrent[key]))+" ")
                else:
                    fout.write("0 ")
            fout.write("\n")

main()
