/*
 * ICVectorizer_lib.h
 *
 *  Created on: Feb 21, 2017
 *      Author: Joonmoo Huh
 */

#ifndef TOOLS_AUTOVEC_ICVECTORIZER_LIB_H_
#define TOOLS_AUTOVEC_ICVECTORIZER_LIB_H_

#define MAX_BITS_VECTOR_REG 256

#include "llvm/Analysis/LoopAccessAnalysis.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

//#include <iostream>
//#include <string>
//#include <vector>
#include <stack>
//#include <map>
#include <fstream>

using namespace std;
using namespace llvm;

class Insts {
private:
	vector<unsigned int> serial;
	unsigned int max_size;
	Type *type;
	vector<Instruction *> insts;
	vector<Insts *> depends;
	vector<Insts *> op1_preds;
	vector<Insts *> op2_preds;
	vector<Insts *> succs;

public:
	Insts(unsigned int, Instruction *);
	~Insts() { serial.clear(); insts.clear(); op1_preds.clear(); op2_preds.clear(); succs.clear(); };

	void addOp1Pred(Insts *);
	void addOp2Pred(Insts *);
	void clearPred();
	void addSucc(Insts *);
	void clearSucc();
	void addDepend(Insts *);
	void clearDepend();
	bool merge(Insts *);
	bool reorder(vector<unsigned int>);

	vector<unsigned int> getSerial();
	unsigned int getMaxSize();
	Type *getType();
	unsigned int getNumInsts();
	unsigned int getNumOp1Preds();
	unsigned int getNumOp2Preds();
	unsigned int getNumSuccs();
	Instruction *getInstByIndex(unsigned int);
	Insts *getOp1PredByIndex(unsigned int);
	Insts *getOp2PredByIndex(unsigned int);
	Insts *getSuccByIndex(unsigned int);

	bool isDependent(Insts *);
	bool isDependentChain(Insts *, map<Insts *, bool> *visited);

	void dump();
	void dumpSeiral();
};

class AddrGEP {
private:
	Value *base;
	vector<Value *> offsets;

public:
	AddrGEP(LoadInst *);
	AddrGEP(StoreInst *);
	~AddrGEP() { offsets.clear(); };

	Value *getBase();
	unsigned int getNumOffsets();
	Value *getOffsetsByIdx(unsigned int);

	bool isAdjacentAddr(AddrGEP *, bool);
	bool isGreaterAddr(AddrGEP *);

	void dump();
};

class DG_ICvec {
private:
	vector<Insts *> nodes;
	vector<pair<Insts *, Insts *> > edges;

public:
	DG_ICvec() {};
	~DG_ICvec() { nodes.clear(); edges.clear(); };

	void addNode(Insts *);
	void eraseNode(Insts *);
	void updateEdges();

	unsigned int getNumNodes();

	void getReadyGroups(vector<Insts *> *);

	void dump();
};

class LP_node_ICvec {
private:
	vector<Value *> values;

public:
	LP_node_ICvec(Insts *);
	LP_node_ICvec(Insts *, unsigned int);
	~LP_node_ICvec() { values.clear(); };

	unsigned int getNumValues();
	unsigned int getIndexByValue(Value *);
	Value *getValueByIndex(unsigned int);

	bool doesIncludeValue(Value *);
	bool isSameExactly(Insts *);
	bool isIncludedAll(LP_node_ICvec *);
	bool isIncludedPartially(LP_node_ICvec *);

	void dump();
};

class LP_ICvec {
private:
	vector<LP_node_ICvec *> list;

	bool sortPairVector(vector<pair<unsigned int, int> > *);

public:
	LP_ICvec() {};
	~LP_ICvec() { list.clear(); };

	void addNode(Insts *);

	unsigned int getNumberOfReuses(Insts *);
	void dump();
};

class PackedI_ICvec {
private:
	vector<Instruction *> insts;

public:
	PackedI_ICvec() {};
	~PackedI_ICvec() {};

	void addInst(Instruction *);
	void addInst(Instruction *inst, BasicBlock *b);

	Instruction *getLastInst();

	void dump();
};

class PackingS_ICvec {
private:
	BasicBlock *parent;

	map<Instruction *, pair<Instruction *, pair<unsigned int, unsigned int> > > mapping;
	map<Instruction *, bool> proceededSingleInst;
	map<Instruction *, bool> InstNeedtoExtractforPhiNode;
	map<Value *, vector<int > > loadAddr;
	map<Value *, map<int, Value *> > loadAddrTable;
	map<Value *, map<int, LoadInst *> > loadAddrTableLoad;

