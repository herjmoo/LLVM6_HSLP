/*
 * ICVectorizer.cpp
 *
 *  Created on: Feb 21, 2017
 *      Author: Joonmoo Huh
 */

#include "llvm/Transforms/Vectorize/ICVectorizer.h"

using namespace llvm;

vector<pair<Value *, Value *> > *ICVectorizer::FindSeeds(BasicBlock *BB, AliasAnalysis *AA, unsigned int config) {
	vector<pair<Value *, Value *> > *seeds = new vector<pair<Value *, Value *> > ;

	bool loopVected = false;

	errs() << "\n< Original statements >\n";
	for (BasicBlock::iterator I = BB->begin(); I!=BB->end(); I++) {
		//I->dump();
		I->print(errs()); errs() << "\n";
		if (I->getType()->isVectorTy())
			loopVected = true;
	}
	errs() << "\n";

	if (loopVected) {
		errs() << "\n< Already loop-vectorized >\n";
		errs() << "[ Final result ] Give up\n\n";
		return seeds;
	}

	if (CSE(*BB, *AA)) {
		errs() << "[ CSE ] Change\n";
		errs() << "< New statements (After CSE)(" << BB->size() << " instructions)\n";
		for (BasicBlock::iterator I = BB->begin(); I!=BB->end(); I++) {
			//I->dump();
			I->print(errs()); errs() << "\n";
		}
	}
	else
		errs() << "[ CSE ] No change\n";
	errs() << "\n";

	unsigned int serial = 0;
	for (BasicBlock::iterator I = BB->begin(); I != BB->end(); I++)
		if (!I->isTerminator()) {
			if (isa<CallInst>(*I)) {
				CallInst *inst = dyn_cast<CallInst>(&(*I));
				if (inst->getNumArgOperands() == 3) {
					if (!inst->getArgOperand(0)->getType()->isMetadataTy()) {
						Insts *newS = new Insts(serial++, &(*I));
						groupsD[I->getOpcode()].push_back(newS);
					}
				}
				else {
					Insts *newS = new Insts(serial++, &(*I));
					groupsD[I->getOpcode()].push_back(newS);
				}
			}
			else {
				Insts *newS = new Insts(serial++, &(*I));
				groupsD[I->getOpcode()].push_back(newS);
			}
		}

	unsigned int possible_target = 0;
	errs() << " Kinds of instructions: ";
	for (map<unsigned int, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		errs() << I->second.front()->getInstByIndex(0)->getOpcodeName(I->first) << " (" << I->second.size() << ") ";
		if (I->second.size() >= 2)
			possible_target++;
	}
	errs() << "\n";

	if (possible_target < 1) {
		errs() << "\n< No possible target for avx2>\n";
		return seeds;
	}

	updatePredecessor(*BB, *AA);

	CandidatePairs candidates;
	for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		if (isa<LoadInst>(I->second[0]->getInstByIndex(0)) || isa<StoreInst>(I->second[0]->getInstByIndex(0)) || I->second[0]->getInstByIndex(0)->isBinaryOp())
			for (unsigned int i = 0; i < I->second.size()-1; i++) {
				for (unsigned int j = i+1; j < I->second.size(); j++) {
					if (I->second[i]->getType() == I->second[j]->getType()) {
						map<Insts *, bool> v1, v2;
						if (!I->second[i]->isDependentChain(I->second[j], &v1) && !I->second[j]->isDependentChain(I->second[i], &v2)) {
							AddrGEP *S1addr, *S2addr;
							if (isa<LoadInst>(I->second[0]->getInstByIndex(0))) {
								S1addr = new AddrGEP(dyn_cast<LoadInst>(I->second[i]->getInstByIndex(0)));
								S2addr = new AddrGEP(dyn_cast<LoadInst>(I->second[j]->getInstByIndex(0)));
							}
							else if (isa<StoreInst>(I->second[0]->getInstByIndex(0))) {
								S1addr = new AddrGEP(dyn_cast<StoreInst>(I->second[i]->getInstByIndex(0)));
								S2addr = new AddrGEP(dyn_cast<StoreInst>(I->second[j]->getInstByIndex(0)));
							}
							if (/*BB->getParent()->getName()!="binvcrhs" && */(isa<LoadInst>(I->second[0]->getInstByIndex(0)) || isa<StoreInst>(I->second[0]->getInstByIndex(0)))) {
								if (S1addr->isAdjacentAddr(S2addr, false)) {
								//if (isConsecutiveAccess(I->second[i]->getInstByIndex(0), I->second[j]->getInstByIndex(0), *DL, *SE)) {
									unsigned int expect_size = I->second[i]->getNumInsts()+I->second[j]->getNumInsts();
									if (expect_size <= I->second[i]->getMaxSize()) {
										candidates.addCandidate(I->second[i], I->second[j]);
										//flag = true;
									}
								}
							}
							else {
								unsigned int expect_size = I->second[i]->getNumInsts()+I->second[j]->getNumInsts();
								if (expect_size <= I->second[i]->getMaxSize()) {
									candidates.addCandidate(I->second[i], I->second[j]);
									//flag = true;
								}
							}
						}
						v1.clear();
						v2.clear();
					}
				}
			}
	}
	candidates.findPatternCandidate();
	//candidates.dumpCandidates();
	candidates.findSmallIsomorphicChain();
	//candidates.dumpSmallICs();
	candidates.findGlobalIsomorphicChain();
	//candidates.dumpGlobalICs();
	//if (BB->getParent()->getName()=="binvcrhs"/* || BB->getName()=="for.body1631" || BB->getName()=="for.body3131" || BB->getName()=="for.body210"*/)
	//	candidates.selectIsomorphicChain(1);
	//else /*if (BB->getName()!="for.body210")*/
		candidates.selectIsomorphicChain(1);
	candidates.dumpSummary();


	if (config == 0) {
		for (unsigned int i = 0 ; i < candidates.getNumSelectedSeeds(); i++)
			seeds->push_back(make_pair(candidates.getSelectedSeedsbyIdx(i)->getS1()->getInstByIndex(0), candidates.getSelectedSeedsbyIdx(i)->getS2()->getInstByIndex(0)));
	}
	else if (config == 1) {
		for (unsigned int i = 0; i < candidates.getNumSmallIsomorphicChain(); i++) {
			if (0 < candidates.getSmallIsomorphicChianbyIdx(i)->getLevel().min) {
				seeds->push_back(make_pair(candidates.getSmallIsomorphicChianbyIdx(i)->getS1()->getInstByIndex(0), candidates.getSmallIsomorphicChianbyIdx(i)->getS2()->getInstByIndex(0)));
			}
		}
	}
	else if (config == 2) {
		for (unsigned int i = 0; i < candidates.getNumSmallIsomorphicChain(); i++) {
			if (0 < candidates.getSmallIsomorphicChianbyIdx(i)->getLevel().max) {
				seeds->push_back(make_pair(candidates.getSmallIsomorphicChianbyIdx(i)->getS1()->getInstByIndex(0), candidates.getSmallIsomorphicChianbyIdx(i)->getS2()->getInstByIndex(0)));
			}
		}
	}
	else if (config == 3) {
		for (unsigned int i = 0; i < candidates.getNumSmallIsomorphicChain(); i++) {
			if (1 < candidates.getSmallIsomorphicChianbyIdx(i)->getLevel().min) {
				seeds->push_back(make_pair(candidates.getSmallIsomorphicChianbyIdx(i)->getS1()->getInstByIndex(0), candidates.getSmallIsomorphicChianbyIdx(i)->getS2()->getInstByIndex(0)));
			}
		}
	}
	else if (config == 4) {
		for (unsigned int i = 0; i < candidates.getNumSmallIsomorphicChain(); i++) {
			if (1 < candidates.getSmallIsomorphicChianbyIdx(i)->getLevel().max) {
				seeds->push_back(make_pair(candidates.getSmallIsomorphicChianbyIdx(i)->getS1()->getInstByIndex(0), candidates.getSmallIsomorphicChianbyIdx(i)->getS2()->getInstByIndex(0)));
			}
		}
	}

	return seeds;
}

