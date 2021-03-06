#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctype.h>
#include <map>
#include <iomanip>
#include <cstdlib>
#include <sstream>

using namespace std;

int lineNum=1,offset=1, lastLineOffset=0, totalInstr=0;

vector<string> symOrderRecord;
vector<string> symNotUse;

struct symbol
{
	string name;
	int val;
	bool multiDef;
	int moduleIdx;
	bool defNoUse;
	symbol(string name, int val) { this->name = name; this->val = val; multiDef=false; moduleIdx=-1; defNoUse=true;}
};

struct module 
{
	int base;
	int numInstr;
	module() {this->base=0; this->numInstr=0;}
};

int strToInt(string str) 
{
	stringstream ss;
	ss<<str;
	int num;
	ss>>num;
	return num;
}

// clean symbol table and module list which have been dynamically allocated
void cleanMem(vector<module*>& mList, map<string, symbol*>& symTable)
{
	for(unsigned int i=0;i<mList.size();i++) delete mList[i];
	for(map<string, symbol*>::iterator it=symTable.begin();it!=symTable.end();++it) delete it->second;
}

// close the program when met with a parse error
void close(vector<module*>& mList, map<string, symbol*>& symTable)
{
	cleanMem(mList, symTable);
	exit(0);
}

// check whether a give string is valid type (I, R, A, E)
bool isType(string& str)
{
	return str=="I"||str=="R"||str=="A"||str=="E";
}

bool isDigit(string& str)
{
	for(unsigned int i=0;i<str.length();i++) {
		if(!isdigit(str[i])) return false;
	}
	return true;
}

bool isSymbol(string& str)
{
	if(!str.length()) return false;
	for(unsigned int i=0;i<str.length();i++) {
		if(!i) {
			if(!isalpha(str[i])) return false;
		}
		else {
			if(!isalnum(str[i])) return false;
		}
	}
	return true;
}


string read(ifstream& ifile)
{
	string str;
	ifile>>str;
	return str;
}

void parseErrMsg(int type, bool special)
{
	static string errstr[] = {"NUM_EXPECTED", "SYM_EXPECTED", "ADDR_EXPECTED", 
	"SYM_TOLONG", "TO_MANY_DEF_IN_MODULE", "TO_MANY_USE_IN_MODULE", "TO_MANY_INSTR"};

	// special case: eof ends with \n
	if(special && offset==1) cout<<"Parse Error line "<<lineNum-1<<" offset "<<lastLineOffset<<": "<<errstr[type]<<endl;
	else cout<<"Parse Error line "<<lineNum<<" offset "<<offset<<": "<<errstr[type]<<endl;
}

void skipDelimiter(ifstream& ifile)
{
	while(!ifile.eof()) {
		char ch=ifile.peek();
		if(ch=='\n'){ 
			lastLineOffset = offset;
			offset=1;
			lineNum++;
			ifile.get();
		}
		else if(ch==' '||ch=='\t') { offset++; ifile.get(); }
		else break;
	}
}

// idx: index of current module
void readDefList1(ifstream& ifile, vector<module*>& mList, int idx, map<string, symbol*>& symTable)
{
	string tmp=read(ifile);
	if(!isDigit(tmp)) {parseErrMsg(0, false); close(mList, symTable);}
	if(strToInt(tmp)>16) { parseErrMsg(4, false); close(mList, symTable); }
	offset+=tmp.length();

	for(int i=0;i<strToInt(tmp);i++) {
		// read symbol name
		skipDelimiter(ifile);
		if(ifile.eof()) { parseErrMsg(1, true); close(mList, symTable);}
		string symName=read(ifile);
		if(!isSymbol(symName)) { parseErrMsg(1, false); close(mList, symTable);}
		offset+=symName.length();

		// read symbol value
		skipDelimiter(ifile);
		if(ifile.eof()) {parseErrMsg(0, true); close(mList, symTable);}
		string symVal=read(ifile);
		if(!isDigit(symVal)){ parseErrMsg(0, false); close(mList, symTable);}
		offset+=symVal.length();
		
		// check whether the symbol has multiple defined
		map<string,symbol*>::iterator got=symTable.find(symName);
		if(got==symTable.end()) {
			symbol* newSym = new symbol(symName, strToInt(symVal)+mList[idx]->base );
			symTable.insert(pair<string, symbol*>(symName, newSym));
			symOrderRecord.push_back(symName);
			newSym->moduleIdx=idx;
		}
		else got->second->multiDef=true;
		
	}
}

