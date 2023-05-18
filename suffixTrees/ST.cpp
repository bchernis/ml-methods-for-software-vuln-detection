#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <dirent.h>
#include <algorithm>
#include <cmath>
#include <string.h>
#include <cmath>
using namespace std;

map<string, int> spam_db,ham_db;
set<string> dict;
set<string> stopwords;

vector<string> spam,ham;
int *spam_ids,*ham_ids;

int spam_num, ham_num;
int spam_train, ham_train;
int spam_test, ham_test;

struct Score{
	double spam;
	double ham;
	Score(double s, double h){
		spam=s;
		ham=h;
	}
};

vector<Score > spam_score,ham_score;

const int TEST = 50;
const int NUM = 27;
const int MAX_DEEP = 1000;

ofstream fout;

//Remove symbols other than letters and "'", and make sure all letters are lowercase
int remove_punctuation(char* buf){
	int i,j;
	i=j=0;
	for(i=0;i<strlen(buf);i++){
		char ch=buf[i];
		if((ch>='a' && ch<='z') || (ch>='A' && ch<='Z')){
			if(ch>='A' && ch<='Z') ch+='a'-'A';
			buf[j]=ch;
			j++;
		}
	}
	buf[j]=0;
	return 0;
}

//Stem all remaining words.
void stem(char* buf){
	//Gets rid of plurals and -ed or -ing suffixes
	int len,k;
	len=strlen(buf);
	k=len-1;
	if(k>=0 && buf[k] == 's'){
		if(len>=4 && strcmp(buf+len-4,"sses")==0) buf[len-2]=0;
		else if(len>=3 && strcmp(buf+len-3,"ies")==0) buf[len-2]=0;
		else if(k>0 && buf[k-1]!='s') buf[len-1]=0;
	}

	len=strlen(buf);
 	if(len>=3 && strcmp(buf+len-3,"eed")==0) buf[len-1]=0;

 	len=strlen(buf);
 	if(len>=2 && strcmp(buf+len-2,"ed")==0) buf[len-2]=0;

 	len=strlen(buf);
 	if(len>=3 && strcmp(buf+len-3,"ing")==0) buf[len-3]=0;

 	len=strlen(buf);
 	if(len>=2 && strcmp(buf+len-2,"at")==0) buf[len++]='e',buf[len]=0;
 	else if(len>=2 && strcmp(buf+len-2,"bl")==0) buf[len++]='e',buf[len]=0;
	else if(len>=2 && strcmp(buf+len-2,"iz")==0) buf[len++]='e',buf[len]=0;

}

//Read words from file
int get_words(const string& filename, set<string>& db, bool stopword=false){
	string s;
	char buf[1000];
	ifstream infile;
	infile.open(filename.c_str());
	db.clear();

	while (!infile.eof())
	{
		buf[0]=0;
		infile>>buf;
		remove_punctuation(buf);

		stem(buf);
		if(strlen(buf)<1) continue;

		if((stopword || stopwords.find(buf)==stopwords.end()) && db.find(buf) == db.end())
		{
			db.insert(buf);
		}
	}
	return 0;
}
int get_words(const string& filename, vector<string>& db, bool stopword=false){
	string s;
	char buf[1000];
	ifstream infile;
	infile.open(filename.c_str());
	db.clear();

	while (!infile.eof())
	{
		buf[0]=0;
		infile>>buf;
		remove_punctuation(buf);

		stem(buf);
		if(strlen(buf)<1) continue;

		if((stopword || stopwords.find(buf)==stopwords.end()))
			db.push_back(buf);

	}
	return 0;
}

void preprocessing(){
	get_words("stopwords.txt",stopwords,true);
}

//Get file listing from folder
int find_files(const char* path,vector<string>& db){
	DIR *dir;
	struct dirent *ent;

	string filename;
	if ((dir = opendir (path)) != NULL) {
	  //Print all the files and directories within directory
	  while ((ent = readdir (dir)) != NULL) {
	  	if(strstr(ent->d_name,".txt")==NULL) continue;
	  	filename=path;
	  	filename+=  ent->d_name;
		db.push_back(filename);
	  }
	  closedir (dir);
	} else {
	  //Could not open directory
	  return -1;
	}
	return 0;
}

void init_data(){
	find_files("spam/",spam);
	find_files("ham/",ham);
}