void ICVectorizer::generateGV(BasicBlock *BB, AliasAnalysis *AA) {
	bool loopVected = false;

	errs() << "\n< Original statements >\n";
	for (BasicBlock::iterator I = BB->begin(); I!=BB->end(); I++) {
		//I->dump();
		I->print(errs());
		if (I->getType()->isVectorTy())
			loopVected = true;
	}
	errs() << "\n";

	if (loopVected) {
		errs() << "\n< Already loop-vectorized >\n";
		errs() << "[ Final result ] Give up\n\n";
		return;
	}

	if (CSE(*BB, *AA)) {
		errs() << "[ CSE ] Change\n";
		errs() << "< New statements (After CSE)(" << BB->size() << " instructions)\n";
		for (BasicBlock::iterator I = BB->begin(); I!=BB->end(); I++) {
			//I->dump();
			I->print(errs());
		}
	}
	else
		errs() << "[ CSE ] No change\n";
	errs() << "\n";

	{
		unsigned int serial = 0;
		for (BasicBlock::iterator I = BB->begin(); I != BB->end(); I++)
			if (!I->isTerminator()) {
				Insts *newS = new Insts(serial++, &(*I));
				groupsD[I->getOpcode()].push_back(newS);
			}

		unsigned int possible_target = 0;
		errs() << " Kinds of instructions: ";
		for (map<unsigned int, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
			errs() << I->second.front()->getInstByIndex(0)->getOpcodeName(I->first) << " (" << I->second.size() << ") ";
			if (I->second.size() >= 2)
				possible_target++;
		}
		errs() << "\n";

		if (possible_target < 1) {
			errs() << "\n< No possible target for avx2>\n";
			return;
		}

		if (1) {
			updatePredecessor(*BB, *AA);

			string funcName(BB->getParent()->getName());
			string BBName(BB->getName());
			dumpGV(funcName + "_" + BBName + ".gv");
		}
	}

	for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		for (unsigned int i = 0; i < I->second.size(); i++)
			delete I->second[i];
		I->second.clear();
	}
	groupsD.clear();
}

