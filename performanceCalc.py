#! /usr/bin/python

import fileinput, sys
from sklearn.metrics import confusion_matrix
from sklearn.tree import DecisionTreeClassifier
from sklearn.naive_bayes import GaussianNB
import math
import metrics #My file, not a standard library
import numpy as np
import random
from sklearn.cross_validation import KFold

def cmCalc(features): 
    if crossVal:
        kf = KFold(len(y),n_folds=crossVal)
        cm = np.array([[0,0],[0,0]])
        for train,test in kf:
            model.fit(x[train][:,features],y[train])
            y2predicted = model.predict(x[test][:,features])
            cm_current = confusion_matrix(y[test],y2predicted)
            if cm_current.shape[0] == 1:
                cm_current = np.array([[cm_current[0][0]/2,0],[0,cm_current[0][0]/2]])
            cm += cm_current
        return cm
    model.fit(x1[:,features],y1)
    y2predicted = model.predict(x2[:,features])
    return confusion_matrix(y2,y2predicted)

print sys.argv

x1 = []
y1 = []
x2 = []
y2predicted = []
y2 = []
rowNum = 0

#Parameters
nTrain = int(sys.argv[1]) #Total number of samples that are used to train each decision tree.
crossVal = int(sys.argv[2]) #Cross-validation fold

#Input file is allStats.txt. There will be 6 output files, one for each performance metric
fin = open("allStats.txt","r")
fout1 = open("allStats_infoGain.txt","w")
fout2 = open("allStats_precision.txt","w")
fout3 = open("allStats_recall.txt","w")
fout4 = open("allStats_accuracy.txt","w")
fout5 = open("allStats_f1.txt","w")
fout6 = open("allStats_random.txt","w")
files = (fin, fout1, fout2, fout3, fout4, fout5, fout6)
data = []
data_rearranged = []
for row in files[0]: #Read data as 2D list
    data.append([temp for temp in row.split()])
    data_rearranged.append([temp for temp in row.split()])
data = np.array(data) #Convert to array
for i in range(1,len(data)):
    for j in range(1,len(data[i])-1):
            data[i][j] = str(float(data[i][j])+.0000001) #Fixes division by zero issue for NB
data_rearranged = np.array(data_rearranged) #Convert to array
x = data[1:2*nTrain+1,1:-1].astype(float)
y = data[1:2*nTrain+1,-1].astype(int)
x1 = x[:nTrain,:]
x2 = x[nTrain:,:]
y1 = y[:nTrain]
y2 = y[nTrain:]
    
performanceTable = []
fout = open("performance.txt",'w')
fout.write("InfoGain Precision Recall Accuracy F1\n")
numFeatures = len(x1[0])
model = DecisionTreeClassifier()
#model = GaussianNB()
for i in range(0,numFeatures): #Run classifier for each individual feature
    x1_current = list(x1[:,i]) #Exract column
    x2_current = list(x2[:,i])
    for j in range(0,len(x1)):
        x1_current[j] = [x1_current[j]] #Make each element a single-element list
    for j in range(0,len(x2)):
        x2_current[j] = [x2_current[j]]
    cm = cmCalc([i]) #Calculate the confusion matrix for the current feature being tested
    allMetrics = []
    allMetrics.append(i+1) #Feature ID
    allMetrics.append(metrics.infoGain(cm))
    #allMetrics.append(metrics.precision(cm))
    #allMetrics.append(metrics.recall(cm))
    #allMetrics.append(metrics.accuracy(cm))
    #allMetrics.append(metrics.f1(cm))
    #allMetrics.append(random.uniform(0,1)) #To enable random sorting
    performanceTable.append(tuple(allMetrics))
    for j in allMetrics[1:]: #Write everything except for feature ID
        fout.write(str(j)+" ")
    fout.write("\n")

#for i in range(1,len(files)): #For each output file (0th file is input)
for i in range(1,2): #I ended up only using the info gain metric. If you want to be able to use other metrics as well, comment this line, and uncomment previous line.
    performanceTable.sort(key=lambda tup: tup[i], reverse=True) #Sort by metric
    for j in range(1,len(performanceTable)):
        nextColumn = data[:,performanceTable[j][0]] #Add an x column
        data_rearranged[:,j] = nextColumn
    for j in data_rearranged:
        files[i].write(" ".join(j)+"\n")
