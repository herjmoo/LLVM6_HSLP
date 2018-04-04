/*
 * ICVectorizer.h
 *
 *  Created on: Feb 21, 2017
 *      Author: Joonmoo Huh
 */

#ifndef TOOLS_AUTOVEC_ICVECTORIZER_H_
#define TOOLS_AUTOVEC_ICVECTORIZER_H_

#include "ICVectorizer_lib.h"

using namespace std;

namespace llvm {

class ICVectorizer {
private:
	map<unsigned, vector<Insts *> > groupsD;
	vector<Insts *> scheduleD;
	vector<PackedI_ICvec *> packedInst;

public:
	ICVectorizer(){}

	bool runOnBasickBlock(BasicBlock *, AliasAnalysis *);

	bool CSE(BasicBlock &, AliasAnalysis &);
	bool Grouping(BasicBlock &, AliasAnalysis &);
	void Scheduling();
	bool Packing(BasicBlock &);

	void generateGV(BasicBlock *, AliasAnalysis *);
	vector<pair<Value *, Value *> > *FindSeeds(BasicBlock *, AliasAnalysis *, unsigned int);
	map<unsigned, vector<Insts *> > *getGroupD() {return &groupsD; };

	void updatePredecessor(BasicBlock &, AliasAnalysis &);
	void updatePredecessorLoadStore(BasicBlock &, AliasAnalysis &);
	void updatePredecessorFunctionCall(BasicBlock &, AliasAnalysis &);

	void dumpGV(string);
};

}

#endif /* TOOLS_AUTOVEC_ICVECTORIZER_H_ */