bool ICVectorizer::runOnBasickBlock(BasicBlock *BB, AliasAnalysis *AA) {
	bool vectorized = false;
	bool loopVected = false;

	errs() << "[[ SLP ]] Basic block: ";
	errs() << BB->getName() << " (" << BB->size() << " instructions)\n";

	errs() << "\n< Original statements >\n";
	for (BasicBlock::iterator I = BB->begin(); I!=BB->end(); I++) {
		//I->dump();
		I->print(errs());
		if (I->getType()->isVectorTy())
			loopVected = true;
	}
	errs() << "\n";

	if (loopVected) {
		errs() << "\n< Already loop-vectorized >\n";
		errs() << "[ Final result ] Give up\n\n";
		return false;
	}

	if (CSE(*BB, *AA)) {
		errs() << "[ CSE ] Change\n";
		errs() << "< New statements (After CSE)(" << BB->size() << " instructions)\n";
		for (BasicBlock::iterator I = BB->begin(); I!=BB->end(); I++) {
			//I->dump();
			I->print(errs());
		}
	}
	else
		errs() << "[ CSE ] No change\n";
	errs() << "\n";

	if (Grouping(*BB, *AA)) {
		/*errs() << "\n< Set of decided statement groups >\n";
		unsigned int iter = 0;
		for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
			for (unsigned int i = 0; i < I->second.size(); i++) {
				errs() << "group " << iter << " ";
				I->second[i]->dump();
				iter++;
			}
		}*/

		errs() << "[ Scheduling ]\n";
		Scheduling();
		errs() << "< Ordered statement groups >\n";
		for (unsigned int i = 0; i < scheduleD.size(); i++) {
			errs() << "statement group " << i << " ";
			scheduleD[i]->dump();
		}
		errs() << "\n";

		errs() << "[ Packing ]\n";
		if (!Packing(*BB)) {
			for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
				for (unsigned int i = 0; i < I->second.size(); i++) {
					delete I->second[i];
				}
				I->second.clear();
			}
			groupsD.clear();
			scheduleD.clear();
			for (unsigned int i = 0; i < packedInst.size(); i++)
				delete packedInst[i];
			packedInst.clear();
			errs() << "[ Final result ] Give up\n\n";
			return false;
		}

		errs() << "< Packed statement groups >\n";
		for (unsigned int i = 0; i < packedInst.size(); i++) {
			errs() << "packed group " << i << "\n";
			packedInst[i]->dump();
		}
		errs() << "\n";

		errs() << "[ Final result ] Success\n";
		vectorized = true;
		errs() << "< Final statements >\n";
		for (BasicBlock::iterator I = BB->begin(); I != BB->end(); I++) {
			//I->dump();
			I->print(errs());
		}
		errs() << "\n";

		scheduleD.clear();
		for (unsigned int i = 0; i < packedInst.size(); i++)
			delete packedInst[i];
		packedInst.clear();
	}
	else {
		errs() << "\n< No groups to SLP >\n";
		errs() << "[ Final result ] Give up\n\n";
	}

	for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		for (unsigned int i = 0; i < I->second.size(); i++)
			delete I->second[i];
		I->second.clear();
	}
	groupsD.clear();

	return vectorized;
}

bool ICVectorizer::CSE(BasicBlock &BB, AliasAnalysis &AA) {
	bool isChange = false;
	vector<LoadInst *> loads;
	vector<StoreInst *> stores;
	vector<StoreInst *> death_note;

	for (BasicBlock::iterator I = BB.begin(); I != BB.end(); ) {
		bool hasAliasStore = false;
		if (isa<StoreInst>(*I))
			stores.push_back(dyn_cast<StoreInst>(&*I));
		else if (isa<LoadInst>(*I)) {
			for (unsigned int j = 0; j < stores.size(); j++) {
				AliasResult res = AA.alias(MemoryLocation::get(dyn_cast<LoadInst>(&*I)), MemoryLocation::get(stores[stores.size()-j-1]));
				if (res == MustAlias) {
					//errs() << "Remove          ", I->dump();
					//errs() << " must alias with", stores[stores.size()-j-1]->dump();
					if (I->getType() == stores[stores.size()-j-1]->getOperand(0)->getType()) {
						I->replaceAllUsesWith(stores[stores.size()-j-1]->getOperand(0));
						BasicBlock::iterator I_tmp = I++;
						I_tmp->eraseFromParent();
						hasAliasStore = true;
						isChange = true;
						break;
					}
					//else
					//	errs() << " give up because of different type\n";

				}
				else if (res == MayAlias || res == PartialAlias)
					break;
			}
		}
		else if (isa<CallInst>(*I)) {
			CallInst *inst = dyn_cast<CallInst>(I);
			if (inst->getNumArgOperands() == 3)
				if (inst->getArgOperand(0)->getType()->isMetadataTy()) {
					stores.clear();
				}
		}
		if (!hasAliasStore)
			I++;
	}
	stores.clear();

	for (BasicBlock::reverse_iterator I = BB.rbegin(); I != BB.rend(); I++) {
		if (isa<LoadInst>(*I))
			loads.push_back(dyn_cast<LoadInst>(&*I));
		else if (isa<StoreInst>(*I)) {
			bool hasAliasLoad = false;
			for (unsigned int j = 0; j < stores.size(); j++) {
				if (AA.alias(MemoryLocation::get(dyn_cast<StoreInst>(&*I)), MemoryLocation::get(stores[j])) == MustAlias) {
					//errs() << "Target          ", I->dump();
					//errs() << " must alias with", stores[j]->dump();

					for (unsigned int k = 0; k < loads.size(); k++) {
						if (AA.alias(MemoryLocation::get(dyn_cast<StoreInst>(&*I)), MemoryLocation::get(loads[k])) != NoAlias) {
							//errs() << " might alias with ", loads[k]->dump();
							hasAliasLoad = true;
							break;
						}
					}
					if (!hasAliasLoad) {
						//errs() << " no alias with following all loads\n";
						death_note.push_back(dyn_cast<StoreInst>(&*I));
						break;
					}
				}
			}
			if (!hasAliasLoad)
				stores.push_back(dyn_cast<StoreInst>(&*I));
		}
		else if (isa<CallInst>(*I)) {
			CallInst *inst = dyn_cast<CallInst>(&*I);
			if (inst->getNumArgOperands() == 3)
				if (inst->getArgOperand(0)->getType()->isMetadataTy()) {
					loads.clear();
					stores.clear();
				}
		}
	}
	loads.clear();
	stores.clear();

	for (unsigned int i = 0; i < death_note.size(); i++) {
		//errs() << "Remove ", death_note[i]->dump();
		death_note[i]->eraseFromParent();
		isChange = true;
	}
	death_note.clear();

	return isChange;
}

