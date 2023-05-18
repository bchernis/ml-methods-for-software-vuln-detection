#! /bin/bash

rm -f stats/*txt

#countKeywords.bash
#discardChars.bash

#Select folder containing your extracted functions using arguments. extractedFunctions/ will be the one used by default, but if you supply a folder name as the first argument, then extractedFunctions will be cleared, and the contents of the folder (from your argument) will be copied into extractedFunctions.
if [ $# -ge 1 ]; then #If there are one or more arguments
   rm -f extractedFunctions/*
   cp $1/* extractedFunctions
fi

yColumn.py #Extracts "class" column
seqColumn.py #First column (with sample numbers)

#These are all the features that will be extracted. Uncomment the ones you want. Use "0", "1", or "2" as argument for charCount, entropy, and charDiversity to use no preprocessing, Method 1 preprocessing, or Method 2 preprocessing, respectively (see README for details on this). More documentation is present in each python script.
#charCount.py 0
#charCount.py 1
#charCount.py 2
entropy.py 0
#entropy.py 1
#entropy.py 2
#charDiversity.py 0
#charDiversity.py 2
charDiversity.py 2
#maxNestingDepth.py
#arrowCount.py
#ifCount.py
#ifComplexity.py
#whileCount.py
#forCount.py
#ngrams.py c 1 10000
#ngrams.py w 3 10000

#Combine all extracted features (currently in separate files in the stats/ directory) into allStats.txt
cd stats
sed -i /^$/d * #Remove blank lines
paste seqColumn *txt yColumn > ../allStats.txt #seqColumn and yColumn (files with sequence column and class column) do not end with .txt so that I can distinguish them from the other text files. Everything is combined into allStats.txt.
cd ..

#performanceCalc.py takes allStats.txt and generates several other tables, where the feature columns are sorted by info gain, accuracy, precision, etc. See code for the full list of output files.
#Uasge: performanceCalc.py <# training samples per class> <crossVal fold>
#Note: for no cross-validation, just use "0"
performanceCalc.py 50 0


#Classify.py runs the classifier
#For no cross-validation, use "0" for 4th argument
#Results are output to classify_result0.txt. After running, copy/paste into an excel spreadsheet. For each number of features that was used for the classification, we get TP, TN, FP, FN, accuracy, precision, info gain, and several other metrics. It is currently set to run the classifier with top 10, top 20, top 30, ... features. See code for classifier options available. Second argument selects which of the files output by performanceCalc.py (previous step) it will use. "0" means just use allStats.txt, "1" means use allStats_infoGain.txt, etc (see code).
#classify.py <classifier> <feature sorting> <total # training samples> <crossVal fold>
#Note: for no cross-validation, just use "0"

classify.py 0 0 100 0