	void addMappingInst(Instruction *, Instruction *, unsigned int, unsigned int);
	bool isMappingInst(Instruction *);
	Instruction *getMappingNewInst(Instruction *);
	unsigned int getMappingLength(Instruction *);
	unsigned int getMappingPosition(Instruction *);

	void addProceededSingleInst(Instruction *);
	bool isProceededSingleInst(Instruction *);

	void addLoadAddr(Value *, int, Value *, Instruction *);
	bool isIncludedLoadAddr(Value *, int);
	vector<pair<Value *, pair<int, Value *> > > *getLoadVectorAddr(int);
	pair<unsigned int, unsigned int> getLoadVectorLoc(Value *, int, int);

	Constant *getConstantValue(unsigned int);
	Constant *getConstantValue(int);
	UndefValue *getUndefValue(Type *);
	UndefValue *getUndefValue(Type *, unsigned int);
	Constant *getIdxVector(vector<unsigned int>);
	Constant *getIdxVector(vector<int>);

public:
	PackingS_ICvec(BasicBlock *);
	~PackingS_ICvec() {};

	void packSingleInst(Insts *, unsigned int, vector<PackedI_ICvec *> *);
	void packLoadInsts(Insts *, unsigned int, vector<PackedI_ICvec *> *);
	//void packLoadInstsGroups(vector<Insts *>, unsigned int, vector<PackedI_ICvec *> *);
	void packStoreInsts(Insts *, unsigned int, vector<PackedI_ICvec *> *);
	void packBinaryOpInsts(Insts *, unsigned int, vector<PackedI_ICvec *> *);
	void packTerminatorInst(Instruction *, vector<PackedI_ICvec *> *);

	bool isPackableLoadInstsGroups(vector<Insts *>);
	bool hasOutsideUse(Insts *);
};

struct range {
	int min, max;

	struct range& operator+=(struct range target){
		min += target.min;
		max += target.max;
		return *this;
	}
	struct range& operator+(struct range target){
		min += target.min;
		max += target.max;
		return *this;
	}
	struct range& operator-=(struct range target){
		min -= target.min;
		max -= target.max;
		return *this;
	}
};

enum Pattern { P_NONE, Pa, Pb, Pc, Pd, Pe, Pf, Pg };

class CandidatePattern {
private:
	enum Pattern level1_op1;
	enum Pattern level1_op2;
	enum Pattern level2_1_op1;
	enum Pattern level2_1_op2;
	enum Pattern level2_2_op1;
	enum Pattern level2_2_op2;
	int saved;
	struct range costInside, costOutside;

	enum Pattern findOp1Op2Pattern(Insts *, Insts *, unsigned int ,vector<pair<Insts *, Insts *> >, bool);
	struct range convertPackCost(enum Pattern);
	struct range convertUnpackCost(enum Pattern);

public:
	CandidatePattern(Insts *, Insts *, vector<pair<Insts *, Insts *> >);
	~CandidatePattern() {};

	int getSaved();
	struct range getCostInside();
	struct range getCostOutside();

	void dump();
	void dumpPatternOnly();
};

class SmallIsomorphicChain {
private:
	pair<Insts *, Insts *> pairs;
	int saved;
	struct range costInside, costOutside;
	struct range level;

public:
	SmallIsomorphicChain(Insts *, Insts *, int, struct range, struct range);
	~SmallIsomorphicChain() {};

	Insts *getS1();
	Insts *getS2();
	int getSaved();
	struct range getCostInside();
	struct range getCostOutside();
	struct range getLevel();

	void dump();
	void dumpCostOnly();
};

class GlobalIsomorphicChain {
private:
	SmallIsomorphicChain *root;
	map<SmallIsomorphicChain *, unsigned int> smallICs;
	map<Insts *, unsigned int> allInstICs;
	vector<unsigned int> smallIC_maxHeight;
	vector<unsigned int> smallIC_minHeight;

	double averageMaxHeight;
	double averageMinHeight;
	int maxBenefit;
	int minBenefit;
	unsigned int maxHeight;
	unsigned int minHeight;
	unsigned int selectedSmallICs;
	unsigned int conflictSmallICs;
	unsigned int numComleteSmallICs;
	unsigned int numBeneficialSmallICs;
	unsigned int numHarmfulSmallICs;

public:
	GlobalIsomorphicChain(SmallIsomorphicChain *);
	~GlobalIsomorphicChain() {};