bool ICVectorizer::Grouping(BasicBlock &BB, AliasAnalysis &AA) {
	unsigned int serial = 0;
	for (BasicBlock::iterator I = BB.begin(); I != BB.end(); I++)
		if (!I->isTerminator()) {
			Insts *newS = new Insts(serial++, &(*I));
			groupsD[I->getOpcode()].push_back(newS);
		}

	unsigned int possible_target = 0;
	errs() << " Kinds of instructions: ";
	for (map<unsigned int, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		errs() << I->second.front()->getInstByIndex(0)->getOpcodeName(I->first) << " (" << I->second.size() << ") ";
		if (I->second.size() >= 2)
			possible_target++;
	}
	errs() << "\n";

	if (possible_target < 1) {
		errs() << "\n< No possible target for avx2>\n";
		return false;
	}

	unsigned int iter = 1;
	while (1) {
		errs() << "< " << iter << " rounds >\n";

		updatePredecessor(BB, AA);

		bool flag = false;
		CandidatePairs candidates;
		for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
			if (isa<LoadInst>(I->second[0]->getInstByIndex(0)) || isa<StoreInst>(I->second[0]->getInstByIndex(0)) || I->second[0]->getInstByIndex(0)->isBinaryOp())
				for (unsigned int i = 0; i < I->second.size()-1; i++) {
					for (unsigned int j = i+1; j < I->second.size(); j++) {
						if (I->second[i]->getType() == I->second[j]->getType()) {
							map<Insts *, bool> v1, v2;
							if (!I->second[i]->isDependentChain(I->second[j], &v1) && !I->second[j]->isDependentChain(I->second[i], &v2)) {
								AddrGEP *S1addr, *S2addr;
								if (isa<LoadInst>(I->second[0]->getInstByIndex(0))) {
									S1addr = new AddrGEP(dyn_cast<LoadInst>(I->second[i]->getInstByIndex(0)));
									S2addr = new AddrGEP(dyn_cast<LoadInst>(I->second[j]->getInstByIndex(0)));
								}
								else if (isa<StoreInst>(I->second[0]->getInstByIndex(0))) {
									S1addr = new AddrGEP(dyn_cast<StoreInst>(I->second[i]->getInstByIndex(0)));
									S2addr = new AddrGEP(dyn_cast<StoreInst>(I->second[j]->getInstByIndex(0)));
								}
								if (BB.getParent()->getName()!="binvcrhs" && (isa<LoadInst>(I->second[0]->getInstByIndex(0)) || isa<StoreInst>(I->second[0]->getInstByIndex(0)))) {
									bool debug = false;
									if (isa<LoadInst>(I->second[0]->getInstByIndex(0))) {
										//errs() << "DEBUG LOAD\n";
										//I->second[i]->getInstByIndex(0)->dump();
										//I->second[j]->getInstByIndex(0)->dump();
										//LoadInst *ld1 = dyn_cast<LoadInst>(I->second[i]->getInstByIndex(0));
										//LoadInst *ld2 = dyn_cast<LoadInst>(I->second[j]->getInstByIndex(0));
										//if (ld1->getOperand(0)->getName()=="arrayidx3134" && ld2->getOperand(0)->getName()=="arrayidx3664") {
										//	errs() << "DEBUG TARGET LOAD\n";
										//	ld1->getOperand(0)->dump();
										//	ld2->getOperand(0)->dump();
										//	debug = true;
										//}
									}
									if (S1addr->isAdjacentAddr(S2addr, debug)) {
									//if (isConsecutiveAccess(I->second[i]->getInstByIndex(0), I->second[j]->getInstByIndex(0), *DL, *SE)) {
										unsigned int expect_size = I->second[i]->getNumInsts()+I->second[j]->getNumInsts();
										if (expect_size <= I->second[i]->getMaxSize()) {
											candidates.addCandidate(I->second[i], I->second[j]);
											flag = true;
										}
										//errs() << "Adjacent\n";
									}
								}
								else {
									unsigned int expect_size = I->second[i]->getNumInsts()+I->second[j]->getNumInsts();
									if (expect_size <= I->second[i]->getMaxSize()) {
										candidates.addCandidate(I->second[i], I->second[j]);
										flag = true;
									}
								}
							}
							v1.clear();
							v2.clear();
						}
					}
				}
		}
		candidates.findPatternCandidate();
		//candidates.dumpCandidates();
		candidates.findSmallIsomorphicChain();
		//candidates.dumpSmallICs();
		candidates.findGlobalIsomorphicChain();
		//candidates.dumpGlobalICs();
		//if (BB.getParent()->getName()=="binvcrhs")
		//	candidates.selectIsomorphicChain(1);
		//else
			candidates.selectIsomorphicChain(1);
		candidates.dumpSummary();

		if (flag) {
			string funcName(BB.getParent()->getName());
			string BBName(BB.getName());
			dumpGV(funcName + "_" + BBName + ".gv");
		}

		for (unsigned int i = 0 ; i < candidates.getNumSelecteds(); i++) {
			candidates.getSelectedsbyIdx(i)->getS1()->merge(candidates.getSelectedsbyIdx(i)->getS2());

			vector<Insts *>::iterator I;
			for (I = groupsD[candidates.getSelectedsbyIdx(i)->getS1()->getInstByIndex(0)->getOpcode()].begin(); I != groupsD[candidates.getSelectedsbyIdx(i)->getS1()->getInstByIndex(0)->getOpcode()].end(); I++) {
				if (candidates.getSelectedsbyIdx(i)->getS2() == *I) {
					break;
				}
			}
			if (I == groupsD[candidates.getSelectedsbyIdx(i)->getS1()->getInstByIndex(0)->getOpcode()].end()) {
				errs() << "ERROR: cannot be end of iteration\n";
				candidates.getSelectedsbyIdx(i)->getS1()->dump();
				candidates.getSelectedsbyIdx(i)->getS2()->dump();
			}
			groupsD[candidates.getSelectedsbyIdx(i)->getS1()->getInstByIndex(0)->getOpcode()].erase(I);
		}


		updatePredecessor(BB, AA);

		if (!candidates.getNumSelecteds()) {
			if (iter == 1) {
				errs() << "\n< No groups from first iter >\n";
				return false;
			}
			else
				break;
		}

		iter++;
		break;
	}

	return true;
}