//Split training and test data
void split_data(){
	int i;
	spam_num=spam.size();
 	spam_ids=new int[spam_num];
	for(i=0;i<spam_num;i++)
		spam_ids[i]=i;
	random_shuffle(spam_ids,spam_ids+spam_num);

 	ham_num=ham.size();
 	ham_ids=new int[ham_num];
	for(i=0;i<ham_num;i++)
		ham_ids[i]=i;
	random_shuffle(ham_ids,ham_ids+ham_num);

	if(spam_num>ham_num) spam_num=ham_num;
	else ham_num=spam_num;

	spam_test=spam_num*TEST/100;
	spam_train=spam_num-spam_test;

	ham_test=ham_num*TEST/100;
	ham_train=ham_num-ham_test;

	fout<<spam_num<<" spam files"<<endl;
	fout<<ham_num<<" ham files"<<endl;
	fout<<endl;

	cout<<spam_num<<" spam files"<<endl;
	cout<<ham_num<<" ham files"<<endl;
	cout<<endl;
}


//For debugging
void display(map<string,int>& db){
	fout<<"size = "<<db.size()<<endl;
	for(map<string,int>::iterator itr=db.begin();itr!=db.end();itr++)
		fout<<itr->first<<" "<<itr->second<<endl;
}

void display(set<string>& db){
	fout<<"size = "<<db.size()<<endl;
	for(set<string>::iterator itr=db.begin();itr!=db.end();itr++)
		fout<<*itr<<endl;
}


//SuffixTree
struct Node{
	char name;
	int num,sum;
	int level;
	Node* children[NUM]; //'a'~'z','.'
	Node(){
		name='A';
		num=sum=level=0;
		for(int i=0;i<NUM;i++)	children[i]=0;
	}
	Node(char ch, int v){
		name=ch;
		num=1,sum=0;
		level=v;
		for(int i=0;i<NUM;i++)	children[i]=0;
	}
	void display(){
		cout<<"("<<name<<","<<level<<"-"<<num<<"-"<<sum<<") ";
	}
};

string pattern="abcdefghijklmnopqrstuvwxyz'";
class SuffixTree{
public:
	SuffixTree(){
		root=new Node();
		for(int i=0;i<MAX_DEEP;i++)
			freq[i]=0;
	}

	void AddString(const string& word){
		Node* node=root;

		for(int i=0;i<word.size();i++){
			char ch=word[i];

			int id=pattern.find(ch);

			if(id<0 || id>=NUM) continue;

			if(node->children[id]) node->children[id]->num++;
			else{
				node->children[id]=new Node(ch,node->level+1);
				root->num++;
			}
			node->sum++;
			node=node->children[id];
			freq[node->level]++;
		}
		if(word.size()>1) AddString(word.substr(1));
	}

	void training(set<string>& db){
		for(set<string>::iterator itr=db.begin();itr!=db.end();itr++)
			AddString(*itr);
	}

	void display(bool all=false){
		if(all) display(root);
		else root->display();
		cout<<endl;
	}

	void display(Node* node){
		node->display();
		for(int i=0;i<NUM;i++)
			if(node->children[i]) display(node->children[i]);
	}

	double phi(double p){
        //return 1;
	    return p;
	    return pow(p,2);
		return sqrt(p);
	}

	double score(const string& s){
		double res=0.0,temp;
		Node* p=root;
		int i,j;
		for(i=0;i<s.size();i++){
			temp=0;
			for(j=i;j<s.size();j++){
				int id=pattern.find(s[j]);
				if(!p->children[id]) break;
				temp+=phi((double)p->children[id]->num/p->sum);
				p=p->children[id];
			}
			if(res<temp) res=temp;
		}
		//string m=s.substr(0,i);
		return  res;
	}

	double score(set<string>& db){
		double res=0.0;
		for(set<string>::iterator itr=db.begin();itr!=db.end();itr++)
			res+=score(*itr);
		return res;
	}

private:
	Node* root;
	int freq[MAX_DEEP];
};

SuffixTree spam_tree,ham_tree;

//training suffix tree
void suffix_tree(){
	int i;
	set<string> db;
	for(i=0;i<spam_train;i++){
		get_words(spam[spam_ids[i]],db);
		spam_tree.training(db);
	}

	for(i=0;i<ham_train;i++){
		get_words(ham[ham_ids[i]],db);
		ham_tree.training(db);
	}
}