void readUseList1(ifstream& ifile, vector<module*>& mList, map<string, symbol*>& symTable)
{
	string tmp=read(ifile);
	if(!isDigit(tmp)) { parseErrMsg(0, false); close(mList, symTable); }
	if(strToInt(tmp)>16) {parseErrMsg(5, false);close(mList, symTable);}
	offset+=tmp.length();

	for(int i=0;i<strToInt(tmp);i++) {
		skipDelimiter(ifile);
		if(ifile.eof()) { parseErrMsg(1, true); close(mList, symTable);}
		string symName=read(ifile);
		if(!isSymbol(symName)) { parseErrMsg(1, false); close(mList, symTable);}
		offset+=symName.length();
	}
}

// idx: index of current module, to record number of instructions in the current module
void readInstrList1(ifstream& ifile,vector<module*>& mList, int idx, map<string, symbol*>& symTable)
{
	string tmp=read(ifile);
	if(!isDigit(tmp))  { parseErrMsg(0, false); close(mList, symTable);}
	totalInstr += strToInt(tmp);
	if(totalInstr>512) { parseErrMsg(6, false); close(mList, symTable);}
	mList[idx]->numInstr=strToInt(tmp);
	offset+=tmp.length();

	for(int i=0;i<strToInt(tmp);i++) {
		skipDelimiter(ifile);

		if(ifile.eof()) { parseErrMsg(2,true); close(mList, symTable); }
		string newType=read(ifile);
		if(!isType(newType)) { parseErrMsg(2, false); close(mList, symTable); }
		offset+=newType.length();

		skipDelimiter(ifile);
		if(ifile.eof()) {parseErrMsg(2,true); close(mList, symTable);}
		string newBuf = read(ifile);
		if(!isDigit(newBuf)) {parseErrMsg(2, false); close(mList, symTable);}
		offset+=newBuf.length();
	}
}

void readfile1(ifstream& ifile, vector<module*>& mList, map<string, symbol*>& symTable)
{
	// i: idx of new module in the mList
	int i=0;
	while(!ifile.eof()) {
		skipDelimiter(ifile);
		if(ifile.eof()) break;		
		else {
			mList.push_back(new module());
			
			// configure new module base address
			if(!i) mList[i]->base=0;
			else mList[i]->base=mList[i-1]->base + mList[i-1]->numInstr;

			readDefList1(ifile, mList, i, symTable);
			skipDelimiter(ifile);
			if(ifile.eof()) {parseErrMsg(0, true); close(mList, symTable);}

			readUseList1(ifile, mList, symTable);
			skipDelimiter(ifile);
			if(ifile.eof()) {parseErrMsg(0, true); close(mList,  symTable);}

			readInstrList1(ifile, mList, i, symTable);
			skipDelimiter(ifile);
		}
		i++;
	}
}

void printSymTable(vector<module*>& mList, map<string, symbol*>& symTable)
{
	//check whether the address exceeds the size of module (rule 5)
	for(unsigned int i=0; i<symOrderRecord.size();i++) {
		symbol* sym = symTable.find(symOrderRecord[i])->second;
		if(sym->val>=mList[sym->moduleIdx]->numInstr+mList[sym->moduleIdx]->base) {
			cout<<"Warning: Module "<<sym->moduleIdx+1
			<<": "<<sym->name<<" to big "
			<<sym->val<<" (max="<<mList[sym->moduleIdx]->numInstr-+mList[sym->moduleIdx]->base-1	
			<<") assume zero relative"<<endl;
			sym->val=0;
		}
	}
	cout<<"Symbol Table "<<endl;
	for(unsigned int i=0;i<symOrderRecord.size();i++) {
		symbol* sym = symTable.find(symOrderRecord[i])->second;
		cout<<sym->name<<"="<<sym->val;
		if(sym->multiDef) cout<<" Error: This variable is multiple times defined; first value used";
		cout<<endl;
	}	
}

void readDefList2(ifstream& ifile, int numDef)
{
	string word;
	for(int i=0;i<numDef;i++) {
		// read symbol(def) name and symbol address
		ifile>>word; ifile>>word;
	}
}

void readUseList2(ifstream& ifile,map<string, symbol*>& symTable, int numUse, string* useList, bool used[])
{
	string word;
	for(int i=0;i<numUse;i++) {
		// read symbol(use) name
		ifile>>word;
		useList[i]=word;
		used[i]=false;
		map<string, symbol*>::iterator it=symTable.find(word);
		// mark whether a symbol is defined AND used
		if(it!=symTable.end()) it->second->defNoUse=false;
	}
}