void ICVectorizer::updatePredecessor(BasicBlock &BB, AliasAnalysis &AA) {
	map <Instruction *, Insts *> mapping;

	for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		for (unsigned int i = 0; i < I->second.size(); i++) {
			I->second[i]->clearPred();
			I->second[i]->clearSucc();
			I->second[i]->clearDepend();
			for (unsigned int j = 0; j < I->second[i]->getNumInsts(); j++) {
				mapping[I->second[i]->getInstByIndex(j)] = I->second[i];
			}
		}
	}
	for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		if (!isa<PHINode>(I->second[0]->getInstByIndex(0)))
			for (unsigned int i = 0; i < I->second.size(); i++) {
				for (unsigned int k = 0; k < I->second[i]->getNumInsts(); k++) {
					Insts *operands[2];
					unsigned int operandsSize = 0;
					for (unsigned int j = 0; j < I->second[i]->getInstByIndex(k)->getNumOperands(); j++)
						if (isa<Instruction>(I->second[i]->getInstByIndex(k)->getOperand(j))) {
							Instruction *operand = dyn_cast<Instruction>(I->second[i]->getInstByIndex(k)->getOperand(j));
							if (operand->getParent() == I->second[i]->getInstByIndex(k)->getParent()) {
								if (mapping.find(operand) == mapping.end()) {
									errs() << "ERROR: should be find in the mapping table\n";
									//I->second[i]->getInstByIndex(k)->dump();
									I->second[i]->getInstByIndex(k)->print(errs());
									//operand->dump();
									operand->print(errs());
									errs() << "ERROR end\n";
								}
								if ((isa<StoreInst>(I->second[i]->getInstByIndex(0)) && j!=0) || isa<LoadInst>(I->second[i]->getInstByIndex(0)) || isa<GetElementPtrInst>(I->second[i]->getInstByIndex(0)) || isa<CallInst>(I->second[i]->getInstByIndex(0)))
									I->second[i]->addDepend(mapping[operand]);
								else {
									//if (j==0)
									//	I->second[i]->addOp1Pred(mapping[operand]);
									//else if (j==1)
									//	I->second[i]->addOp2Pred(mapping[operand]);
									if (j==0 || j==1) {
										if (operandsSize)
											operands[1] = mapping[operand];
										else
											operands[0] = mapping[operand];
										operandsSize++;
									}
									else {
										errs() << "ERROR: should be either first or second operand (" << j << ")";
										//I->second[i]->getInstByIndex(k)->dump();
										I->second[i]->getInstByIndex(k)->print(errs());
										errs() << "\n";
									}

									mapping[operand]->addSucc(I->second[i]);
								}
							}
						}
					if (operandsSize == 1) {
						I->second[i]->addOp1Pred(operands[0]);
					}
					else if (operandsSize == 2) {
						unsigned Opcode = I->second[i]->getInstByIndex(k)->getOpcode();
						if (Opcode == Instruction::Add || Opcode == Instruction::FAdd || Opcode == Instruction::Mul || Opcode == Instruction::FMul) {
							if (operands[0]->getInstByIndex(0)->getOpcode() < operands[1]->getInstByIndex(0)->getOpcode()) {
								I->second[i]->addOp1Pred(operands[0]);
								I->second[i]->addOp2Pred(operands[1]);
							}
							else {
								I->second[i]->addOp1Pred(operands[1]);
								I->second[i]->addOp2Pred(operands[0]);
							}
						}
						else {
							I->second[i]->addOp1Pred(operands[1]);
							I->second[i]->addOp2Pred(operands[0]);
						}
					}
				}
			}
	}
	mapping.clear();

	updatePredecessorFunctionCall(BB, AA);
	updatePredecessorLoadStore(BB, AA);
}

