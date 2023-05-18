#! /bin/bash

rm -f keywordStats.txt
for i in $(cat keywords.txt)
do echo $i $(grep $i extractedFunctions/* | wc -l) >> keywordStats.txt
done
sort -nrk2 keywordStats.txt > keywordStats_sorted.txt