// bool used[]: check whether a symbol appeared in the uselist but was not actually used
// depends on specific module so cannot make this a member variable of symbol
void readInstrList2(ifstream& ifile, vector<module*>& mList, int idx, map<string, symbol*>& symTable, int numInstr, int& memIdx, string* useList, bool used[], int numUse)
{
	string word;
	for(int i=0;i<numInstr;i++) {
		cout<<setw(3)<<setfill('0')<<memIdx++<<": ";

		string type; ifile>>type;
		string address; ifile>>address;

		int opcode, addcode;
		if(address.length()<4) { opcode=0; addcode=strToInt(address); }
		else { opcode=address[0]-'0'; addcode=strToInt(address.substr(1)); }

		if(type=="I") {
			if(address.length()>4) cout<<"9999 Error: Illegal immediate value; treated as 9999"<<endl;
			else cout<<setw(4)<<setfill('0')<<address<<endl;
		}
		else if(type=="R") {
			if(address.length()>4) cout<<"9999 Error: Illegal opcode; treated as 9999"<<endl;
			else {
				// Relative address exceeds module size; zero used
				if(addcode>=mList[idx]->numInstr) {
					cout<<opcode;
					cout<<setw(3)<<setfill('0')<<(strToInt("000")+mList[idx]->base)
					<<" Error: Relative address exceeds module size; zero used"<<endl;
				}
				else {
					cout<<opcode;
					cout<<setw(3)<<setfill('0')<<addcode+mList[idx]->base<<endl;
				}
			}
		}
		else if(type=="A") {
			if(address.length()>4) cout<<"9999 Error: Illegal opcode; treated as 9999"<<endl;
			else if(addcode>=512) cout<<opcode<<"000 Error: Absolute address exceeds machine size; zero used"<<endl;
			else {
				cout<<opcode;
				cout<<setw(3)<<setfill('0')<<addcode<<endl;
			}

		}
		else if(type=="E") {
			if(address.length()>4) cout<<"9999 Error: Illegal opcode; treated as 9999"<<endl;
			else if(addcode>=numUse) cout<<setw(4)<<setfill('0')<<address<<" Error: External address exceeds length of uselist; treated as immediate"<<endl;
			else if(addcode<numUse) {
				used[addcode]=true;
				cout<<opcode;
				map<string ,symbol*>::iterator got=symTable.find(useList[addcode]);
				if(got==symTable.end()) cout<<"000 Error: "<<useList[addcode]<<" is not defined; zero used"<<endl;
				else cout<<setw(3)<<setfill('0')<<symTable.find(useList[addcode])->second->val<<endl;
			}
		}
		else;
	}
	for(int i=0;i<numUse;i++) {
		if(!used[i]) cout<<"Warning: Module "<<idx+1<<": "<<useList[i]<<" appeared in the uselist but was not actually used"<<endl;
	}
}

void printWarning(map<string, symbol*>& symTable)
{
	for(unsigned int i=0;i<symOrderRecord.size();i++) {
		symbol* sym = symTable.find(symOrderRecord[i])->second;
		if(sym->defNoUse) {
			cout<<"Warning: Module "<<sym->moduleIdx+1<<": "
			<<sym->name<<" was defined but never used"<<endl;
		}
	}
}

void readfile2(ifstream& ifile, vector<module*>& mList, map<string, symbol*>& symTable)
{
	// i: idx of current module
	int i=0, memIdx=0;
	string word;
	while(ifile>>word) {
		readDefList2(ifile, strToInt(word));

		ifile>>word;
		int numUse = strToInt(word);
		string *useList= new string[numUse];
		bool used[numUse];
		readUseList2(ifile, symTable, numUse, useList, used);

		ifile>>word;
		readInstrList2(ifile, mList, i, symTable, strToInt(word), memIdx, useList, used,numUse);

		delete[] useList;
		i++;
	}
	cout<<endl;
	printWarning(symTable);
}


int main(int argc, char** argv)
{
	if(argc<2)  { cerr<<"Enter your input file."<<endl; return 1; }
	ifstream ifile(argv[1]);
	if(ifile.fail()) { cerr<<"Unable to open file "<<argv[1]<<endl; return 1; }

	// create empty module list and symbol table
	vector<module*> mList;
	map<string, symbol*> symTable;
	
	//First Pass
	readfile1(ifile, mList, symTable);
	printSymTable(mList, symTable);
	cout<<endl;

	// Second Pass
	ifile.clear();
	ifile.seekg(0);
	cout<<"Memory Map"<<endl;
	readfile2(ifile, mList, symTable);

	cleanMem(mList, symTable);

	ifile.close();
	return 0;
}