void ICVectorizer::updatePredecessorFunctionCall(BasicBlock &BB, AliasAnalysis &AA) {
	map<Instruction *, unsigned int> insts;
	unsigned int serial = 0;

	for (BasicBlock::iterator I = BB.begin(); I != BB.end(); I++)
		insts[&*I] = serial++;

	if (groupsD.find(54) != groupsD.end())
		for (unsigned int i = 0; i < groupsD[54].size(); i++) { // opcode 54 = call inst. (include/IR/llvm/IR/Instruction.def)
			unsigned int call_serial = 0;
			if (insts.find(groupsD[54][i]->getInstByIndex(0)) == insts.end())
				//errs() << "ERROR: should be find in the insts table (call)", groupsD[54][i]->getInstByIndex(0)->dump(), errs() << "ERROR END\n";
				errs() << "ERROR: should be find in the insts table (call)", groupsD[54][i]->getInstByIndex(0)->print(errs()), errs() << "ERROR END\n";
			else
				call_serial = insts[groupsD[54][i]->getInstByIndex(0)];

			for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
				if (!isa<CallInst>(I->second[0]->getInstByIndex(0)))
					for (unsigned int j = 0; j < I->second.size(); j++) {
						unsigned int current_serial = 0;
						if (insts.find(I->second[j]->getInstByIndex(0)) == insts.end())
							errs() << "ERROR: should be find in the insts table\n", I->second[j]->dump(), errs() << "ERROR END\n";
						else
							current_serial = insts[I->second[j]->getInstByIndex(0)];
						if (current_serial < call_serial) {
							groupsD[54][i]->addDepend(I->second[j]);
						}
						else if (call_serial < current_serial) {
							I->second[j]->addDepend(groupsD[54][i]);
						}
						else
							errs() << "ERROR: could not be the same serial";
					}
			}
		}
	insts.clear();
}

void ICVectorizer::updatePredecessorLoadStore(BasicBlock &BB, AliasAnalysis &AA) {
	map<Instruction *, unsigned int> loads_stores;
	unsigned int serial = 0;
	unsigned int op_code_load = 0;
	unsigned int op_code_store = 0;


	for (BasicBlock::iterator I = BB.begin(); I != BB.end(); I++) {
		if (isa<LoadInst>(*I)) {
			loads_stores[&*I] = serial++;
			op_code_load = I->getOpcode();
		}
		if (isa<StoreInst>(*I)) {
			loads_stores[&*I] = serial++;
			op_code_store = I->getOpcode();
		}
	}

	if (op_code_store) {
		for (unsigned int i = 0; i < groupsD[op_code_store].size(); i++) {
			for (unsigned int j = 0; j < groupsD[op_code_store].size(); j++) {
				if (i != j) {
					bool mightAlias = false;
					bool isGreater = false;
					bool isLess = false;
					for (unsigned int k = 0; k < groupsD[op_code_store][i]->getNumInsts(); k++) {
						for (unsigned int l = 0; l < groupsD[op_code_store][j]->getNumInsts(); l++) {
							if (AA.alias(MemoryLocation::get(dyn_cast<StoreInst>(groupsD[op_code_store][i]->getInstByIndex(k))), MemoryLocation::get(dyn_cast<StoreInst>(groupsD[op_code_store][j]->getInstByIndex(l)))) != NoAlias) {
								mightAlias = true;
								if (loads_stores[groupsD[op_code_store][i]->getInstByIndex(k)] < loads_stores[groupsD[op_code_store][j]->getInstByIndex(l)]) {
									isLess = true;
								}
								else if (loads_stores[groupsD[op_code_store][i]->getInstByIndex(k)] > loads_stores[groupsD[op_code_store][j]->getInstByIndex(l)]) {
									isGreater = true;
								}
								else
									errs() << "ERROR: could not be the same instruction\n";
							}
						}
					}
					if (mightAlias) {
						if (isGreater && isLess)
							errs() << "ERROR: could not be dependent on each other\n";
						else if (isGreater) {
							groupsD[op_code_store][i]->addDepend(groupsD[op_code_store][j]);
						}
					}
				}
			}
		}
	}

	if (op_code_load && op_code_store) {
		for (unsigned int i = 0; i < groupsD[op_code_load].size(); i++) {
			for (unsigned int j = 0; j < groupsD[op_code_store].size(); j++) {
				bool mightAlias = false;
				bool isGreater = false;
				bool isLess = false;
				for (unsigned int k = 0; k < groupsD[op_code_load][i]->getNumInsts(); k++) {
					for (unsigned int l = 0; l < groupsD[op_code_store][j]->getNumInsts(); l++) {
						if (AA.alias(MemoryLocation::get(dyn_cast<LoadInst>(groupsD[op_code_load][i]->getInstByIndex(k))), MemoryLocation::get(dyn_cast<StoreInst>(groupsD[op_code_store][j]->getInstByIndex(l)))) != NoAlias) {
							mightAlias = true;
							if (loads_stores[groupsD[op_code_load][i]->getInstByIndex(k)] < loads_stores[groupsD[op_code_store][j]->getInstByIndex(l)]) {
								isLess = true;
							}
							else if (loads_stores[groupsD[op_code_load][i]->getInstByIndex(k)] > loads_stores[groupsD[op_code_store][j]->getInstByIndex(l)]) {
								isGreater = true;
							}
							else
								errs() << "ERROR: could not be the same instruction\n";
						}
					}
				}
				if (mightAlias) {
					if (isGreater && isLess)
						errs() << "ERROR: could not be dependent on each other\n";
					else if (isGreater) {
						groupsD[op_code_load][i]->addDepend(groupsD[op_code_store][j]);
					}
				}
			}
		}
	}

	loads_stores.clear();
}

