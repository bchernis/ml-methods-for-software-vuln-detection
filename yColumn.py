#! /usr/bin/python

import collections, os

def main():
    #bugList = ["021","065","074","270","428","448","10004","10010"]    
    #bugList = ["0005","0099","0247","0351","0440","0461","0525","0531","0621","0637","0666","0698","0743","0797","0812","0928","0965","1031","1069","1211","1230","1241","1256","1265","1277","1315","1332","1347","1371","1395","1411","1424","1442","1613","1736","1749","1781","1814","1823","1838","2168","2209","2218","2313","2462","2536","2590","2657","2691","2740","2758","2783","3034"]
    indir = '/root/Desktop/entropy/extractedFunctions'
    fout = open("stats/yColumn","w")
    fout.write("Class\n")
    for root, dirs, filenames in os.walk(indir):
        filenames.sort()
        for fileName in filenames:
            with open(indir+'/'+fileName,'r') as fin:
                if fileName[-5] =="1":
                    fout.write("1\n")
                else:
                    fout.write("0\n")

main()
