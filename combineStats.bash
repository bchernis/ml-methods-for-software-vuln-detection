#! /bin/bash



#Attention: This script is currently faulty!!!!!




sed -e "s/\t/ /g" allStats_03.txt | cut -d ' ' -f 2-21 > allStats01.txt
sed -e "s/\t/ /g" allStats_05.txt | cut -d ' ' -f 2-75 > allStats02.txt
sed -e "s/\t/ /g" allStats_08.txt | cut -d ' ' -f 2-200 > allStats03.txt
sed -e "s/\t/ /g" allStats_11.txt | cut -d ' ' -f 2-176 > allStats04.txt
sed -e "s/\t/ /g" allStats_14.txt | cut -d ' ' -f 2-251 > allStats05.txt
sed -e "s/\t/ /g" allStats_16.txt | cut -d ' ' -f 2-101 > allStats06.txt
sed -e "s/\t/ /g" allStats_19.txt | cut -d ' ' -f 2-141 > allStats07.txt
sed -e "s/\t/ /g" allStats_23.txt | cut -d ' ' -f 2-96  > allStats08.txt
sed -e "s/\t/ /g" allStats_26.txt | cut -d ' ' -f 2-61  > allStats09.txt
sed -e "s/\t/ /g" allStats_29.txt | cut -d ' ' -f 2-61  > allStats10.txt

paste allStats_seq.txt allStats01.txt allStats02.txt allStats03.txt allStats04.txt allStats05.txt allStats_class.txt > allStats_chars.txt
paste allStats_seq.txt allStats06.txt allStats07.txt allStats08.txt allStats09.txt allStats10.txt allStats_class.txt > allStats_words.txt
paste allStats_seq.txt allStats01.txt allStats02.txt allStats03.txt allStats04.txt allStats05.txt allStats06.txt allStats07.txt allStats08.txt allStats09.txt allStats10.txt allStats_class.txt > allStats_chars_words.txt
