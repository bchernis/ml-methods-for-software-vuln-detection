#! /bin/bash

rm -f extractedFunctions_prep/*
rm -f extractedFunctions/*
rm -f uncommented/*

for i in `ls -1 originals/*[ab].c | sed 's/\.c//g' | awk '{print substr($1,11)}'`
	 do
	     gcc -fpreprocessed -dD -E originals/${i}.c | sed '1d' > uncommented/${i}1
	     fixCurlyBraces.py uncommented/${i}1 > uncommented/${i}2 #Remove spaces from curly braces, for easier parsing
done

cat uncommented/*a2 > combo_0 #nobugs
cat uncommented/*b2 > combo_1 #bugs

#Parse functions without and with bugs into extractedFunctions_prep
functionParser.py combo_0
functionParser.py combo_1

#Randomly select functions from each category
cd extractedFunctions_prep
ls 0???? | sort -R | head -100 | nl -n rz | awk '{print "cp "$2" ../extractedFunctions/"substr($1,4)"_"$2}' > temp.bash
ls 1???? | sort -R | head -100 | nl -n rz | awk '{print "cp "$2" ../extractedFunctions/"substr($1,4)"_"$2}' >> temp.bash
source temp.bash
rm -f temp.bash
cd ..
