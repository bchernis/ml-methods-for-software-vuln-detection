#! /usr/bin/python

import sys
from sklearn.naive_bayes import GaussianNB
from sklearn import neighbors, cluster
from sklearn.neural_network import MLPClassifier
from sklearn.svm import SVC
from sklearn.metrics import confusion_matrix
from sklearn.ensemble import RandomForestClassifier
from sklearn.tree import DecisionTreeClassifier
#from sklearn.cross_validation import cross_val_score
from sklearn.cross_validation import KFold
import metrics #My file, not a standard library
import numpy as np




####Very similar to classify.py. Man difference is the last block of code#########





def cmCalc(features):
    if crossVal:
        kf = KFold(len(y),n_folds=crossVal)
        cm = np.array([[0,0],[0,0]])
        for i in range(0,10):
            for train,test in kf:
                if classifierID == 2: #If kmeans clustering is used, no y needed
                    model.fit(x[train][:,features])
                else:
                    model.fit(x[train][:,features],y[train])
                y2predicted = model.predict(x[test][:,features])
                cm_current = confusion_matrix(y[test],y2predicted)
                if cm_current.shape[0] == 1:
                    cm_current = np.array([[cm_current[0][0]/2,0],[0,cm_current[0][0]/2]])
                cm += cm_current
            if classifierID != 5: #Only need 1 classification run if not using random forest
                break
        if classifierID == 5: #For random forests, take an average of 10 classification runs
            cm /= 10
        return cm
    model.fit(x1[:,features],y1)
    y2predicted = model.predict(x2[:,features])
    return confusion_matrix(y2,y2predicted)

def writeResult(featureID, cm, fout, featureSelection):
    n = cm[0][0] + cm[0][1] + cm[1][0] + cm[1][1]
    fout.write(str(featureID)+" ")
    fout.write(str(metrics.infoGain(cm))+" ")
    fout.write(str(metrics.precision(cm))+" ")
    fout.write(str(metrics.recall(cm))+" ")
    fout.write(str(metrics.accuracy(cm))+" ")
    fout.write(str(metrics.f1(cm))+" ")
    fout.write(str(cm[1][1])+" ")
    fout.write(str(cm[0][1])+" ")
    fout.write(str(cm[0][0])+" ")
    fout.write(str(cm[1][0])+" ")
    for i in featureSelection:
        fout.write(str(i)+" ")
    fout.write("\n")
    
#Parameters
classifierID = int(sys.argv[1]) #Classifier
featureSortMetric = int(sys.argv[2]) #Feature sorting method
nTrain = int(sys.argv[3]) #Total size of training dataset
crossVal = int(sys.argv[4]) #Use cross validation or not

print sys.argv

if featureSortMetric == 0:
    inputFile = "allStats.txt"
elif featureSortMetric == 1:
    inputFile = "allStats_infoGain.txt"
elif featureSortMetric == 2:
    inputFile = "allStats_precision.txt"
elif featureSortMetric == 3:
    inputFile = "allStats_recall.txt"
elif featureSortMetric == 4:
    inputFile = "allStats_accuracy.txt"
elif featureSortMetric == 5:
    inputFile = "allStats_f1.txt"
elif featureSortMetric == 6:
    inputFile = "allStats_random.txt"
else:
    print "Choose a valid feature sorting method"
    exit()

data = []
for row in open(inputFile): #Read data as 2D list
#for row in open("temp.txt"): #Read data as 2D list
    data.append([temp for temp in row.split()])
for i in range(1,len(data)):
    for j in range(1,len(data[i])-1):
        if classifierID == 0:
            data[i][j] = str(float(data[i][j])+.0000001) #Fixes division by zero issue for NB
        if classifierID == 3 or classifierID == 4:
            data[i][j] = str(float(data[i][j])*1000) #Prevents NN and SVM from crashing
data = np.array(data) #Convert to array, and remove first row and column
x = data[1:2*nTrain+1,1:-1].astype(float)
y = data[1:2*nTrain+1,-1].astype(int)
x1 = x[:nTrain,:]
x2 = x[nTrain:,:]
y1 = y[:nTrain]
y2 = y[nTrain:]
#print data.shape, x1.shape, x2.shape, y1.shape, y2.shape

if classifierID == 0:
    print "Naive Bayes"
    model = GaussianNB()
elif classifierID == 1:
    print "KNN"    
    model = neighbors.KNeighborsClassifier(n_neighbors=2)
elif classifierID == 2:
    print "KMeans"    
    model = cluster.KMeans(n_clusters=2) #Crashes if default parameters with n_clusters=1
    #model.fit(x1) #Unsupervised learning, so no "y1" is needed
    #y2predicted = model.predict(x2)
    #print confusion_matrix(y2,y2predicted)
    #exit()
elif classifierID == 3:
    print "Neural Network"    
    model = MLPClassifier()
    #model = MLPClassifier(solver='lbfgs',alpha=1e-6,hidden_layer_sizes=(10),random_state=1)
    model.fit(x1,y1)
elif classifierID == 4:
    print "SVM"    
    model = SVC()
elif classifierID == 5:
    print "Random Forest"    
    model = RandomForestClassifier()
    #model = RandomForestClassifier(bootstrap=True, class_weight=None, criterion='gini', max_depth=None, max_features='auto', max_leaf_nodes=None, min_samples_leaf=1, min_samples_split=2, min_weight_fraction_leaf=0.0, n_estimators=10, n_jobs=2, oob_score=False, random_state=None, verbose=0, warm_start=False)
elif classifierID == 6:
    print "Decision Tree"
    model = DecisionTreeClassifier()

fout = open("classify_result_bruteforce.txt","w")
fout.write("FeatureID InfoGainx1000 Precision Recall Accuracy F1 TP FP TN FN charCount0 charCount1 charCount2 entropy0 entropy1 entropy2 charDiversity0 charDiversity1 charDiversity2 maxNestingDepth arrowCount ifCount ifComplexity whileCount forCount\n")

nFeatures = x1.shape[1]
for i in range(1,2**nFeatures): #Construct all subsets of features (brute force)
    binStr = bin(i)[2:] #Binary string storing feature combo
    binStr = "0"*(nFeatures-len(binStr))+binStr #Add leading zeroes
    featureCombo = list(binStr)
    featureCombo2 = []
    for j in range(0,len(featureCombo)): #Replace "1"s with feature numbers
        if featureCombo[j] == "1":
            featureCombo2.append(j)
    #for j in range(len(featureCombo)-1,-1,-1): #Go backwards to remove zeroes
        #if featureCombo[j] == "0":
            #del featureCombo[j]
    cm = cmCalc(featureCombo2)
    #print i, cm, featureCombo
    writeResult(i,cm,fout,featureCombo)
