#! /usr/bin/python

import string, fileinput, re, sys

def badChars(temp):
    if "|" in temp or '!' in temp or "==" in temp or "&&" in temp or '.' in temp or '-' in temp or '>' in temp or '<' in temp or temp.count('(') > 1 and temp.count(')') > 1:
        return True
    else:
        return False
    
def main():
	#State 1 means we are in a function head, state 2 means we are in a function body, and state 0 means we are in neither.
    state = 0
    functionNum = 0
    functionHead = ""
    for row in fileinput.input():
        if state != 2: #If we are not currently  in a function body
            if badChars(row): #If the current line being read has any characters that a function head cannot have
                state = 0
                continue            
            functionHead += row
            if not re.match(".*(if|while|do|for|switch)\s*\(",row) and '(' in row and not (')' in row and '){' not in row):
                state = 1
                functionHead = row
            if state == 1 and re.match(".*\){\s*",row):              
                state = 2
                if badChars(functionHead):                   
                    state = 0
                    continue
                functionNum += 1
                if sys.argv[1] == "combo_0": #Functions without bugs will be output to files with names starting with "0"
                    fout = open('extractedFunctions_prep/'+str(functionNum).zfill(5),'w')
                elif sys.argv[1] == "combo_1": #Buggy functions will be output to files with names starting with "1"
                    fout = open('extractedFunctions_prep/'+str(functionNum+10000),'w')
                else:
                    print "Please give a valid combo file!!!"
                fout.write(functionHead)
        else: #If we are in a function body
            fout.write(row)
            if re.match("^}.*",row): #If line starts with right curly brace, this signifies the end of the function
                state = 0
main()
