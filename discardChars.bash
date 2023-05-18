#! /bin/bash

cd dataset4
for i in $(ls)
do
    cat $i | sed "s/[0-9a-zA-Z]*//g" > ../dataset4a/$i
    cat $i | sed "s/[2-9]*//g" | sed -e "s/if/2/g" -e "s/int/3/g" -e "s/case/4/g" -e "s/return/5/g" -e "s/break/6/g" -e "s/struct/7/g" -e "s/char/8/g" -e "s/else/9/g" | sed "s/[a-zA-Z]*//g" > ../dataset4b/$i
done
cd ..