void ICVectorizer::Scheduling() {
	DG_ICvec dg;
	for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
		for (unsigned int i = 0; i < I->second.size(); i++) {
			//errs() << "DEBUG Scheduing: ";
			//I->second[i]->dumpSeiral();
			//errs() << "\n";
			if (isa<PHINode>(I->second[i]->getInstByIndex(0)))
				scheduleD.push_back(I->second[i]);
			else
				dg.addNode(I->second[i]);
		}
	}
	dg.updateEdges();

	vector<Insts *> rd;
	vector<Insts *> ld;
	LP_ICvec lp;
	dg.getReadyGroups(&rd);

	while (rd.size()) {
		int max_count = -1;
		unsigned int max_i = 0;
		for (unsigned int i = 0; i < rd.size(); i++) {
			unsigned int reuses = lp.getNumberOfReuses(rd[i]);
			if (max_count < (int) reuses) {
				bool find = false;
				for (unsigned int j = 0; j < ld.size(); j++)
					if (ld[j] == rd[i])
						find = true;

				if (!find) {
					max_count = (int) reuses;
					max_i = i;
				}
			}
		}
		if (max_count == -1) {
			if (ld.size()) {
				for (unsigned int i = 0; i < ld.size(); i++) {
					lp.addNode(ld[i]);
					scheduleD.push_back(ld[i]);
					dg.eraseNode(ld[i]);
				}
				ld.clear();
			}
			else
				errs() << "ERROR: max_count should not be -1\n";
		}
		else {
			if (isa<LoadInst>(rd[max_i]->getInstByIndex(0)) && rd[max_i]->getNumInsts()!= 1) {
				ld.push_back(rd[max_i]);
			}
			else {
				lp.addNode(rd[max_i]);
				scheduleD.push_back(rd[max_i]);
				dg.eraseNode(rd[max_i]);
			}
		}

		dg.updateEdges();

		rd.clear();
		dg.getReadyGroups(&rd);

		//lp.dump();
	}

	if (dg.getNumNodes()) {
		errs() << "ERROR: should have no remain node\n";
		dg.dump();
	}
}