void testing(){
	int i,j,k;
	set<string> db;
	double score_spam, score_ham;
	double p1,p2;


	fout<<"testing spam "<< spam_test<<endl;

	for(i=spam_train;i<spam_num;i++){
		get_words(spam[spam_ids[i]],db);

		score_spam=spam_tree.score(db);
		score_ham=ham_tree.score(db);

		spam_score.push_back(Score (score_spam,score_ham));

	}

	fout<<endl;

	fout<<"testing ham "<< ham_test<<endl;

	for(i=ham_train;i<ham_num;i++){

		get_words(ham[ham_ids[i]],db);

		score_spam=spam_tree.score(db);
		score_ham=ham_tree.score(db);

		ham_score.push_back(Score (score_spam,score_ham));
	}

	fout<<endl<<"Done!!!"<<endl<<endl;
	cout<<endl<<"Done!!!"<<endl<<endl;
}

void scoring(double th){
	int SS,SH,HS,HH;
	int i;
	double score_spam, score_ham;

	SS=SH=HS=HH=0;
	for(i=0;i<spam_score.size();i++){
		score_spam=	spam_score[i].spam;
		score_ham=	spam_score[i].ham;
		if(score_spam*th>=score_ham) SS++;
		else SH++;
	}
	for(i=0;i<ham_score.size();i++){
		score_spam=	ham_score[i].spam;
		score_ham=	ham_score[i].ham;
		if(score_spam*th<=score_ham) HH++;
		else HS++;
	}
	//double SR = (double)SS/(SS+SH);
	//double SP = (double)SS/(SS+HS);
	double accuracy = (double)(SS+HH)/(SS+HH+SH+HS);
	double TPR = (double)SS/(SS+SH);
	double FPR = (double)HS/(HH+HS);
	double TNR = 1 - FPR;
	double FNR = 1 - TPR;


	fout << left << fixed << setprecision (2) << setw(9) << th << setw(9) << SS << setw(9) << SH << setw(9) << HS <<setw(9) << HH << setw(9) << setprecision (3) << accuracy << setw(9) << TPR << setw(9) << FPR << setw(9) << TNR <<setw(9) << FNR << endl;
	cout << left << fixed << setprecision (2) << setw(9) << th << setw(9) << SS << setw(9) << SH << setw(9) << HS <<setw(9) << HH << setw(9) << setprecision (3) << accuracy << setw(9) << TPR << setw(9) << FPR << setw(9) << TNR <<setw(9) << FNR << endl;
}

int main(){
	fout.open("out.txt");
	//initialize data
	spam_db.clear();
	ham_db.clear();
	dict.clear();

	spam=vector<string>();
	ham=vector<string>();

	spam_score=vector<Score>();
	ham_score=vector<Score>();

	//load data
	fout<<"Loading files..."<<endl;
	cout<<"Loading files..."<<endl;

	init_data();
	fout<<spam.size()<<" spam files"<<endl;
	fout<<ham.size()<<" ham files"<<endl;
	fout<<endl;

	cout<<spam.size()<<" spam files"<<endl;
	cout<<ham.size()<<" ham files"<<endl;
	cout<<endl;

	//split test data and training data
	fout<<100-TEST<<"% used for training and "<<TEST<<"% used for testing"<<endl<<endl;
	cout<<100-TEST<<"% used for training and "<<TEST<<"% used for testing"<<endl<<endl;

	fout<<"Splitting data..."<<endl;
	cout<<"Splitting data..."<<endl;

	split_data();
	fout<<"Spam: "<<spam_train<<" training files and "<<spam_test<<" test files"<<endl;
	fout<<"Ham:  "<<ham_train<<" training files and "<<ham_test<<" test files"<<endl;
	fout<<endl;

	cout<<"Spam: "<<spam_train<<" training files and "<<spam_test<<" test files"<<endl;
	cout<<"Ham:  "<<ham_train<<" training files and "<<ham_test<<" test files"<<endl;
	cout<<endl;

	fout<<"Making Suffix Tree..."<<endl;
	cout<<"Making Suffix Tree..."<<endl;

	suffix_tree();

	fout<<endl;
	cout<<endl;

	fout<<"Testing Data..."<<endl;
	cout<<"Testing Data..."<<endl;
	testing();
	fout<<endl;
	cout<<endl;

	//scoring
	fout<<"Stats:"<<endl;
	cout<<"Stats:"<<endl;

	fout << "TH       SS       SH       HS       HH       Accuracy TPR      FPR      TNR      FNR" << endl;
	cout << "TH       SS       SH       HS       HH       Accuracy TPR      FPR      TNR      FNR" << endl;

    for(double i=.01;i<=10;i+=.01)
		scoring(i);
}

