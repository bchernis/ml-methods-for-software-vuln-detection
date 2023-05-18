#! /usr/bin/python

import math

#This file imported by other .py files for calculation of metrics.
def infoGain(conMatrix):
    TP = float(conMatrix[1][1])
    TN = float(conMatrix[0][0])
    FP = float(conMatrix[0][1])
    FN = float(conMatrix[1][0])
    n = TP + TN + FP + FN
    p1 = (TN + FN) / n
    p2 = (TP + FP) / n
    
    if p1 != 0:
        p11 = TN / (TN + FN)
        p12 = FN / (TN + FN)
    else:
        p11 = p12 = 0
    if p2 != 0:
        p21 = FP / (TP + FP)
        p22 = TP / (TP + FP)
    else:
        p21 = p22 = 0
        
    if p11 == 0 or p12 == 0:
        info_11 = info_12 = 0
    else:
        info_11 = p1 * p11 * math.log(p11)
        info_12 = p1 * p12 * math.log(p12)
    if p21 == 0 or p22 == 0:
        info_21 = info_22 = 0
    else:
        info_21 = p2 * p21 * math.log(p21)
        info_22 = p2 * p22 * math.log(p22)
        
    info_1 = info_11 + info_12
    info_2 = info_21 + info_22
    info_initial = math.log(.5) #50/50 split originally
    info_final = info_1 + info_2
    #print conMatrix, info_final
    return (info_final-info_initial)*1000 #Scaling doesn't matter

def precision(conMatrix):
    TP = float(conMatrix[1][1])
    TN = float(conMatrix[0][0])
    FP = float(conMatrix[0][1])
    FN = float(conMatrix[1][0])
    if TP + FP == 0:
        return 0
    else:
        return TP/(TP+FP)

def recall(conMatrix):
    TP = float(conMatrix[1][1])
    TN = float(conMatrix[0][0])
    FP = float(conMatrix[0][1])
    FN = float(conMatrix[1][0])
    return TP/(TP+FN)

def accuracy(conMatrix):
    TP = float(conMatrix[1][1])
    TN = float(conMatrix[0][0])
    FP = float(conMatrix[0][1])
    FN = float(conMatrix[1][0])
    return (TP+TN)/(TP+TN+FP+FN)

def f1(conMatrix):
    p = precision(conMatrix)
    r = recall(conMatrix)
    if p + r == 0:
        return 0
    else:
        return 2*p*r/(p+r)