bool ICVectorizer::Packing(BasicBlock &BB) {
	PackingS_ICvec packing(&BB);
	Instruction *terminator = BB.getTerminator();

	//for (unsigned int i = 0; i < scheduleD.size(); i++) {
	//	if (/*LDopt*/false && scheduleD[i]->getNumInsts() != 1 && isa<LoadInst>(scheduleD[i]->getInstByIndex(0))) {
	/*		vector<Insts *> ld;

			for (unsigned int j = i; j < scheduleD.size(); j++) {
				if (isa<LoadInst>(scheduleD[j]->getInstByIndex(0)) && scheduleD[j]->getNumInsts() != 1)
					ld.push_back(scheduleD[j]);
				else
					break;
			}

			if (!packing.isPackableLoadInstsGroups(ld)) {
				ld.clear();
				errs() << "\n< No support load group >\n";
				return false;
			}

			i += ld.size()-1;

			ld.clear();
		}
		if (!packing.hasOutsideUse(scheduleD[i])) {
			errs() << "\n< No support group >\n";
			return false;
		}
	}*/

	for (unsigned int i = 0; i < scheduleD.size(); i++) {
		Insts *target_Insts = scheduleD[i];
		//errs() << "DEBUG Packing:\n", target_Insts->dump();

		if (target_Insts->getNumInsts() == 1) {
			packing.packSingleInst(target_Insts, i, &packedInst);
		}
		else if (isa<LoadInst>(target_Insts->getInstByIndex(0))) {
			//if (/*LDopt*/false) {
			/*	vector<Insts *> ld;

				for (unsigned int j = i; j < scheduleD.size(); j++) {
					if (isa<LoadInst>(scheduleD[j]->getInstByIndex(0)) && scheduleD[j]->getNumInsts() != 1)
						ld.push_back(scheduleD[j]);
					else
						break;
				}

				packing.packLoadInstsGroups(ld, i, &packedInst);

				i += ld.size()-1;

				ld.clear();
			}
			else*/
				packing.packLoadInsts(target_Insts, i, &packedInst);
		}
		else if (isa<StoreInst>(target_Insts->getInstByIndex(0))) {
			packing.packStoreInsts(target_Insts, i, &packedInst);
		}
		else if (target_Insts->getInstByIndex(0)->isBinaryOp()) {
			packing.packBinaryOpInsts(target_Insts, i, &packedInst);
		}
		else
			errs() << "ERROR: The case is not implemented yet (Packing)\n", scheduleD[i]->dump();

	}

	packing.packTerminatorInst(terminator, &packedInst);

	stack<Instruction *> erase_list;

	//errs() << "DEBUG before delete\n";
	//for (BasicBlock::iterator I = BB.begin(); I != BB.end(); I++) {
	//	I->dump();
	//}
	//errs() << "DEBUG end\n";



	for (BasicBlock::iterator I = BB.begin(); I != BB.end(); I++) {
		erase_list.push(&*I);
		if (dyn_cast<Instruction>(I) == terminator)
			break;
	}

	while (!erase_list.empty()) {
		erase_list.top()->eraseFromParent();
		erase_list.pop();
	}

	return true;
}

void ICVectorizer::dumpGV(string fileName) {
	ofstream GVfile((fileName).c_str());

	if (GVfile.is_open()) {
		GVfile << "digraph G {\n";
		for (map<unsigned, vector<Insts *> >::iterator I = groupsD.begin(); I != groupsD.end(); I++) {
			for (unsigned int j = 0; j < I->second.size(); j++) {
				vector<unsigned int> S2_serial = I->second[j]->getSerial();
				string S2;
				for (unsigned int j = 0; j < S2_serial.size(); j++)
					if (j)
						S2 = S2 + "," + to_string(S2_serial[j]);
					else
						S2 = "\"" + to_string(S2_serial[j]);

				if (I->second[j]->getInstByIndex(0)->getOpcode() == 30) // load
					GVfile << "  " << S2 << "\" [color=blue, style=filled];\n";
				else if (I->second[j]->getInstByIndex(0)->getOpcode() == 31) // store
					GVfile << "  " << S2 << "\" [color=green, style=filled];\n";
				else if (I->second[j]->getInstByIndex(0)->getOpcode() == 32) // GEP
					GVfile << "  " << S2 << "\" [color=lightblue, style=filled];\n";
				else if (I->second[j]->getInstByIndex(0)->getOpcode() == 13 || I->second[j]->getInstByIndex(0)->getOpcode() == 14) // fsub
					GVfile << "  " << S2 << "\" [color=purple, style=filled];\n";
				else if (I->second[j]->getInstByIndex(0)->getOpcode() == 15 || I->second[j]->getInstByIndex(0)->getOpcode() == 16) // fmul
					GVfile << "  " << S2 << "\" [color=orange, style=filled];\n";
				else if (I->second[j]->getInstByIndex(0)->getOpcode() == 17 || I->second[j]->getInstByIndex(0)->getOpcode() == 18 || I->second[j]->getInstByIndex(0)->getOpcode() == 19) // fdiv
					GVfile << "  " << S2 << "\" [color=yellow, style=filled];\n";
				else if (I->second[j]->getInstByIndex(0)->getOpcode() == 11 || I->second[j]->getInstByIndex(0)->getOpcode() == 12) // add
					GVfile << "  " << S2 << "\" [color=pink, style=filled];\n";

				for (unsigned int k = 0; k < I->second[j]->getNumOp1Preds(); k++) {
					vector<unsigned int> S1_serial = I->second[j]->getOp1PredByIndex(k)->getSerial();
					string S1;
					for (unsigned int j = 0; j < S1_serial.size(); j++)
						if (j)
							S1 = S1 + "," + to_string(S1_serial[j]);
						else
							S1 = "\"" + to_string(S1_serial[j]);
					GVfile << "  " << S1 << "\" -> " << S2 << "\";\n";
				}
				for (unsigned int k = 0; k < I->second[j]->getNumOp2Preds(); k++) {
					vector<unsigned int> S1_serial = I->second[j]->getOp2PredByIndex(k)->getSerial();
					string S1;
					for (unsigned int j = 0; j < S1_serial.size(); j++)
						if (j)
							S1 = S1 + "," + to_string(S1_serial[j]);
						else
							S1 = "\"" + to_string(S1_serial[j]);
					GVfile << "  " << S1 << "\" -> " << S2 << "\";\n";
				}
			}
		}
		GVfile << "}\n";

		GVfile.close();
	}
	else
		errs() << "ERROR: cannot open output file (" << fileName << ")\n";
}

