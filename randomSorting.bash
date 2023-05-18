#! /bin/bash

for file in `ls extractedFunctions_prep`
do
    cp extractedFunctions_prep/$file extractedFunctions/`shuf -i 100-999 -n1`_$file
done