	void addSmallIC(SmallIsomorphicChain *, unsigned int);
	void updateProperties(map<SmallIsomorphicChain *, unsigned int>, map<Insts *, bool>);

	bool isExist(SmallIsomorphicChain *);
	bool isConflict(SmallIsomorphicChain *);

	SmallIsomorphicChain *getRoot();
	//map<SmallIsomorphicChain *, unsigned int> getSmallICs();
	map<SmallIsomorphicChain *, unsigned int>::iterator getSmallICsBegin();
	map<SmallIsomorphicChain *, unsigned int>::iterator getSmallICsEnd();
	unsigned int getSmallICsSize();
	unsigned int getSmallIC_maxHeight(unsigned int);
	unsigned int getSmallIC_minHeight(unsigned int);
	double getAverageMaxHeight();
	double getAverageMinHeight();
	int getMaxBenefit();
	int getMinBenefit();
	unsigned int getMaxHeight();
	unsigned int getMinHeight();
	unsigned int getSelectedSmallICs();
	unsigned int getConflictSmallICs();
	unsigned int getNumCompleteSmallICs();
	unsigned int getNumBeneficialSmallICs();
	unsigned int getNumHarmfulSmallICs();

	void dump(map<Insts *, struct range>, map<Insts *, struct range>);
	void dumpAll(map<Insts *, struct range>, map<Insts *, struct range>);
};

class CandidatePairs {
private:
	vector<pair<Insts *, Insts *> > pre_candidates;
	vector<pair<pair<Insts *, Insts *>, CandidatePattern > > candidates;
	vector<SmallIsomorphicChain *> allCandidates;
	map<Insts *, bool> allInstsCandidates;
	map<Insts *, struct range> allInstsDepth;
	map<Insts *, struct range> allInstsHeight;
	map<GlobalIsomorphicChain *, bool> globalICs;

	//vector<SmallIsomorphicChain *> smallICs;
	//vector<SmallIsomorphicChain *> smallICsLevel4;
	//map<SmallIsomorphicChain *, map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> > > globalICs;
	//map<SmallIsomorphicChain *, pair<double, double> > globalICsAverageHeight;
	//map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> > globalICsHeight;
	//map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> > globalICsSelectedICs;
	//map<SmallIsomorphicChain *, pair<int, int> > globalICsBenefit;
	//map<SmallIsomorphicChain *, unsigned int > globalICsNumCSmallICs;
	//map<SmallIsomorphicChain *, unsigned int > globalICsNumBSmallICs;
	//map<SmallIsomorphicChain *, unsigned int > globalICsNumHSmallICs;
	unsigned int selectedGlobalICs;
	map<SmallIsomorphicChain *, unsigned int> selectedICs;
	vector<SmallIsomorphicChain *> finalSelectedICs;
	vector<SmallIsomorphicChain *> finalSelectedSeeds;

	void addHeight(Insts *, map<Insts *, bool> *, int *, int *, int);
	void addDepth(Insts *, map<Insts *, bool> *, int *, int *, int);
	bool isSameAllHeightDepth(SmallIsomorphicChain *);
	bool isEvenAllHeightDepth(SmallIsomorphicChain *);
	void findAllPreds(Insts *, map<Insts *, bool> *, unsigned int, map<Insts *, unsigned int> *);
	void findPredIsomorphicChain(GlobalIsomorphicChain *, SmallIsomorphicChain *, unsigned int, bool);
	//void findGlobalIsomorphicChainAverageHeight();
	//void findGlobalIsomorphicChainSelectedICs();

public:
	CandidatePairs();
	~CandidatePairs() {};

	void addCandidate(Insts *, Insts *);
	void findPatternCandidate();
	void findSmallIsomorphicChain();
	void findGlobalIsomorphicChain();
	void selectIsomorphicChain(unsigned int);

	unsigned int getNumSmallIsomorphicChain();
	SmallIsomorphicChain *getSmallIsomorphicChianbyIdx(unsigned int);
	unsigned int getNumSelecteds();
	SmallIsomorphicChain *getSelectedsbyIdx(unsigned int);
	unsigned int getNumSelectedSeeds();
	SmallIsomorphicChain *getSelectedSeedsbyIdx(unsigned int);

	void dumpCandidates();
	void dumpSmallICs();
	void dumpGlobalICs();
	void dumpSummary();
};

#endif /* TOOLS_AUTOVEC_ICVECTORIZER_LIB_H_ */
