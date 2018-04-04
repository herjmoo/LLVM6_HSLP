/*
 * ICVectorizer_lib.cpp
 *
 *  Created on: Feb 21, 2017
 *      Author: Joonmoo Huh
 */

#include "llvm/Transforms/Vectorize/ICVectorizer_lib.h"

using namespace llvm;

Insts::Insts(unsigned int argSerial, Instruction *argInst) {
	Type *t;

	serial.push_back(argSerial);
	if (isa<StoreInst>(argInst))
		t = argInst->getOperand(0)->getType();
	else
		t = argInst->getType();

	if (t->isIntegerTy(1))
		max_size = 1;
	else if (t->isIntegerTy(8))
		max_size = MAX_BITS_VECTOR_REG / 8;
	else if (t->isIntegerTy(16))
		max_size = MAX_BITS_VECTOR_REG / 16;
	else if (t->isIntegerTy(32))
		max_size = MAX_BITS_VECTOR_REG / 32;
	else if (t->isIntegerTy(64))
		max_size = MAX_BITS_VECTOR_REG / 64;
	else if (t->isIntegerTy(128))
		max_size = MAX_BITS_VECTOR_REG / 128;
	else if (t->isHalfTy())
		max_size = MAX_BITS_VECTOR_REG / 16;
	else if (t->isFloatTy())
		max_size = MAX_BITS_VECTOR_REG / 32;
	else if (t->isDoubleTy())
		max_size = MAX_BITS_VECTOR_REG / 64;
	else if (t->isFP128Ty())
		max_size = MAX_BITS_VECTOR_REG / 128;
	else if (t->isPPC_FP128Ty())
		max_size = MAX_BITS_VECTOR_REG / 128;
	else if (isa<GetElementPtrInst>(argInst))
		max_size = 1;
	else if (isa<CallInst>(argInst))
		max_size = 1;
	else {
		max_size = 1;
	}

	type = t;
	insts.push_back(argInst);
}

void Insts::addOp1Pred(Insts *pred) {
	for (unsigned int i = 0; i < op1_preds.size(); i++)
		if (op1_preds[i] == pred)
			return;

	op1_preds.push_back(pred);
}

void Insts::addOp2Pred(Insts *pred) {
	for (unsigned int i = 0; i < op2_preds.size(); i++)
		if (op2_preds[i] == pred)
			return;

	op2_preds.push_back(pred);
}

void Insts::clearPred() {
	op1_preds.clear();
	op2_preds.clear();
}

void Insts::addSucc(Insts *succ) {
	for (unsigned int i = 0; i < succs.size(); i++)
		if (succs[i] == succ)
			return;

	succs.push_back(succ);
}

void Insts::clearSucc() {
	succs.clear();
}

void Insts::addDepend(Insts *depend) {
	for (unsigned int i = 0; i < depends.size(); i++)
		if (depends[i] == depend)
			return;

	depends.push_back(depend);
}

void Insts::clearDepend() {
	depends.clear();
}

bool Insts::merge(Insts *d) {
	if (max_size<insts.size() + d->getNumInsts()) {
		errs() << "ERROR: Exceed maximum vector size (" << insts.size() << " + " << d->getNumInsts() << ")\n";
		return false;
	}

	for (unsigned int i = 0; i < d->getNumInsts(); i++) {
		insts.push_back(d->getInstByIndex(i));
	}

	vector<unsigned int> d_serial = d->getSerial();
	for (unsigned int i = 0; i < d_serial.size(); i++)
		serial.push_back(d_serial[i]);

	return true;
}

bool Insts::reorder(vector<unsigned int> order) {
	if (order.size() != insts.size()) {
		errs() << "ERROR: The number of elements in order vector should have the same number of instructions\n";
		return false;
	}

	vector<Instruction *> new_order;
	for (unsigned int i = 0; i < order.size(); i++)
		new_order.push_back(insts[order[i]]);
	for (unsigned int i = 0; i < insts.size(); i++)
		insts[i] = new_order[i];

	new_order.clear();

	return true;
}

vector<unsigned int> Insts::getSerial() {
	return serial;
}

unsigned int Insts::getMaxSize() {
	return max_size;
}

Type *Insts::getType() {
	return type;
}

unsigned int Insts::getNumInsts() {
	return insts.size();
}

unsigned int Insts::getNumOp1Preds() {
	return op1_preds.size();
}

unsigned int Insts::getNumOp2Preds() {
	return op2_preds.size();
}

unsigned int Insts::getNumSuccs() {
	return succs.size();
}

Instruction *Insts::getInstByIndex(unsigned int idx) {
	return insts[idx];
}

Insts *Insts::getOp1PredByIndex(unsigned int idx) {
	return op1_preds[idx];
}

Insts *Insts::getOp2PredByIndex(unsigned int idx) {
	return op2_preds[idx];
}

Insts *Insts::getSuccByIndex(unsigned int idx) {
	return succs[idx];
}

bool Insts::isDependent(Insts *d) {
	for (unsigned int i = 0; i < op1_preds.size(); i++)
		if (op1_preds[i] == d)
			return true;
	for (unsigned int i = 0; i < op2_preds.size(); i++)
		if (op2_preds[i] == d)
			return true;
	for (unsigned int i = 0; i < depends.size(); i++)
		if (depends[i] == d)
			return true;

	return false;
}

bool Insts::isDependentChain(Insts *d, map<Insts *, bool> *visited) {
	if (visited->find(this) != visited->end())
		return false;

	(*visited)[this] = true;

	if (this == d)
		return true;

	if (isa<PHINode>(insts[0]))
		return false;

	for (unsigned int i = 0; i < op1_preds.size(); i++) {
		if (op1_preds[i]->isDependentChain(d, visited))
			return true;
	}
	for (unsigned int i = 0; i < op2_preds.size(); i++) {
		if (op2_preds[i]->isDependentChain(d, visited))
			return true;
	}
	for (unsigned int i = 0; i < depends.size(); i++) {
		if (depends[i]->isDependentChain(d, visited))
			return true;
	}

	return false;
}

void Insts::dump() {
	errs() << "(";
	for (unsigned int j = 0; j < serial.size(); j++)
		errs() << serial[j] << ", ";
	errs() << ") Pred1 #: " << op1_preds.size() << ", Pred2 #: " << op2_preds.size() << ", Succ#: " << succs.size() <<	"\n";
	for (unsigned int i = 0; i< insts.size(); i++) {
		errs() << "s" << i << ": ";
		//insts[i]->dump();
		insts[i]->print(errs());
	}
}

void Insts::dumpSeiral() {
	errs() << "(";
	for (unsigned int j = 0; j < serial.size(); j++) {
		if (j) errs() << ",";
		errs() << serial[j];
	}
	errs() << ")";
}

AddrGEP::AddrGEP(LoadInst *ld) {
	if (isa<GetElementPtrInst>(ld->getOperand(0))) {
		GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(ld->getOperand(0));
		base = GEP->getOperand(0);
		for (unsigned int i = 1; i < GEP->getNumOperands(); i++)
			offsets.push_back(GEP->getOperand(i));
	}
	else
		base = ld->getOperand(0);
}

AddrGEP::AddrGEP(StoreInst *st) {
	if (isa<GetElementPtrInst>(st->getOperand(1))) {
		GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(st->getOperand(1));
		base = GEP->getOperand(0);
		for (unsigned int i = 1; i < GEP->getNumOperands(); i++)
			offsets.push_back(GEP->getOperand(i));
	}
	else
		base = st->getOperand(1);
}

Value *AddrGEP::getBase() {
	return base;
}

unsigned int AddrGEP::getNumOffsets() {
	return offsets.size();
}

Value *AddrGEP::getOffsetsByIdx(unsigned int idx) {
	return offsets[idx];
}

bool AddrGEP::isAdjacentAddr(AddrGEP *target, bool debug) {
	if (debug) errs() << "DEBUG 1\n";

	if (base == target->getBase()) {
		if (debug) errs() << "DEBUG 2\n";
		if (!offsets.size() && target->getNumOffsets() == 1) {
			if (isa<ConstantInt>(target->getOffsetsByIdx(0))) {
				ConstantInt *idx = dyn_cast<ConstantInt>(target->getOffsetsByIdx(0));
				if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
					return true;
			}
		}
		else if (offsets.size() == 1 && !target->getNumOffsets()) {
			if (isa<ConstantInt>(offsets[0])) {
				ConstantInt *idx = dyn_cast<ConstantInt>(offsets[0]);
				if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
					return true;
			}
		}
		else if (offsets.size() == target->getNumOffsets()) {
			if (debug) errs() << "DEBUG 3\n";
			if (offsets.size()) {
				unsigned int maxOffset = offsets.size();
				for (unsigned int i = 0; i < maxOffset-1; i++)
					if (offsets[i] != target->getOffsetsByIdx(i))
						return false;
				if (isa<ConstantInt>(offsets[maxOffset-1]) && isa<ConstantInt>(target->getOffsetsByIdx(maxOffset-1))) {
					ConstantInt *idx1 = dyn_cast<ConstantInt>(offsets[maxOffset-1]);
					ConstantInt *idx2 = dyn_cast<ConstantInt>(target->getOffsetsByIdx(maxOffset-1));
					if (idx1->getSExtValue() - idx2->getSExtValue() == 1)
						return true;
					if (idx1->getSExtValue() - idx2->getSExtValue() == -1)
						return true;
				}
				else {
					if (isa<Instruction>(offsets[maxOffset-1])) {
						if (debug) errs() << "DEBUG 4\n";
						Instruction *binary = dyn_cast<Instruction>(offsets[maxOffset-1]);
						if (debug) binary->print(errs());//binary->dump();
						if (binary->getOpcode() == Instruction::Add) {
							if (debug) errs() << "DEBUG 4-1\n";
							if (isa<ConstantInt>(binary->getOperand(0))) {
								if (debug) errs() << "DEBUG 4-2\n";
								ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(0));
								if (binary->getOperand(1) == target->getOffsetsByIdx(maxOffset-1))
									if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
										return true;
							}
							if (isa<ConstantInt>(binary->getOperand(1))) {
								if (debug) errs() << "DEBUG 4-2\n";
								ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
								if (binary->getOperand(0) == target->getOffsetsByIdx(maxOffset-1))
									if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
										return true;
							}
						}
						if (binary->getOpcode() == Instruction::Sub) {
							if (isa<ConstantInt>(binary->getOperand(1))) {
								ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
								if (binary->getOperand(0) == target->getOffsetsByIdx(maxOffset-1))
									if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
										return true;
							}
						}
					}
					if (isa<Instruction>(target->getOffsetsByIdx(maxOffset-1))) {
						if (debug) errs() << "DEBUG 5\n";
						Instruction *binary = dyn_cast<Instruction>(target->getOffsetsByIdx(maxOffset-1));
						if (binary->getOpcode() == Instruction::Add) {
							if (debug) errs() << "DEBUG 5-1\n";
							if (isa<ConstantInt>(binary->getOperand(0))) {
								if (debug) errs() << "DEBUG 5-2\n";
								ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(0));
								if (binary->getOperand(1) == offsets[maxOffset-1])
									if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
										return true;
							}
							if (isa<ConstantInt>(binary->getOperand(1))) {
								if (debug) errs() << "DEBUG 5-2\n";
								ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
								if (binary->getOperand(0) == offsets[maxOffset-1])
									if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
										return true;
							}
						}
						if (binary->getOpcode() == Instruction::Sub) {
							if (isa<ConstantInt>(binary->getOperand(1))) {
								ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
								if (binary->getOperand(0) == offsets[maxOffset-1])
									if (idx->getSExtValue() == 1 || idx->getSExtValue() == -1)
										return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool AddrGEP::isGreaterAddr(AddrGEP *target) {
	if (!offsets.size() && target->getNumOffsets() == 1) {
		if (isa<ConstantInt>(target->getOffsetsByIdx(0))) {
			ConstantInt *idx = dyn_cast<ConstantInt>(target->getOffsetsByIdx(0));
			if (idx->getSExtValue() == -1)
				return true;
			else
				return false;
		}
	}
	else if (offsets.size() == 1 && !target->getNumOffsets()) {
		if (isa<ConstantInt>(offsets[0])) {
			ConstantInt *idx = dyn_cast<ConstantInt>(offsets[0]);
			if (idx->getSExtValue() == 1)
				return true;
			else
				return false;
		}
	}
	else if (offsets.size() == target->getNumOffsets()) {
		unsigned int maxOffset = offsets.size();
		if (isa<ConstantInt>(offsets[maxOffset-1]) && isa<ConstantInt>(target->getOffsetsByIdx(maxOffset-1))) {
			ConstantInt *idx1 = dyn_cast<ConstantInt>(offsets[maxOffset-1]);
			ConstantInt *idx2 = dyn_cast<ConstantInt>(target->getOffsetsByIdx(maxOffset-1));
			if (idx1->getSExtValue() - idx2->getSExtValue() == 1)
				return true;
			if (idx1->getSExtValue() - idx2->getSExtValue() == -1)
				return false;
		}
		else {
			if (isa<Instruction>(offsets[maxOffset-1])) {
				Instruction *binary = dyn_cast<Instruction>(offsets[maxOffset-1]);
				if (binary->getOpcode() == Instruction::Add) {
					if (isa<ConstantInt>(binary->getOperand(0))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(0));
						if (binary->getOperand(1) == target->getOffsetsByIdx(maxOffset-1)) {
							if (idx->getSExtValue() == 1)
								return true;
							if (idx->getSExtValue() == -1)
								return false;
						}
					}
					if (isa<ConstantInt>(binary->getOperand(1))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
						if (binary->getOperand(0) == target->getOffsetsByIdx(maxOffset-1)) {
							if (idx->getSExtValue() == 1)
								return true;
							if (idx->getSExtValue() == -1)
								return false;
						}
					}
				}
				if (binary->getOpcode() == Instruction::Sub) {
					if (isa<ConstantInt>(binary->getOperand(1))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
						if (binary->getOperand(0) == target->getOffsetsByIdx(maxOffset-1)) {
							if (idx->getSExtValue() == 1)
								return false;
							if (idx->getSExtValue() == -1)
								return true;
						}
					}
				}
			}
			if (isa<Instruction>(target->getOffsetsByIdx(maxOffset-1))) {
				Instruction *binary = dyn_cast<Instruction>(target->getOffsetsByIdx(maxOffset-1));
				if (binary->getOpcode() == Instruction::Add) {
					if (isa<ConstantInt>(binary->getOperand(0))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(0));
						if (binary->getOperand(1) == offsets[maxOffset-1]) {
							if (idx->getSExtValue() == 1)
								return false;
							if (idx->getSExtValue() == -1)
								return true;
						}
					}
					if (isa<ConstantInt>(binary->getOperand(1))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
						if (binary->getOperand(0) == offsets[maxOffset-1]) {
							if (idx->getSExtValue() == 1)
								return false;
							if (idx->getSExtValue() == -1)
								return true;
						}
					}
				}
				if (binary->getOpcode() == Instruction::Sub) {
					if (isa<ConstantInt>(binary->getOperand(1))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(binary->getOperand(1));
						if (binary->getOperand(0) == offsets[maxOffset-1]) {
							if (idx->getSExtValue() == 1)
								return true;
							if (idx->getSExtValue() == -1)
								return false;
						}
					}
				}
			}
		}
	}
	return false;
}

void AddrGEP::dump() {
	//base->dump();
	base->print(errs());
	for (unsigned int i = 0; i < offsets.size(); i++)
		offsets[i]->print(errs()); //offsets[i]->dump();
}

void DG_ICvec::addNode(Insts *s) {
	nodes.push_back(s);
}

void DG_ICvec::eraseNode(Insts *s) {
	vector<Insts *>::iterator I;
	for (I = nodes.begin(); I != nodes.end(); I++) {
		if (*I == s) {
			nodes.erase(I);
			I--;
			break;
		}
	}
	if (I == nodes.end())
		errs() << "ERROR: should not be end of iteration \n";
}

void DG_ICvec::updateEdges() {
	edges.clear();

	for (unsigned int i = 0; i < nodes.size(); i++)
		for (unsigned int j = 0; j < nodes.size(); j++)
			if (i != j)
				if (nodes[i]->isDependent(nodes[j]))
					edges.push_back(make_pair(nodes[i], nodes[j]));
}

unsigned int DG_ICvec::getNumNodes() {
	return nodes.size();
}

void DG_ICvec::getReadyGroups(vector<Insts *> *rd) {
	for (unsigned int i = 0; i < nodes.size(); i++) {
		unsigned int j;
		for (j = 0; j < edges.size(); j++) {
			if (nodes[i] == edges[j].first)
				break;
		}
		if (j == edges.size())
			rd->push_back(nodes[i]);
	}
}

void DG_ICvec::dump() {
	map<Insts *, unsigned int> serial;
	for (unsigned int i = 0; i < nodes.size(); i++) {
		errs() << "NODE " << i << "\n";
		nodes[i]->dump();
		serial[nodes[i]] = i;
	}
	errs() << "EDGE\n";
	for (unsigned int i = 0; i < edges.size(); i++)
		errs() << serial[edges[i].first] << " -> " << serial[edges[i].second] << "\n";
}

LP_node_ICvec::LP_node_ICvec(Insts *s) {
	for (unsigned int i = 0; i < s->getNumInsts(); i++)
		values.push_back(s->getInstByIndex(i));
}

LP_node_ICvec::LP_node_ICvec(Insts *s, unsigned int operand) {
	for (unsigned int i = 0; i < s->getNumInsts(); i++)
		values.push_back(s->getInstByIndex(i)->getOperand(operand));
}

unsigned int LP_node_ICvec::getNumValues() {
	return values.size();
}

unsigned int LP_node_ICvec::getIndexByValue(Value *v) {
	for (unsigned int i = 0; i < values.size(); i++)
		if (values[i] == v)
			return i;
	return 9999;
}

Value *LP_node_ICvec::getValueByIndex(unsigned int idx) {
	return values[idx];
}

bool LP_node_ICvec::doesIncludeValue(Value *target) {
	for (unsigned int i = 0; i < values.size(); i++) {
		if (values[i] == target)
			return true;
	}
	return false;
}

bool LP_node_ICvec::isSameExactly(Insts* node) {
	if (values.size() != node->getNumInsts())
		return false;

	for (unsigned int i = 0; i < values.size(); i++) {
		if (values[i] != dyn_cast<Value>(node->getInstByIndex(i)))
			return false;
	}
	return true;
}

bool LP_node_ICvec::isIncludedAll(LP_node_ICvec *node) {
	for (unsigned int i = 0; i < values.size(); i++) {
		if (!isa<Instruction>(values[i]))
			continue;
		unsigned int j;
		for (j = 0; j < node->getNumValues(); j++) {
			if (values[i] == node->getValueByIndex(j))
				break;
		}
		if (j == node->getNumValues())
			return false;
	}
	return true;
}

bool LP_node_ICvec::isIncludedPartially(LP_node_ICvec *node) {
	for (unsigned int i = 0; i < values.size(); i++) {
		if (!isa<Instruction>(values[i]))
			continue;
		for (unsigned int j = 0; j < node->getNumValues(); j++) {
			if (values[i] == node->getValueByIndex(j))
				return true;
		}
	}
	return false;
}

void LP_node_ICvec::dump() {
	for (unsigned int i = 0; i < values.size(); i++)
		values[i]->print(errs()); //values[i]->dump();
}

void LP_ICvec::addNode(Insts *s) {
	if (isa<GetElementPtrInst>(s->getInstByIndex(0)))
		return;

	if (s->getNumInsts() == 1) {
		list.push_back(new LP_node_ICvec(s));
	}
	else if (isa<LoadInst>(s->getInstByIndex(0))) {
		list.push_back(new LP_node_ICvec(s));
	}
	else if (isa<StoreInst>(s->getInstByIndex(0))) {

	}
	else if (s->getInstByIndex(0)->isBinaryOp()){
		map<LP_node_ICvec *, unsigned int> counts[2];
		bool has_same_order[2];

		for (unsigned int j = 0; j < 2; j++)
			for (unsigned int i = 0; i < s->getNumInsts(); i++) {
				for (unsigned int k = 0; k < list.size(); k++) {
					if (list[k]->doesIncludeValue(s->getInstByIndex(i)->getOperand(j))) {
						if (counts[j].find(list[k]) == counts[j].end())
							counts[j][list[k]] = 1;
						else
							counts[j][list[k]]++;
					}
				}
			}


		for (unsigned int j = 0; j < 2; j++) {
			if (counts[j].size() == 1) {
				if (counts[j].begin()->second == s->getNumInsts()) {
					has_same_order[j] = true;
					for (unsigned int i = 0; i < s->getNumInsts(); i++) {
						unsigned int idx = counts[j].begin()->first->getIndexByValue(s->getInstByIndex(i)->getOperand(j));
						if (i != idx)
							has_same_order[j] = false;
					}
				}
				else
					has_same_order[j] = false;
			}
			else
				has_same_order[j] = false;
		}

		if (!has_same_order[0] && !has_same_order[1]) {
			//errs() << "Reordering binary op \n", s->dump();

			vector<unsigned int> new_order[2];
			for (unsigned int j = 0; j < 2; j++)
				if (counts[j].size() == 1) {
					vector<pair<unsigned int, int> > order;
					for (unsigned int i = 0; i < s->getNumInsts(); i++) {
						unsigned int idx = counts[j].begin()->first->getIndexByValue(s->getInstByIndex(i)->getOperand(j));
						if (idx == 9999)
							errs() << "ERROR: should be one of value in the LP_node\n";
						else
							order.push_back(make_pair(i, idx));
					}
					if (sortPairVector(&order)) {
						for (unsigned int k = 0; k < order.size(); k++)
							new_order[j].push_back(order[k].first);
						if (new_order[j].size() != s->getNumInsts())
							errs() << "ERROR: should be the same number of value\n";

					}
				}

			if (new_order[1].size()) {
				//errs() << "Reordering based on the second operands from\n";
				//s->dump();
				s->reorder(new_order[1]);
				//errs() << "to\n";
				//s->dump();
				//errs() << "\n";
			}
			else if (new_order[0].size()) {
				//errs() << "Reordering based on the first operands from\n";
				//s->dump();
				s->reorder(new_order[0]);
				//errs() << "to\n";
				//s->dump();
				//errs() << "\n";
			}
			else {
				//errs() << "No reordering\n\n";
			}
		}

		list.push_back(new LP_node_ICvec(s));
	}
	else
		errs() << "ERROR: The case is not implemented yet (LP)\n", s->dump();
}

unsigned int LP_ICvec::getNumberOfReuses(Insts *s) {
	vector<LP_node_ICvec *> vp_list;
	unsigned int reuses = 0;

	if (isa<GetElementPtrInst>(s->getInstByIndex(0)))
		return 100000 + s->getNumInsts();

	if (isa<LoadInst>(s->getInstByIndex(0)))
		return 10000 + s->getNumInsts();

	if (isa<StoreInst>(s->getInstByIndex(0)))
		vp_list.push_back(new LP_node_ICvec(s, 0));
	else
		for (unsigned int j = 0; j < s->getInstByIndex(0)->getNumOperands(); j++)
			vp_list.push_back(new LP_node_ICvec(s, j));

	for (unsigned int i = 0; i < vp_list.size(); i++) {
		for (unsigned int j = 0; j < list.size(); j++) {
			if (vp_list[i]->isIncludedPartially(list[j]))
				reuses++;
		}
	}
	return reuses;
}

bool LP_ICvec::sortPairVector(vector<pair<unsigned int, int> > *vec) {
	bool changed = false;
	for (unsigned int l = vec->size()-1; 0 < l; l--)
		for (unsigned int i = 0; i < l; i++)
			if ((*vec)[i+1].second < (*vec)[i].second) {
				unsigned int tmp_first, tmp_second;
				tmp_first = (*vec)[i].first;
				tmp_second = (*vec)[i].second;
				(*vec)[i].first = (*vec)[i+1].first;
				(*vec)[i].second = (*vec)[i+1].second;
				(*vec)[i+1].first = tmp_first;
				(*vec)[i+1].second = tmp_second;
				changed = true;
			}
	return changed;
}

void LP_ICvec::dump() {
	for (unsigned int i = 0; i < list.size(); i++) {
		errs() << "LP node" << i << "\n";
		list[i]->dump();
	}
}

void PackedI_ICvec::addInst(Instruction *inst) {
	insts.push_back(inst);
}

void PackedI_ICvec::addInst(Instruction *inst, BasicBlock *b) {
	insts.push_back(inst);
	b->getInstList().insert(b->end(), inst);
}

Instruction *PackedI_ICvec::getLastInst() {
	if (insts.size())
		return insts[insts.size()-1];
	else {
		errs() << "ERROR: no instructions in PackedI\n";
		Instruction *tmp;
		return tmp;
	}
}

void PackedI_ICvec::dump() {
	for (unsigned int i = 0; i< insts.size(); i++) {
		errs() << "s" << i << ": ";
		//insts[i]->dump();
		insts[i]->print(errs());
	}
}

PackingS_ICvec::PackingS_ICvec(BasicBlock *basicBlock) {
	parent = basicBlock;
}

void PackingS_ICvec::addMappingInst(Instruction *origInst, Instruction *newInst, unsigned int length, unsigned int idx) {
	if (mapping.find(origInst) == mapping.end())
		mapping[origInst] = make_pair(newInst, make_pair(length, idx));
	else {
		errs() << "ERROR: already mapped instruction - ";
		//origInst->dump();
		origInst->print(errs());
	}
}

bool PackingS_ICvec::isMappingInst(Instruction *origInst) {
	if (mapping.find(origInst) == mapping.end())
		return false;
	else
		return true;
}

Instruction *PackingS_ICvec::getMappingNewInst(Instruction *origInst) {
	if (mapping.find(origInst) == mapping.end()) {
		errs() << "ERROR: could not find mapped instruction - ";
		//origInst->dump();
		origInst->print(errs());
		return origInst;
	}
	else
		return mapping[origInst].first;
}

unsigned int PackingS_ICvec::getMappingLength(Instruction *origInst) {
	if (mapping.find(origInst) == mapping.end()) {
		errs() << "ERROR: could not find mapped instruction - ";
		//origInst->dump();
		origInst->print(errs());
		return 0;
	}
	else
		return mapping[origInst].second.first;
}

unsigned int PackingS_ICvec::getMappingPosition(Instruction *origInst) {
	if (mapping.find(origInst) == mapping.end()) {
		errs() << "ERROR: could not find mapped instruction - ";
		//origInst->dump();
		origInst->print(errs());
		return 0;
	}
	else
		return mapping[origInst].second.second;
}

void PackingS_ICvec::addProceededSingleInst(Instruction *inst) {
	if (proceededSingleInst.find(inst) == proceededSingleInst.end())
		proceededSingleInst[inst] = true;
	else {
		errs() << "ERROR: already mapped instruction - ";
		//inst->dump();
		inst->print(errs());
	}
}

bool PackingS_ICvec::isProceededSingleInst(Instruction *inst) {
	if (proceededSingleInst.find(inst) == proceededSingleInst.end())
		return false;
	else
		return true;
}

void PackingS_ICvec::addLoadAddr(Value *base, int idx, Value *GEPInst, Instruction *loadInst) {
	loadAddr[base].push_back(idx);
	loadAddrTable[base][idx] = GEPInst;
	loadAddrTableLoad[base][idx] = dyn_cast<LoadInst>(loadInst);
}

bool PackingS_ICvec::isIncludedLoadAddr(Value *base, int idx) {
	for (unsigned int i = 0; i < loadAddr[base].size(); i++)
		if (loadAddr[base][i] == idx)
			return true;
	return false;
}

vector<pair<Value *, pair<int, Value *> > > *PackingS_ICvec::getLoadVectorAddr(int sizeReg) {
	vector<pair<Value *, pair<int, Value *> > > *loadVectorAddr = new vector<pair<Value *, pair<int, Value *> > >;
	int baseAddr;

	for (map<Value*, vector<int > >::iterator I = loadAddr.begin(); I != loadAddr.end(); I++) {
		sort(I->second.begin(), I->second.end());
		baseAddr = I->second[0];
		loadVectorAddr->push_back(make_pair(I->first, make_pair(baseAddr, loadAddrTable[I->first][baseAddr])));
		for (unsigned int j = 1; j < I->second.size(); j++) {
			if (baseAddr+sizeReg <= I->second[j]) {
				baseAddr = I->second[j];
				loadVectorAddr->push_back(make_pair(I->first, make_pair(baseAddr, loadAddrTable[I->first][baseAddr])));
			}
		}
	}
	return loadVectorAddr;
}

pair<unsigned int, unsigned int> PackingS_ICvec::getLoadVectorLoc(Value *base, int idx, int sizeReg) {
	int baseAddr;
	unsigned int locReg = 0;
	for (map<Value*, vector<int > >::iterator I = loadAddr.begin(); I != loadAddr.end(); I++) {
		sort(I->second.begin(), I->second.end());
		baseAddr = I->second[0];
		if (I->first == base && baseAddr <= idx && idx < baseAddr+sizeReg)
			return make_pair(locReg, (unsigned int) (idx - baseAddr));
		locReg++;
		for (unsigned int j = 1; j < I->second.size(); j++) {
			if (baseAddr+sizeReg <= I->second[j]) {
				baseAddr = I->second[j];
				if (I->first == base && baseAddr <= idx && idx < baseAddr+sizeReg)
					return make_pair(locReg, (unsigned int) (idx - baseAddr));
				locReg++;
			}
		}
	}

	errs() << "ERROR: The address should be found on one of vector load regs\n";
	return make_pair(locReg, 0);
}

Constant *PackingS_ICvec::getConstantValue(unsigned int constant) {
	return ConstantInt::get(Type::getInt32Ty(parent->getContext()), constant);
}

Constant *PackingS_ICvec::getConstantValue(int constant) {
	return ConstantInt::get(Type::getInt32Ty(parent->getContext()), constant, true);
}

UndefValue *PackingS_ICvec::getUndefValue(Type *type) {
	return UndefValue::get(type);
}

UndefValue *PackingS_ICvec::getUndefValue(Type *type, unsigned int size) {
	return UndefValue::get(VectorType::get(type, size));
}

Constant *PackingS_ICvec::getIdxVector(vector<unsigned int> idx) {
	vector<Constant *> idx_constant;
	for (unsigned int k = 0; k < idx.size(); k++)
		idx_constant.push_back(getConstantValue(idx[k]));
	ArrayRef<Constant *> idx_vector(idx_constant);
	return ConstantVector::get(idx_vector);
}

Constant *PackingS_ICvec::getIdxVector(vector<int> idx) {
	vector<Constant *> idx_constant;
	for (unsigned int k = 0; k < idx.size(); k++)
		idx_constant.push_back(getConstantValue(idx[k]));
	ArrayRef<Constant *> idx_vector(idx_constant);
	return ConstantVector::get(idx_vector);
}

void PackingS_ICvec::packSingleInst(Insts *s, unsigned int i, vector<PackedI_ICvec *> *packedInst) {
	unsigned int serialNewInst = 0;
	PackedI_ICvec *packedI = new PackedI_ICvec;
	IRBuilder<> builder(parent);

	for (unsigned int l = 0; l < s->getInstByIndex(0)->getNumOperands(); l++) {
		if (isa<Instruction>(s->getInstByIndex(0)->getOperand(l))) {
			Instruction *operand = dyn_cast<Instruction>(s->getInstByIndex(0)->getOperand(l));
			if (isMappingInst(operand) && !isProceededSingleInst(operand)) {
				Instruction *newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(operand), getConstantValue(getMappingPosition(operand)), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
				packedI->addInst(newInst);
				operand->replaceAllUsesWith(newInst);
				addMappingInst(newInst, getMappingNewInst(operand), getMappingLength(operand), getMappingPosition(operand));
				addProceededSingleInst(newInst);
			}
			if (isa<PHINode>(operand)) {
				InstNeedtoExtractforPhiNode[operand] = true;
			}
		}
	}

	Instruction *newInst = s->getInstByIndex(0)->clone();
	if (s->getInstByIndex(0)->hasName())
		newInst->setName("S" + to_string(i) + "_" + to_string(serialNewInst++));
	packedI->addInst(newInst, parent);
	s->getInstByIndex(0)->replaceAllUsesWith(newInst);

	packedInst->push_back(packedI);
}

void PackingS_ICvec::packLoadInsts(Insts *s, unsigned int i, vector<PackedI_ICvec *> *packedInst) {
	unsigned int serialNewInst = 0;
	PackedI_ICvec *packedI = new PackedI_ICvec;
	IRBuilder<> builder(parent);
	map<Instruction *, Instruction *> mapLoad;
	Instruction *newInst;
	bool useVecLoad = false;

	if (s->getNumInsts() == 2) {
		AddrGEP *S1addr = new AddrGEP(dyn_cast<LoadInst>(s->getInstByIndex(0)));
		AddrGEP *S2addr = new AddrGEP(dyn_cast<LoadInst>(s->getInstByIndex(1)));
		if (S1addr->isAdjacentAddr(S2addr, false))
			useVecLoad = true;
		delete S1addr;
		delete S2addr;
	}

	if (useVecLoad) {
		AddrGEP *S1addr = new AddrGEP(dyn_cast<LoadInst>(s->getInstByIndex(0)));
		AddrGEP *S2addr = new AddrGEP(dyn_cast<LoadInst>(s->getInstByIndex(1)));
		LoadInst *base;
		if (S1addr->isGreaterAddr(S2addr))
			base = dyn_cast<LoadInst>(s->getInstByIndex(1));
		else
			base = dyn_cast<LoadInst>(s->getInstByIndex(0));

		Type *VecTy = VectorType::get(base->getType(), 2);
		Type *VecPtrTy = PointerType::get(VecTy, 0);
		newInst = (dyn_cast<Instruction>(builder.CreateBitCast(base->getOperand(0), VecPtrTy, "LoadOpt" + to_string(serialNewInst++))));
		packedI->addInst(newInst);
		newInst = (dyn_cast<Instruction>(builder.CreateLoad(newInst, "LoadOpt" + to_string(serialNewInst++))));
		LoadInst *ldInst = dyn_cast<LoadInst>(newInst);
		ldInst->setAlignment(base->getAlignment());
		packedI->addInst(newInst);

		if (S1addr->isGreaterAddr(S2addr)) {
			addMappingInst(s->getInstByIndex(0), packedI->getLastInst(), s->getNumInsts(), 1);
			addMappingInst(s->getInstByIndex(1), packedI->getLastInst(), s->getNumInsts(), 0);
		}
		else {
			addMappingInst(s->getInstByIndex(0), packedI->getLastInst(), s->getNumInsts(), 0);
			addMappingInst(s->getInstByIndex(1), packedI->getLastInst(), s->getNumInsts(), 1);
		}

		delete S1addr;
		delete S2addr;

		for (unsigned int j = 0; j< 2; j++) {
			bool hasOutsideUse = false;
			for (Value::user_iterator I = s->getInstByIndex(j)->user_begin(); I != s->getInstByIndex(j)->user_end(); I++) {
				if (isa<Instruction>(*I)) {
					Instruction *user = dyn_cast<Instruction>(*I);
					if (parent != user->getParent()) {
						hasOutsideUse = true;
					}
					if (isa<PHINode>(*I)) {
						newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(s->getInstByIndex(j)), getConstantValue(getMappingPosition(s->getInstByIndex(j))), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
						packedI->addInst(newInst);
						s->getInstByIndex(j)->replaceAllUsesWith(newInst);
						hasOutsideUse = false;
						break;
					}
				}
			}
			if (hasOutsideUse) {
				newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(s->getInstByIndex(j)), getConstantValue(getMappingPosition(s->getInstByIndex(j))), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
				packedI->addInst(newInst);
				s->getInstByIndex(j)->replaceUsesOutsideBlock(newInst, parent);
				//break;
			}
		}
	}
	else {
		for (unsigned int j = 0; j < s->getNumInsts(); j++) {
			newInst = s->getInstByIndex(j)->clone();
			packedI->addInst(newInst, parent);
			mapLoad[s->getInstByIndex(j)] = newInst;
			s->getInstByIndex(j)->replaceAllUsesWith(newInst);
		}

		Type *Int32Ty = Type::getInt32Ty(s->getInstByIndex(0)->getContext());
		Type *VecTy = VectorType::get(s->getInstByIndex(0)->getType(), s->getNumInsts());
		Value *undefVec = UndefValue::get(VecTy);
		for (unsigned int j = 0; j < s->getNumInsts(); j++) {
			if (j)
				newInst = dyn_cast<Instruction>(builder.CreateInsertElement(newInst, mapLoad[s->getInstByIndex(j)], ConstantInt::get(Int32Ty, j), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
			else
				newInst = dyn_cast<Instruction>(builder.CreateInsertElement(undefVec, mapLoad[s->getInstByIndex(j)], ConstantInt::get(Int32Ty, j), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
			packedI->addInst(newInst);
		}

		for (unsigned int j = 0; j < s->getNumInsts(); j++)
			addMappingInst(s->getInstByIndex(j), packedI->getLastInst(), s->getNumInsts(), j);

		/*for (unsigned int j = 0; j < s->getNumInsts(); j++)
			if (InstNeedtoExtractforPhiNode.find(s->getInstByIndex(j)) != InstNeedtoExtractforPhiNode.end()) {
				Instruction *newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(s->getInstByIndex(j)), getConstantValue(getMappingPosition(s->getInstByIndex(j))), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
				packedI->addInst(newInst);
				s->getInstByIndex(j)->replaceAllUsesWith(newInst);
				//addMappingInst(newInst, getMappingNewInst(s->getInstByIndex(j)), getMappingLength(operand), getMappingPosition(operand));
				addProceededSingleInst(newInst);
			}*/
	}

	packedInst->push_back(packedI);
}

bool PackingS_ICvec::isPackableLoadInstsGroups(vector<Insts *> ld) {
	for (unsigned int j = 0; j < ld.size(); j++) {
		for (unsigned int k = 0; k < ld[j]->getNumInsts(); k++) {
			if (isa<GetElementPtrInst>(ld[j]->getInstByIndex(k)->getOperand(0))) {
				GetElementPtrInst *ptrInst = dyn_cast<GetElementPtrInst>(ld[j]->getInstByIndex(k)->getOperand(0));
				if (ptrInst->getNumOperands() == 2) {
					if (!isa<ConstantInt>(ptrInst->getOperand(1))) {
						errs() << "DEBUG: index should be constant int\n";
						return false;
					}
				}
				else {
					errs() << "DEBUG: only handle 1D index\n";
					return false;
				}
			}
		}
	}

	for (unsigned int k = 0; k < ld.size(); k++)
		for (unsigned int j = 0; j < ld[k]->getNumInsts(); j++) {
			for (Value::user_iterator I = ld[k]->getInstByIndex(j)->user_begin(); I != ld[k]->getInstByIndex(j)->user_end(); I++) {
				if (isa<Instruction>(*I)) {
					Instruction *user = dyn_cast<Instruction>(*I);
					if (parent != user->getParent()) {
						//errs() << "DEBUG: Loads - use outside of BB\n", ld[k]->getInstByIndex(j)->dump(), user->dump(), errs() << "DEBUG END\n";
						errs() << "DEBUG: Loads - use outside of BB\n", ld[k]->getInstByIndex(j)->print(errs()), user->print(errs()), errs() << "DEBUG END\n";
						return false;
					}
				}
			}
		}

	return true;
}

bool PackingS_ICvec::hasOutsideUse(Insts *s) {
	for (unsigned int j = 0; j < s->getNumInsts(); j++) {
		for (Value::user_iterator I = s->getInstByIndex(j)->user_begin(); I != s->getInstByIndex(j)->user_end(); I++) {
			if (isa<Instruction>(*I)) {
				Instruction *user = dyn_cast<Instruction>(*I);
				if (parent != user->getParent()) {
					//errs() << "DEBUG: BinOp - use outside of BB\n", s->getInstByIndex(j)->dump(), user->dump(), errs() << "DEBUG END\n";
					errs() << "DEBUG: BinOp - use outside of BB\n", s->getInstByIndex(j)->print(errs()), user->print(errs()), errs() << "DEBUG END\n";
					return false;
				}
			}
		}
	}

	return true;
}

/*void PackingS_ICvec::packLoadInstsGroups(vector<Insts *> ld, unsigned int i, vector<PackedI_ICvec *> *packedInst) {
	unsigned int serialNewInst = 0;
	IRBuilder<> builder(parent);
	unsigned int maxInsts = 0;

	for (unsigned int j = 0; j < ld.size(); j++) {
		for (unsigned int k = 0; k < ld[j]->getNumInsts(); k++) {
			if (isa<GetElementPtrInst>(ld[j]->getInstByIndex(k)->getOperand(0))) {
				GetElementPtrInst *ptrInst = dyn_cast<GetElementPtrInst>(ld[j]->getInstByIndex(k)->getOperand(0));
				if (ptrInst->getNumOperands() == 2) {
					if (isa<ConstantInt>(ptrInst->getOperand(1))) {
						ConstantInt *idx = dyn_cast<ConstantInt>(ptrInst->getOperand(1));
						addLoadAddr(ptrInst->getOperand(0), idx->getSExtValue(), ptrInst, ld[j]->getInstByIndex(k));
					}
					else
						outs() << "ERROR: index should be constant int\n";
				}
				else
					outs() << "ERROR: only handle 1D index\n";
			}
			else {
				addLoadAddr(ld[j]->getInstByIndex(k)->getOperand(0), 0, ld[j]->getInstByIndex(k)->getOperand(0), ld[j]->getInstByIndex(k));
			}
		}
		if (maxInsts < ld[j]->getNumInsts())
			maxInsts = ld[j]->getNumInsts();
	}

	vector<pair<Value *, pair<int, Value *> > > *loadVectorAddr = getLoadVectorAddr((int) maxInsts);

	map<unsigned int, Instruction *> newLoadMap;
	unsigned int newLoadMapSerial = 0;

	vector<pair<string, int> > inputRegs;
	vector<pair<unsigned int, unsigned int> > *outputRegs;
	outputRegs = new vector<pair<unsigned int, unsigned int> > [ld.size()];

	PackedI_ICvec *packedI = new PackedI_ICvec;

	for (unsigned int j = 0; j < loadVectorAddr->size(); j++) {
		Instruction *newInst;
		bool useVLoad = true;

		for (unsigned int k = 0; k < maxInsts; k++) {
			if (!isIncludedLoadAddr((*loadVectorAddr)[j].first, (*loadVectorAddr)[j].second.first + k)) {
				useVLoad = false;
				break;
			}
		}

		if (useVLoad) {
			inputRegs.push_back(make_pair((*loadVectorAddr)[j].first->getName(), (*loadVectorAddr)[j].second.first));
			Type *VecTy = VectorType::get((*loadVectorAddr)[j].first->getType()->getArrayElementType(), maxInsts);
			Type *VecPtrTy = PointerType::get(VecTy, 0);
			newInst = (dyn_cast<Instruction>(builder.CreateBitCast((*loadVectorAddr)[j].second.second, VecPtrTy, "LoadOpt" + to_string(serialNewInst++))));
			packedI->addInst(newInst);
			newInst = (dyn_cast<Instruction>(builder.CreateLoad(newInst, "LoadOpt" + to_string(serialNewInst++))));
			LoadInst *ldInst = dyn_cast<LoadInst>(newInst);
			ldInst->setAlignment(loadAddrTableLoad[(*loadVectorAddr)[j].first][(*loadVectorAddr)[j].second.first]->getAlignment());
			packedI->addInst(newInst);
			newLoadMap[newLoadMapSerial++] = newInst;
		}
		else {
			inputRegs.push_back(make_pair((*loadVectorAddr)[j].first->getName(), (*loadVectorAddr)[j].second.first));
			Type *Int32Ty = Type::getInt32Ty(ld[0]->getInstByIndex(0)->getContext());
			UndefValue *undefValue = getUndefValue((*loadVectorAddr)[j].first->getType()->getArrayElementType(), maxInsts);
			bool first = true;
			for (unsigned int k = 0; k < maxInsts; k++) {
				if (isIncludedLoadAddr((*loadVectorAddr)[j].first, (*loadVectorAddr)[j].second.first + k)) {
					Instruction *newLoad = loadAddrTableLoad[(*loadVectorAddr)[j].first][(*loadVectorAddr)[j].second.first+k]->clone();
					newLoad->dump();
					packedI->addInst(newLoad, parent);
					if (first) {
						newInst = dyn_cast<Instruction>(builder.CreateInsertElement(undefValue, newLoad, ConstantInt::get(Int32Ty, k), "LoadOpt" + to_string(serialNewInst++)));
						first = false;

					}
					else
						newInst = dyn_cast<Instruction>(builder.CreateInsertElement(newInst, newLoad, ConstantInt::get(Int32Ty, k), "LoadOpt" + to_string(serialNewInst++)));
					packedI->addInst(newInst);
				}
			}
			if (first)
				errs() << "ERROR: should not be the first\n";

			newLoadMap[newLoadMapSerial++] = newInst;
		}
	}
	loadVectorAddr->clear();
	delete loadVectorAddr;

	for (unsigned int j = 0; j < ld.size(); j++) {
		for (unsigned int k = 0; k < ld[j]->getNumInsts(); k++) {
			if (isa<GetElementPtrInst>(ld[j]->getInstByIndex(k)->getOperand(0))) {
				GetElementPtrInst *ptrInst = dyn_cast<GetElementPtrInst>(ld[j]->getInstByIndex(k)->getOperand(0));
				if (ptrInst->getNumOperands() == 2) {
					ConstantInt *idx;
					if (isa<ConstantInt>(ptrInst->getOperand(1)))
						idx = dyn_cast<ConstantInt>(ptrInst->getOperand(1));
					else
						errs() << "ERROR: index should be constant int\n";
					outputRegs[j].push_back(getLoadVectorLoc(ptrInst->getOperand(0), idx->getSExtValue(), (int) maxInsts));
				}
				else
					errs() << "ERROR: only handle 1D index\n";
			}
			else
				outputRegs[j].push_back(getLoadVectorLoc(ld[j]->getInstByIndex(k)->getOperand(0), 0, (int) maxInsts));
		}
	}

	errs() << "DEBUG Vector Loads: ";
	for (unsigned int j = 0; j < inputRegs.size(); j++)
		errs() << "%" << inputRegs[j].first << " " << inputRegs[j].second << "~" << inputRegs[j].second + maxInsts -1 << ", ";
	errs() << "\n";
	errs() << "DEBUG Output Vector Regs\n";
	for (unsigned int j = 0; j < ld.size(); j++) {
		errs() << "Reg" << j << ": ";
		for (unsigned int k = 0; k < outputRegs[j].size(); k++)
			errs() << "<" << outputRegs[j][k].first << ", " << outputRegs[j][k].second << "> ";
		errs() << "\n";
	}

	JoonmooHuh perm;
	vector<pair<pair<unsigned int, unsigned int>, vector<int> *> > perm_seq;
	pair<unsigned int, vector<unsigned int> *> *perm_loc = new pair<unsigned int, vector<unsigned int> *> [ld.size()];
	perm.generateVperm(maxInsts, inputRegs, ld.size(), outputRegs, &perm_seq, perm_loc);

	for (unsigned int j = 0; j < perm_seq.size(); j++) {
		vector<unsigned int> idx;
		Instruction *newInst;

		for (unsigned int k = 0; k < perm_seq[j].second->size(); k++)
			if ((*perm_seq[j].second)[k] == -1)
				idx.push_back(0);
			else
				idx.push_back((unsigned int)(*perm_seq[j].second)[k]);
		newInst = (dyn_cast<Instruction>(builder.CreateShuffleVector(newLoadMap[perm_seq[j].first.first], newLoadMap[perm_seq[j].first.second], getIdxVector(idx), "LoadOpt" + to_string(serialNewInst++))));
		newLoadMap[newLoadMapSerial++] = newInst;
		packedI->addInst(newInst);
	}

	packedInst->push_back(packedI);

	for (unsigned int k = 0; k < ld.size(); k++) {
		PackedI_ICvec *packedII = new PackedI_ICvec;
		vector<unsigned int> idx;
		Instruction *newInst;

		for (unsigned int j = 0; j < ld[k]->getNumInsts(); j++)
			idx.push_back((unsigned int)(*perm_loc[k].second)[j]);
		newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(newLoadMap[perm_loc[k].first], getUndefValue(ld[k]->getType(), maxInsts),  getIdxVector(idx), "S" + to_string(i+k) + "_0"));
		packedII->addInst(newInst);
		for (unsigned int j = 0; j < ld[k]->getNumInsts(); j++) {
			addMappingInst(ld[k]->getInstByIndex(j), packedII->getLastInst(), ld[k]->getNumInsts(), j);
			for (Value::user_iterator I = ld[k]->getInstByIndex(j)->user_begin(); I != ld[k]->getInstByIndex(j)->user_end(); I++) {
				if (isa<Instruction>(*I)) {
					Instruction *user = dyn_cast<Instruction>(*I);
					if (parent != user->getParent())
						errs() << "ERROR Loads - use outside of BB\n", ld[k]->getInstByIndex(j)->dump(), user->dump(), errs() << "ERROR END\n";
					if (isa<PHINode>(*I)) {
						newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(ld[k]->getInstByIndex(j)), getConstantValue(getMappingPosition(ld[k]->getInstByIndex(j))), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
						packedI->addInst(newInst);
						ld[k]->getInstByIndex(j)->replaceAllUsesWith(newInst);
					}
				}
			}
		}

		packedInst->push_back(packedII);
	}
}*/

void PackingS_ICvec::packStoreInsts(Insts *s, unsigned int i, vector<PackedI_ICvec *> *packedInst) {
	unsigned int serialNewInst = 0;
	PackedI_ICvec *packedI = new PackedI_ICvec;
	IRBuilder<> builder(parent);
	bool useVecStore = false;

	//errs() << "DEBUG: packStoreInstEntry\n";
	if (s->getNumInsts() == 2) {
		AddrGEP *S1addr = new AddrGEP(dyn_cast<StoreInst>(s->getInstByIndex(0)));
		AddrGEP *S2addr = new AddrGEP(dyn_cast<StoreInst>(s->getInstByIndex(1)));
		//errs() << "DEBUG: packStoreInst1\n";
		//s->getInstByIndex(0)->dump();
		//s->getInstByIndex(1)->dump();
		if (S1addr->isAdjacentAddr(S2addr, false)) {
			//errs() << "DEBUG: packStoreInst2\n";
			//if (isa<Instruction>(s->getInstByIndex(0)->getOperand(0)) && isa<Instruction>(s->getInstByIndex(1)->getOperand(0)))
			//	if (isMappingInst(dyn_cast<Instruction>(s->getInstByIndex(0)->getOperand(0))) && isMappingInst(dyn_cast<Instruction>(s->getInstByIndex(1)->getOperand(0))))
			//		if (getMappingNewInst(dyn_cast<Instruction>(s->getInstByIndex(0)->getOperand(0)))==dyn_cast<Instruction>(s->getInstByIndex(1)->getOperand(0)))
						useVecStore = true;
		}
		delete S1addr;
		delete S2addr;
	}

	if (useVecStore) {
		AddrGEP *S1addr = new AddrGEP(dyn_cast<StoreInst>(s->getInstByIndex(0)));
		AddrGEP *S2addr = new AddrGEP(dyn_cast<StoreInst>(s->getInstByIndex(1)));
		StoreInst *base;
		if (S1addr->isGreaterAddr(S2addr))
			base = dyn_cast<StoreInst>(s->getInstByIndex(1));
		else
			base = dyn_cast<StoreInst>(s->getInstByIndex(0));

		Type *VecTy = VectorType::get(base->getOperand(0)->getType(), 2);
		Type *VecPtrTy = PointerType::get(VecTy, 0);
		Instruction *newInst = (dyn_cast<Instruction>(builder.CreateBitCast(base->getOperand(1), VecPtrTy, "StoreOpt" + to_string(serialNewInst++))));
		packedI->addInst(newInst);
		newInst = (dyn_cast<Instruction>(builder.CreateStore(getMappingNewInst(dyn_cast<Instruction>(s->getInstByIndex(0)->getOperand(0))), newInst)));
		StoreInst *stInst = dyn_cast<StoreInst>(newInst);
		stInst->setAlignment(base->getAlignment());
		packedI->addInst(newInst);

		delete S1addr;
		delete S2addr;
	}
	else {
		for (unsigned int j = 0; j < s->getNumInsts(); j++) {
			if (isa<Instruction>(s->getInstByIndex(j)->getOperand(0))) {
				Instruction *operand = dyn_cast<Instruction>(s->getInstByIndex(j)->getOperand(0));
				if (isMappingInst(operand)){
					Instruction *newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(operand), getConstantValue(getMappingPosition(operand)), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					packedI->addInst(newInst);

					newInst = dyn_cast<Instruction>(builder.CreateStore(newInst, s->getInstByIndex(j)->getOperand(1)));
					packedI->addInst(newInst);
				}
				else {
					Instruction *newInst = s->getInstByIndex(j)->clone();
					packedI->addInst(newInst, parent);
				}
			}
			else {
				//errs() << "ERROR: store constant value\n";
				Instruction *newInst = s->getInstByIndex(j)->clone();
				packedI->addInst(newInst, parent);
			}
		}
	}

	packedInst->push_back(packedI);
}

void PackingS_ICvec::packBinaryOpInsts(Insts *s, unsigned int i, vector<PackedI_ICvec *> *packedInst) {
	unsigned int serialNewInst = 0;
	PackedI_ICvec *packedI = new PackedI_ICvec;
	IRBuilder<> builder(parent);
	Value *LHS_RHS[2];
	Instruction *newInst;

	for (unsigned int l = 0; l < 2; l++) {
		map <Instruction *, unsigned int> count_S;
		map <Instruction *, unsigned int> size_S;
		bool flag = false;
		bool first_shuffle = true;
		Value *last_shuffle;
		Value *newValue;

		for (unsigned int j = 0; j < s->getNumInsts(); j++) {
			if (isa<Instruction>(s->getInstByIndex(j)->getOperand(l))) {
				Instruction *operand = dyn_cast<Instruction>(s->getInstByIndex(j)->getOperand(l));
				if (isMappingInst(operand)){
					if (count_S.find(getMappingNewInst(operand)) == count_S.end()) {
						count_S[getMappingNewInst(operand)] = 1;
						size_S[getMappingNewInst(operand)] = getMappingLength(operand);
					}
					else
						count_S[getMappingNewInst(operand)]++;
				}
				else
					flag = true;
			}
			else
				flag = true;
		}
		if (flag) {
			for (unsigned int j = 0; j < s->getNumInsts(); j++) {
				if (!isa<Instruction>(s->getInstByIndex(j)->getOperand(l))) {
					if (first_shuffle) {
						newValue = builder.CreateInsertElement(getUndefValue(s->getType(), s->getNumInsts()), s->getInstByIndex(j)->getOperand(l), getConstantValue(j), "S" + to_string(i) + "_" + to_string(serialNewInst++));
						first_shuffle = false;
					}
					else {
						newValue = builder.CreateInsertElement(newValue, s->getInstByIndex(j)->getOperand(l), getConstantValue(j), "S" + to_string(i) + "_" + to_string(serialNewInst++));
					}
					last_shuffle = newValue;
				}
			}

			for (unsigned int j = 0; j < s->getNumInsts(); j++) {
				if (isa<Instruction>(s->getInstByIndex(j)->getOperand(l))) {
					Instruction *operand = dyn_cast<Instruction>(s->getInstByIndex(j)->getOperand(l));
					if (!isMappingInst(operand)) {
						if (first_shuffle) {
							newInst = dyn_cast<Instruction>(builder.CreateInsertElement(getUndefValue(s->getType(), s->getNumInsts()), operand, getConstantValue(j), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
							first_shuffle = false;
						}
						else {
							newInst = dyn_cast<Instruction>(builder.CreateInsertElement(last_shuffle, operand, getConstantValue(j), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
						}
						packedI->addInst(newInst);
						last_shuffle = newInst;
					}
				}
			}
		}
		if (!count_S.size()) {
			if (flag)
				LHS_RHS[l] = last_shuffle;
			else
				errs() << "ERROR: The flag could not be false 0\n";
		}
		else {
			bool inorder = true;

			for (map <Instruction *, unsigned int>::iterator I = count_S.begin(); I != count_S.end(); I++) {
				vector<unsigned int> idx, idx_shorten;

				for (unsigned int j = 0; j < s->getNumInsts(); j++) {
					if (isa<Instruction>(s->getInstByIndex(j)->getOperand(l))) {
						Instruction *operand = dyn_cast<Instruction>(s->getInstByIndex(j)->getOperand(l));
						if (isMappingInst(operand)){
							if (I->first == getMappingNewInst(operand)) {
								idx.push_back(getMappingPosition(operand) + s->getNumInsts());
								idx_shorten.push_back(getMappingPosition(operand) + size_S[getMappingNewInst(operand)]);
								if (j != getMappingPosition(operand))
									inorder = false;
							}
							else {
								idx.push_back(j);
								idx_shorten.push_back(j);
								inorder = false;
							}
						}
						else {
							idx.push_back(j);
							idx_shorten.push_back(j);
							inorder = false;
							if (!flag)
								errs() << "ERROR: The flag could not be false 1\n";
						}
					}
					else {
						idx.push_back(j);
						idx_shorten.push_back(j);
						inorder = false;
						if (!flag)
							errs() << "ERROR: The flag could not be false 2\n";
					}
				}

				if (idx.size() != s->getNumInsts())
					errs() << "ERROR: The number of index should be same\n";
				if (size_S[I->first] < s->getNumInsts()) {
					vector<unsigned int> idx_extend;
					for (unsigned int k = 0; k < s->getNumInsts(); k++)
						idx_extend.push_back(k % size_S[I->first]);
					newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(I->first, getUndefValue(s->getType(), size_S[I->first]), getIdxVector(idx_extend), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					packedI->addInst(newInst);
					if (first_shuffle)
						newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(getUndefValue(s->getType(), s->getNumInsts()), newInst, getIdxVector(idx), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					else
						newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(last_shuffle, newInst, getIdxVector(idx), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					packedI->addInst(newInst);
				}
				else if (s->getNumInsts() < size_S[I->first]) {
					newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(getUndefValue(s->getType(), size_S[I->first]), I->first, getIdxVector(idx_shorten), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					packedI->addInst(newInst);
					vector<unsigned int> idx_shorten_extend;
					for (unsigned int k = 0; k < idx_shorten.size(); k++)
						if (idx[k] == k)
							idx_shorten_extend.push_back(k);
						else
							idx_shorten_extend.push_back(k+idx_shorten.size());
					if (first_shuffle)
						newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(getUndefValue(s->getType(), s->getNumInsts()), newInst, getIdxVector(idx_shorten_extend), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					else
						newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(last_shuffle, newInst, getIdxVector(idx_shorten_extend), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					packedI->addInst(newInst);
				}
				else {
					if (!inorder) {
						if (first_shuffle)
							newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(getUndefValue(s->getType(), s->getNumInsts()), I->first, getIdxVector(idx), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
						else
							newInst = dyn_cast<Instruction>(builder.CreateShuffleVector(last_shuffle, I->first, getIdxVector(idx), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
						packedI->addInst(newInst);
					}
				}
				first_shuffle = false;
				last_shuffle = newInst;
			}
			if (inorder && size_S[count_S.begin()->first] == s->getNumInsts())
				LHS_RHS[l] = count_S.begin()->first;
			else
				LHS_RHS[l] = newInst;
		}
	}

	newInst = dyn_cast<Instruction>(builder.CreateBinOp((enum llvm::Instruction::BinaryOps) s->getInstByIndex(0)->getOpcode(), LHS_RHS[0], LHS_RHS[1], "S" + to_string(i) + "_" + to_string(serialNewInst++)));
	packedI->addInst(newInst);

	vector<Instruction *> addedInsts;
	Instruction *last_newInst = newInst;

	for (unsigned int j = 0; j < s->getNumInsts(); j++) {
		bool hasOutsideUse = false;
		addMappingInst(s->getInstByIndex(j), last_newInst, s->getNumInsts(), j);
		for (Value::user_iterator I = s->getInstByIndex(j)->user_begin(); I != s->getInstByIndex(j)->user_end(); I++) {
			if (isa<Instruction>(*I)) {
				Instruction *user = dyn_cast<Instruction>(*I);
				if (parent != user->getParent()) {
					hasOutsideUse = true;
				}
				if (isa<PHINode>(*I)) {
					newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(s->getInstByIndex(j)), getConstantValue(getMappingPosition(s->getInstByIndex(j))), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
					//packedI->addInst(newInst);
					addedInsts.push_back(newInst);
					s->getInstByIndex(j)->replaceAllUsesWith(newInst);
					addMappingInst(newInst, last_newInst, s->getNumInsts(), j);
					hasOutsideUse = false;
					break;
				}
			}
		}
		if (hasOutsideUse) {
			newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(s->getInstByIndex(j)), getConstantValue(getMappingPosition(s->getInstByIndex(j))), "S" + to_string(i) + "_" + to_string(serialNewInst++)));
			packedI->addInst(newInst);
			s->getInstByIndex(j)->replaceUsesOutsideBlock(newInst, parent);
			//break;
		}
	}

	for (unsigned int j = 0; j < addedInsts.size(); j++)
		packedI->addInst(addedInsts[j]);
	addedInsts.clear();

	packedInst->push_back(packedI);
	//if (i==51) {
	//	errs() << "DEBUG: packedI dump()\n";
	//	packedI->dump();
	//}
}

void PackingS_ICvec::packTerminatorInst(Instruction *inst, vector<PackedI_ICvec *> *packedInst) {
	unsigned int serialNewInst = 0;
	PackedI_ICvec *packedI = new PackedI_ICvec;
	IRBuilder<> builder(parent);

	for (unsigned int l = 0; l < inst->getNumOperands(); l++) {
		if (isa<Instruction>(inst->getOperand(l))) {
			Instruction *operand = dyn_cast<Instruction>(inst->getOperand(l));
			if (isMappingInst(operand) && !isProceededSingleInst(operand)) {
				Instruction *newInst = dyn_cast<Instruction>(builder.CreateExtractElement(getMappingNewInst(operand), getConstantValue(getMappingPosition(operand)), "TerminaterS" + to_string(serialNewInst++)));
				operand->replaceAllUsesWith(newInst);
				addMappingInst(newInst, getMappingNewInst(operand), getMappingLength(operand), getMappingPosition(operand));
				addProceededSingleInst(newInst);
				packedI->addInst(newInst);
			}
		}
	}

	Instruction *newInst = inst->clone();
	packedI->addInst(newInst, parent);
	inst->replaceAllUsesWith(newInst);

	packedInst->push_back(packedI);
}

struct range packPa = { 0, 0 }; struct range unpackPa = { 0, 0 };
struct range packPb = { 1, 1 }; struct range unpackPb = { 0, 0 };
struct range packPc = { 0, 0 }; struct range unpackPc = { 0, 0 };
struct range packPd = { 0, 0 }; struct range unpackPd = { 0, 2 };
struct range packPe = { 0, 0 }; struct range unpackPe = { 1, 1 };
struct range packPf = { 0, 0 }; struct range unpackPf = { 1, 2 };
struct range packPg = { 2, 2 }; struct range unpackPg = { 0, 0 };

CandidatePattern::CandidatePattern(Insts *S1, Insts *S2, vector<pair<Insts *, Insts *> > candidates) {
	bool debug = false;
	//if (S1->getSerial()[0] == 453 && S2->getSerial()[0] == 507) debug = true;

	saved = 0;
	costInside.min = 0;
	costInside.max = 0;
	costOutside.min = 0;
	costOutside.max = 0;

	level1_op1 = findOp1Op2Pattern(S1, S2, 1, candidates, debug);
	level1_op2 = findOp1Op2Pattern(S1, S2, 2, candidates, debug);
	costInside += convertPackCost(level1_op1);
	costInside += convertPackCost(level1_op2);
	costInside += convertUnpackCost(level1_op1);
	costInside += convertUnpackCost(level1_op2);
	if (isa<StoreInst>(S1->getInstByIndex(0))) {
		AddrGEP *S1addr = new AddrGEP(dyn_cast<StoreInst>(S1->getInstByIndex(0)));
		AddrGEP *S2addr = new AddrGEP(dyn_cast<StoreInst>(S2->getInstByIndex(0)));
		if (!S1addr->isAdjacentAddr(S2addr, false)) {
			costOutside.min += 2;
			costOutside.max += 2;
		}
		delete S1addr;
		delete S2addr;
	}
	else {
		costOutside.min += 2;
		costOutside.max += 2;
	}
	saved++;

	level2_1_op1 = P_NONE;
	level2_1_op2 = P_NONE;
	level2_2_op1 = P_NONE;
	level2_2_op2 = P_NONE;
	if (Pc <= level1_op1 && level1_op1 <= Pf) {
		level2_1_op1 = findOp1Op2Pattern(S1->getOp1PredByIndex(0), S2->getOp1PredByIndex(0), 1, candidates, debug);
		level2_1_op2 = findOp1Op2Pattern(S1->getOp1PredByIndex(0), S2->getOp1PredByIndex(0), 2, candidates, debug);
		costInside += convertPackCost(level2_1_op1);
		costInside += convertPackCost(level2_1_op2);
		costInside += convertUnpackCost(level2_1_op1);
		costInside += convertUnpackCost(level2_1_op2);
		saved++;
		if (Pc <= level2_1_op1 && level2_1_op1 <= Pf) {
			if (Pa != findOp1Op2Pattern(S1->getOp1PredByIndex(0)->getOp1PredByIndex(0), S2->getOp1PredByIndex(0)->getOp1PredByIndex(0), 1, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			if (Pa != findOp1Op2Pattern(S1->getOp1PredByIndex(0)->getOp1PredByIndex(0), S2->getOp1PredByIndex(0)->getOp1PredByIndex(0), 2, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			saved++;
		}
		if (Pc <= level2_1_op2 && level2_1_op2 <= Pf) {
			if (Pa != findOp1Op2Pattern(S1->getOp1PredByIndex(0)->getOp2PredByIndex(0), S2->getOp1PredByIndex(0)->getOp2PredByIndex(0), 1, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			if (Pa != findOp1Op2Pattern(S1->getOp1PredByIndex(0)->getOp2PredByIndex(0), S2->getOp1PredByIndex(0)->getOp2PredByIndex(0), 2, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			saved++;
		}
	}
	if (Pc <= level1_op2 && level1_op2 <= Pf) {
		level2_2_op1 = findOp1Op2Pattern(S1->getOp2PredByIndex(0), S2->getOp2PredByIndex(0), 1, candidates, debug);
		level2_2_op2 = findOp1Op2Pattern(S1->getOp2PredByIndex(0), S2->getOp2PredByIndex(0), 2, candidates, debug);
		costInside += convertPackCost(level2_2_op1);
		costInside += convertPackCost(level2_2_op2);
		costInside += convertUnpackCost(level2_2_op1);
		costInside += convertUnpackCost(level2_2_op2);
		saved++;
		if (Pc <= level2_2_op1 && level2_2_op1 <= Pf) {
			if (Pa != findOp1Op2Pattern(S1->getOp2PredByIndex(0)->getOp1PredByIndex(0), S2->getOp2PredByIndex(0)->getOp1PredByIndex(0), 1, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			if (Pa != findOp1Op2Pattern(S1->getOp2PredByIndex(0)->getOp1PredByIndex(0), S2->getOp2PredByIndex(0)->getOp1PredByIndex(0), 2, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			saved++;
		}
		if (Pc <= level2_2_op2 && level2_2_op2 <= Pf) {
			if (Pa != findOp1Op2Pattern(S1->getOp2PredByIndex(0)->getOp2PredByIndex(0), S2->getOp2PredByIndex(0)->getOp2PredByIndex(0), 1, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			if (Pa != findOp1Op2Pattern(S1->getOp2PredByIndex(0)->getOp2PredByIndex(0), S2->getOp2PredByIndex(0)->getOp2PredByIndex(0), 2, candidates, debug)) {
				costOutside.min ++;
				costOutside.max ++;
			}
			saved++;
		}
	}
}

enum Pattern CandidatePattern::findOp1Op2Pattern(Insts *S1, Insts *S2, unsigned int op, vector<pair<Insts *, Insts *> > candidates, bool debug) {
	unsigned int numS1Pred, numS2Pred;
	if (op == 1) {
		numS1Pred = S1->getNumOp1Preds();
		numS2Pred = S2->getNumOp1Preds();
	}
	else if (op == 2 ){
		numS1Pred = S1->getNumOp2Preds();
		numS2Pred = S2->getNumOp2Preds();
	}
	else {
		errs() << "Impossible Operand(" << op << ")\n";
	}

	if (S1 == S2) {
		errs() << "Impossible Pattern\n";
	}
	else if (isa<LoadInst>(S1->getInstByIndex(0))) {
		if (op == 1) {
			if (S1->getNumInsts() != 1 || S2->getNumInsts() != 1) {
				errs() << "WARNING: Not implemented yet " << numS1Pred << "," << numS2Pred << "\n";
				S1->dump(); S2->dump();
				return Pb;
			}
			AddrGEP *S1addr = new AddrGEP(dyn_cast<LoadInst>(S1->getInstByIndex(0)));
			AddrGEP *S2addr = new AddrGEP(dyn_cast<LoadInst>(S2->getInstByIndex(0)));
			if (S1addr->isAdjacentAddr(S2addr, false))
				return Pa;
			else
				return Pb;
			delete S1addr;
			delete S2addr;
		}
		else {
			if (numS1Pred || numS2Pred)
				errs() << "Impossible Load Pattern\n";
			return Pa;
		}
	}
	else if (numS1Pred == 0 && numS2Pred == 0) {
		return Pa;
	}
	else if (numS1Pred > 1 || numS2Pred > 1) {
		errs() << "Should not appear for now\n";
		return Pg;
	}
	else if (numS1Pred == 0 || numS2Pred == 0) {
		if (debug) errs() << "DEBUG1";
		return Pb;
	}
	else if (numS1Pred == 1 && numS2Pred == 1) {
		Insts *S1Pred, *S2Pred;
		if (op == 1) {
			S1Pred = S1->getOp1PredByIndex(0);
			S2Pred = S2->getOp1PredByIndex(0);
		}
		else {
			S1Pred = S1->getOp2PredByIndex(0);
			S2Pred = S2->getOp2PredByIndex(0);
		}
		if (debug) errs() << "[" << S1Pred->getSerial()[0] << ", " << S2Pred->getSerial()[0] << "]\n";

		if (S1Pred == S2Pred) {
			return Pb;
		}
		else {
			if (S1Pred->getInstByIndex(0)->getOpcode() == S2Pred->getInstByIndex(0)->getOpcode()) {
				unsigned int j;
				for (j = 0; j < candidates.size(); j++)
					if ((candidates[j].first == S1Pred && candidates[j].second == S2Pred) || (candidates[j].first == S2Pred && candidates[j].second == S1Pred))
						break;
				if (j < candidates.size()) {
					if (S1Pred->getNumSuccs() == 0 || S2Pred->getNumSuccs() == 0) {
						errs() << "Impossible number of successes\n";
					}
					else if (S1Pred->getNumSuccs() == 1 && S2Pred->getNumSuccs() == 1) {
						if (S1Pred->getSuccByIndex(0)->getInstByIndex(0)->getOpcode() != S2Pred->getSuccByIndex(0)->getInstByIndex(0)->getOpcode())
							errs() << "Impossible opcode\n";
						return Pc;
					}
					else if (S1Pred->getNumSuccs() == 1 || S2Pred->getNumSuccs() == 1) {
						return Pe;
					}
					else if (S1Pred->getNumSuccs() == S2Pred->getNumSuccs()) {
						map<unsigned int, unsigned int> opCodeS1, opCodeS2;
						for (unsigned int i = 0; i < S1Pred->getNumSuccs(); i++) {
							if (opCodeS1.find(S1Pred->getSuccByIndex(i)->getInstByIndex(0)->getOpcode()) == opCodeS1.end())
								opCodeS1[S1Pred->getSuccByIndex(i)->getInstByIndex(0)->getOpcode()] = 0;
							else
								opCodeS1[S1Pred->getSuccByIndex(i)->getInstByIndex(0)->getOpcode()]++;
							if (opCodeS2.find(S2Pred->getSuccByIndex(i)->getInstByIndex(0)->getOpcode()) == opCodeS2.end())
								opCodeS2[S2Pred->getSuccByIndex(i)->getInstByIndex(0)->getOpcode()] = 0;
							else
								opCodeS2[S2Pred->getSuccByIndex(i)->getInstByIndex(0)->getOpcode()]++;
						}
						if (opCodeS1.size() == opCodeS2.size()) {
							map<unsigned int, unsigned int>::iterator I1, I2;
							for (I1 = opCodeS1.begin(), I2 = opCodeS2.begin(); I1 != opCodeS1.end(); I1++, I2++)
								if (I1->second != I2->second)
									break;
							if (I1 == opCodeS1.end())
								return Pd;
							else
								return Pf;
						}
						else
							return Pf;
					}
					else
						return Pf;
				}
				else
					return Pb;
			}
			else
				return Pb;
		}
	}
	else {
		errs() << "Impossible case (" << numS1Pred << ", " << numS2Pred << ")\n";
	}

	return P_NONE;
}

struct range CandidatePattern::convertPackCost(enum Pattern pattern) {
	switch (pattern) {
	case Pa: return packPa; break;
	case Pb: return packPb; break;
	case Pc: return packPc; break;
	case Pd: return packPd; break;
	case Pe: return packPe; break;
	case Pf: return packPf; break;
	case Pg: return packPg; break;
	default: return packPa; break;
	}
}

struct range CandidatePattern::convertUnpackCost(enum Pattern pattern) {
	switch (pattern) {
	case Pa: return unpackPa; break;
	case Pb: return unpackPb; break;
	case Pc: return unpackPc; break;
	case Pd: return unpackPd; break;
	case Pe: return unpackPe; break;
	case Pf: return unpackPf; break;
	case Pg: return unpackPg; break;
	default: return unpackPa; break;
	}
}

int CandidatePattern::getSaved() {
	return saved;
}

struct range CandidatePattern::getCostInside() {
	return costInside;
}

struct range CandidatePattern::getCostOutside() {
	return costOutside;
}

void CandidatePattern::dump() {
	errs() << level1_op1 << "," << level1_op2 << "-" << level2_1_op1 << "," << level2_1_op2 << "-" << level2_2_op1 << "," << level2_2_op2 << " (" << costInside.min << "-" << costInside.max << ", " << costOutside.min << "-" << costOutside.max << ", " << saved << ")\n";
}

void CandidatePattern::dumpPatternOnly() {
	errs() << level1_op1 << "," << level1_op2 << "-" << level2_1_op1 << "," << level2_1_op2 << "-" << level2_2_op1 << "," << level2_2_op2 << " ";
}

SmallIsomorphicChain::SmallIsomorphicChain(Insts *S1, Insts *S2, int argSaved, struct range argCostInside, struct range argCostOutside) {
	pairs.first = S1;
	pairs.second = S2;
	saved = argSaved;
	costInside = argCostInside;
	costOutside = argCostOutside;
	level.max = 0;
	level.min = 0;
	if (costInside.min <= saved) {
		if (costInside.max <= saved) {
			level.max = 1;
		}
		level.min = 1;
	}
	if (costInside.min+costOutside.min <= saved) {
		if (costInside.max+costOutside.max <= saved) {
			level.max = 2;
		}
		level.min = 2;
	}
}

Insts *SmallIsomorphicChain::getS1() {
	return pairs.first;
}

Insts *SmallIsomorphicChain::getS2() {
	return pairs.second;
}

int SmallIsomorphicChain::getSaved() {
	return saved;
}

struct range SmallIsomorphicChain::getCostInside() {
	return costInside;
}

struct range SmallIsomorphicChain::getCostOutside() {
	return costOutside;
}

struct range SmallIsomorphicChain::getLevel() {
	return level;
}

void SmallIsomorphicChain::dump() {
	errs() << "{" << pairs.first->getSerial()[0] << ", " << pairs.second->getSerial()[0] << "} " << " (" << costInside.min << "-" << costInside.max << ", " << costOutside.min << "-" << costOutside.max << ", " << saved << ")L" << level.max << "\n";
}

void SmallIsomorphicChain::dumpCostOnly() {
	errs() <<  "(" << costInside.min << "-" << costInside.max << ", " << costOutside.min << "-" << costOutside.max << ", " << saved << ")L" << level.max << " " ;
}


GlobalIsomorphicChain::GlobalIsomorphicChain(SmallIsomorphicChain *argRoot) {
	root = argRoot;
	averageMaxHeight = 0.0;
	averageMinHeight = 0.0;
	maxBenefit = 0;
	minBenefit = 0;
	maxHeight = 0;
	minHeight = 0;
	selectedSmallICs = 0;
	conflictSmallICs = 0;
	numComleteSmallICs = 0;
	numBeneficialSmallICs = 0;
	numHarmfulSmallICs = 0;
}

void GlobalIsomorphicChain::addSmallIC(SmallIsomorphicChain *sIC, unsigned int level) {
	unsigned int size = smallICs.size();
	if (smallICs.find(sIC) == smallICs.end()) {
		smallICs[sIC] = size;
		smallIC_maxHeight.push_back(level);
		smallIC_minHeight.push_back(level);
		allInstICs[sIC->getS1()] = 0;
		allInstICs[sIC->getS2()] = 0;
	}
	else {
		if (smallIC_maxHeight[smallICs[sIC]] < level)
			smallIC_maxHeight[smallICs[sIC]] = level;
		if (smallIC_minHeight[smallICs[sIC]] > level)
			smallIC_minHeight[smallICs[sIC]] = level;
		allInstICs[sIC->getS1()]++;
		allInstICs[sIC->getS2()]++;
	}
}

void GlobalIsomorphicChain::updateProperties(map<SmallIsomorphicChain *, unsigned int> selectedICs, map<Insts *, bool> allInstsCandidates) {
	unsigned int sumMax = 0;
	unsigned int sumMin = 0;
	maxHeight = 0;
	minHeight = 100;
	for (unsigned int i = 0; i < smallIC_maxHeight.size(); i++) {
		sumMax += smallIC_maxHeight[i];
		if (maxHeight < smallIC_maxHeight[i])
			maxHeight = smallIC_maxHeight[i];
	}
	for (unsigned int i = 0; i < smallIC_minHeight.size(); i++) {
		sumMin += smallIC_minHeight[i];
		if (minHeight > smallIC_minHeight[i])
			minHeight = smallIC_minHeight[i];
	}
	averageMaxHeight = ((double) sumMax) / ((double) smallIC_maxHeight.size());
	averageMinHeight = ((double) sumMin) / ((double) smallIC_minHeight.size());

	selectedSmallICs = 0;
	conflictSmallICs = 0;
	numComleteSmallICs = 0;
	numBeneficialSmallICs = 0;
	numHarmfulSmallICs = 0;

	maxBenefit = root->getSaved() - root->getCostInside().min;
	minBenefit = root->getSaved() - root->getCostInside().max;
	if (root->getLevel().max == 2)
		numComleteSmallICs++;
	else if (root->getLevel().max == 1)
		numBeneficialSmallICs++;
	else
		numHarmfulSmallICs++;
	for (map<SmallIsomorphicChain *, unsigned int>::iterator I = smallICs.begin(); I != smallICs.end(); I++) {
		if (I->first->getLevel().max == 2)
			numComleteSmallICs++;
		else if (I->first->getLevel().max == 1)
			numBeneficialSmallICs++;
		else
			numHarmfulSmallICs++;

		if (selectedICs.find(I->first) != selectedICs.end())
			selectedSmallICs++;
		else if (!allInstsCandidates[I->first->getS1()] || !allInstsCandidates[I->first->getS2()])
			conflictSmallICs++;

		maxBenefit += I->first->getSaved() - I->first->getCostInside().min;
		minBenefit += I->first->getSaved() - I->first->getCostInside().max;
	}
}

bool GlobalIsomorphicChain::isExist(SmallIsomorphicChain *sIC) {
	if (smallICs.find(sIC)==smallICs.end())
		return false;
	else
		return true;
}

bool GlobalIsomorphicChain::isConflict(SmallIsomorphicChain *sIC) {
	if (allInstICs.find(sIC->getS1())==allInstICs.end() && allInstICs.find(sIC->getS2())==allInstICs.end())
		return false;
	else
		return true;
}

SmallIsomorphicChain *GlobalIsomorphicChain::getRoot() {
	return root;
}

//map<SmallIsomorphicChain *, unsigned int> GlobalIsomorphicChain::getSmallICs() {
//	return smallICs;
//}

map<SmallIsomorphicChain *, unsigned int>::iterator GlobalIsomorphicChain::getSmallICsBegin() {
	return smallICs.begin();
}

map<SmallIsomorphicChain *, unsigned int>::iterator GlobalIsomorphicChain::getSmallICsEnd() {
	return smallICs.end();
}

unsigned int GlobalIsomorphicChain::getSmallICsSize() {
	return smallICs.size();
}

unsigned int GlobalIsomorphicChain::getSmallIC_maxHeight(unsigned int idx) {
	return smallIC_maxHeight[idx];
}

unsigned int GlobalIsomorphicChain::getSmallIC_minHeight(unsigned int idx) {
	return smallIC_minHeight[idx];
}

double GlobalIsomorphicChain::getAverageMaxHeight() {
	return averageMaxHeight;
}

double GlobalIsomorphicChain::getAverageMinHeight() {
	return averageMinHeight;
}

int GlobalIsomorphicChain::getMaxBenefit() {
	return maxBenefit;
}

int GlobalIsomorphicChain::getMinBenefit() {
	return minBenefit;
}

unsigned int GlobalIsomorphicChain::getMaxHeight() {
	return maxHeight;
}

unsigned int GlobalIsomorphicChain::getMinHeight() {
	return minHeight;
}

unsigned int GlobalIsomorphicChain::getSelectedSmallICs() {
	return selectedSmallICs;
}

unsigned int GlobalIsomorphicChain::getConflictSmallICs() {
	return conflictSmallICs;
}

unsigned int GlobalIsomorphicChain::getNumCompleteSmallICs() {
	return numComleteSmallICs;
}

unsigned int GlobalIsomorphicChain::getNumBeneficialSmallICs() {
	return numBeneficialSmallICs;
}

unsigned int GlobalIsomorphicChain::getNumHarmfulSmallICs() {
	return numHarmfulSmallICs;
}

void GlobalIsomorphicChain::dump(map<Insts *, struct range> allInstsHeight, map<Insts *, struct range> allInstsDepth) {
	errs() << "{" << root->getS1()->getSerial()[0] << ", " << root->getS2()->getSerial()[0] << "}";
	errs() << "L" << root->getLevel().max << root->getLevel().min << ": ";
	errs() << averageMaxHeight << "~" << averageMinHeight << ", ";
	errs() << maxBenefit << "~" << minBenefit << ", ";
	errs() << maxHeight << "~" << minHeight << " (";
	errs() << allInstsHeight[root->getS1()].max << "-" << allInstsHeight[root->getS1()].min << "-" << allInstsDepth[root->getS1()].max << "-" << allInstsDepth[root->getS1()].min << ", "
		   << allInstsHeight[root->getS2()].max << "-" << allInstsHeight[root->getS2()].min << "-" << allInstsDepth[root->getS2()].max << "-" << allInstsDepth[root->getS2()].min << ")\n";
}

void GlobalIsomorphicChain::dumpAll(map<Insts *, struct range> allInstsHeight, map<Insts *, struct range> allInstsDepth) {
	errs() << "{" << root->getS1()->getSerial()[0] << ", " << root->getS2()->getSerial()[0] << "}";
	errs() << "L" << root->getLevel().max << root->getLevel().min << ": ";
	errs() << averageMaxHeight << "~" << averageMinHeight << ", ";
	errs() << maxBenefit << "~" << minBenefit << ", ";
	errs() << maxHeight << "~" << minHeight << " (";
	errs() << allInstsHeight[root->getS1()].max << "-" << allInstsHeight[root->getS1()].min << "-" << allInstsDepth[root->getS1()].max << "-" << allInstsDepth[root->getS1()].min << ", "
		   << allInstsHeight[root->getS2()].max << "-" << allInstsHeight[root->getS2()].min << "-" << allInstsDepth[root->getS2()].max << "-" << allInstsDepth[root->getS2()].min << ") ";
	errs() << smallICs.size() << " selected: " << selectedSmallICs << "\n";
	for (map<SmallIsomorphicChain *, unsigned int>::iterator I = smallICs.begin(); I != smallICs.end(); I++) {
		errs() << "{" << I->first->getS1()->getSerial()[0] << ", " << I->first->getS2()->getSerial()[0] << "}" << "L" << I->first->getLevel().max << I->first->getLevel().min;
		errs() << " " << smallIC_maxHeight[I->second] << "~"<< smallIC_minHeight[I->second] << ", ";
		//errs() << " " << I->second << ", ";
	}
	errs() << "\n";
}

CandidatePairs::CandidatePairs() {
	selectedGlobalICs = 0;
}

void CandidatePairs::addHeight(Insts *current, map<Insts *, bool> *visited, int *max_height, int *min_height, int current_height) {
	if (visited->find(current) != visited->end())
		return;
	if (isa<PHINode>(current->getInstByIndex(0)))
		return;
	(*visited)[current] = true;

	if (*max_height < current_height)
		*max_height = current_height;
	if (!current->getNumSuccs() && current_height < *min_height)
		*min_height = current_height;

	for (unsigned int i = 0; i < current->getNumSuccs(); i++)
		addHeight(current->getSuccByIndex(i), visited, max_height, min_height, current_height+1);
}

void CandidatePairs::addDepth(Insts *current, map<Insts *, bool> *visited, int *max_depth, int *min_depth, int current_depth) {
	if (visited->find(current) != visited->end())
		return;
	(*visited)[current] = true;

	if (*max_depth < current_depth)
		*max_depth = current_depth;
	if (!current->getNumOp1Preds() && !current->getNumOp2Preds() && current_depth < *min_depth)
		*min_depth = current_depth;
	if (current->getNumOp1Preds() == 1) {
		if (isa<GetElementPtrInst>(current->getOp1PredByIndex(0)->getInstByIndex(0)))
			if (current_depth < *min_depth)
				*min_depth = current_depth;
	}
	if (current->getNumOp2Preds() == 1) {
		if (isa<GetElementPtrInst>(current->getOp2PredByIndex(0)->getInstByIndex(0)))
			if (current_depth < *min_depth)
				*min_depth = current_depth;
	}

	if (!isa<PHINode>(current->getInstByIndex(0))) {
		for (unsigned int i = 0; i < current->getNumOp1Preds(); i++)
			addDepth(current->getOp1PredByIndex(i), visited, max_depth, min_depth, current_depth+1);
		for (unsigned int i = 0; i < current->getNumOp2Preds(); i++)
			addDepth(current->getOp2PredByIndex(i), visited, max_depth, min_depth, current_depth+1);
	}
}

bool CandidatePairs::isSameAllHeightDepth(SmallIsomorphicChain *IC) {
	if (allInstsHeight[IC->getS1()].max != allInstsHeight[IC->getS2()].max)
		return false;
	if (allInstsHeight[IC->getS1()].min != allInstsHeight[IC->getS2()].min)
		return false;
	if (allInstsDepth[IC->getS1()].max != allInstsDepth[IC->getS2()].max)
		return false;
	if (allInstsDepth[IC->getS1()].min != allInstsDepth[IC->getS2()].min)
		return false;

	return true;
}

bool CandidatePairs::isEvenAllHeightDepth(SmallIsomorphicChain *IC) {
	if (allInstsHeight[IC->getS1()].max+allInstsHeight[IC->getS1()].min != allInstsHeight[IC->getS2()].max+allInstsHeight[IC->getS2()].min)
		return false;
	if (allInstsDepth[IC->getS1()].max+allInstsDepth[IC->getS1()].min != allInstsDepth[IC->getS2()].max+allInstsDepth[IC->getS2()].min)
		return false;

	return true;
}

/*void CandidatePairs::findAllPreds(Insts *current, map<Insts *, bool> *visited, unsigned int level, map<Insts *, unsigned int> *storage) {
	if (visited->find(current) != visited->end())
		return;
	(*visited)[current] = true;

	//if (current->getNumSuccs() != 1)
	//	return;

	if (allInstsSmallICs.find(current) != allInstsSmallICs.end()) {
		if (storage->find(current) != storage->end())
			errs() << "ERROR: should not be in the storage\n";
		(*storage)[current] = level;
	}

	if (!isa<PHINode>(current->getInstByIndex(0))) {
		for (unsigned int i = 0; i < current->getNumOp1Preds(); i++)
			findAllPreds(current->getOp1PredByIndex(i), visited, level+1, storage);
		for (unsigned int i = 0; i < current->getNumOp2Preds(); i++)
			findAllPreds(current->getOp2PredByIndex(i), visited, level+1, storage);
	}
}*/

void CandidatePairs::findPredIsomorphicChain(GlobalIsomorphicChain *root, SmallIsomorphicChain *current, unsigned int level, bool debug) {
	if (debug) errs() << "check current (" << current->getS1()->getSerial()[0] << ", " << current->getS2()->getSerial()[0] << ") <" << current->getS1()->getNumOp1Preds() << ", " << current->getS2()->getNumOp1Preds() << ", " << current->getS1()->getNumOp2Preds() << ", " << current->getS2()->getNumOp2Preds() << ">\n";
	if (current->getS1()->getNumOp1Preds() == 1 && current->getS2()->getNumOp1Preds() == 1) {
		if (current->getS1()->getOp1PredByIndex(0) != current->getS2()->getOp1PredByIndex(0)) {
			Insts *S1Pred = current->getS1()->getOp1PredByIndex(0);
			Insts *S2Pred = current->getS2()->getOp1PredByIndex(0);
			if (debug) errs() << "check1 opcode " << S1Pred->getInstByIndex(0)->getOpcode() << ", " << S2Pred->getInstByIndex(0)->getOpcode() << "\n";
			if (S1Pred->getInstByIndex(0)->getOpcode() == S2Pred->getInstByIndex(0)->getOpcode()) {
				if (debug) errs() << "check1 (" << S1Pred->getSerial()[0] << ", " << S2Pred->getSerial()[0] << ")\n";
				unsigned int j;
				for (j = 0; j < allCandidates.size(); j++)
					if ((allCandidates[j]->getS1() == S1Pred && allCandidates[j]->getS2() == S2Pred) || (allCandidates[j]->getS1() == S2Pred && allCandidates[j]->getS2() == S1Pred))
						break;
				if (j < allCandidates.size()) {
					if (debug) errs() << "check in1\n";
					if (root->isExist(allCandidates[j]) || !root->isConflict(allCandidates[j])) {
						if (debug) errs() << "check in2\n";
						root->addSmallIC(allCandidates[j], level);
						findPredIsomorphicChain(root, allCandidates[j], level+1, debug);
					}
				}
			}
		}
	}
	if (current->getS1()->getNumOp2Preds() == 1 && current->getS2()->getNumOp2Preds() == 1) {
		if (current->getS1()->getOp2PredByIndex(0) != current->getS2()->getOp2PredByIndex(0)) {
			Insts *S1Pred = current->getS1()->getOp2PredByIndex(0);
			Insts *S2Pred = current->getS2()->getOp2PredByIndex(0);
			if (debug) errs() << "check2 opcode " << S1Pred->getInstByIndex(0)->getOpcode() << ", " << S2Pred->getInstByIndex(0)->getOpcode() << "\n";
			if (S1Pred->getInstByIndex(0)->getOpcode() == S2Pred->getInstByIndex(0)->getOpcode()) {
				if (debug) errs() << "check2 (" << S1Pred->getSerial()[0] << ", " << S2Pred->getSerial()[0] << ")\n";
				unsigned int j;
				for (j = 0; j < allCandidates.size(); j++)
					if ((allCandidates[j]->getS1() == S1Pred && allCandidates[j]->getS2() == S2Pred) || (allCandidates[j]->getS1() == S2Pred && allCandidates[j]->getS2() == S1Pred))
						break;
				if (j < allCandidates.size()) {
					if (debug) errs() << "check in1\n";
					if (root->isExist(allCandidates[j]) || !root->isConflict(allCandidates[j])) {
						if (debug) errs() << "check in2\n";
						root->addSmallIC(allCandidates[j], level);
						findPredIsomorphicChain(root, allCandidates[j], level+1, debug);
					}
				}
			}
		}
	}
	if (debug) errs() << "return\n";
}

void CandidatePairs::addCandidate(Insts *S1, Insts *S2) {
	struct range height, depth;
	map<Insts *, bool> visited;

	if (allInstsHeight.find(S1) == allInstsHeight.end()) {
		if (allInstsDepth.find(S1) != allInstsDepth.end())
			errs() << "Error: S1 should not be on Depth also\n";
		height.max = 0;
		height.min = 10000;
		depth.max = 0;
		depth.min = 10000;
		addHeight(S1, &visited, &height.max, &height.min, 0);
		visited.clear();
		addDepth(S1, &visited, &depth.max, &depth.min, 0);
		visited.clear();
		allInstsHeight[S1].max = height.max;
		allInstsHeight[S1].min = height.min;
		allInstsDepth[S1].max = depth.max;
		allInstsDepth[S1].min = depth.min;
	}
	if (allInstsHeight.find(S2) == allInstsHeight.end()) {
		if (allInstsDepth.find(S2) != allInstsDepth.end())
			errs() << "Error: S2 should not be on Depth also\n";
		height.max = 0;
		height.min = 10000;
		depth.max = 0;
		depth.min = 10000;
		addHeight(S2, &visited, &height.max, &height.min, 0);
		visited.clear();
		addDepth(S2, &visited, &depth.max, &depth.min, 0);
		visited.clear();
		allInstsHeight[S2].max = height.max;
		allInstsHeight[S2].min = height.min;
		allInstsDepth[S2].max = depth.max;
		allInstsDepth[S2].min = depth.min;
	}
	pre_candidates.push_back(make_pair(S1, S2));
	//candidates.push_back(make_pair(make_pair(S1, S2), CandidatePattern(S1, S2)));
}

void CandidatePairs::findPatternCandidate() {
	for (unsigned int i = 0; i < pre_candidates.size(); i++)
		candidates.push_back(make_pair(make_pair(pre_candidates[i].first, pre_candidates[i].second), CandidatePattern(pre_candidates[i].first, pre_candidates[i].second, pre_candidates)));
}

void CandidatePairs::findSmallIsomorphicChain() {
	for (unsigned int i = 0; i < candidates.size(); i++) {
		SmallIsomorphicChain *sICs = new SmallIsomorphicChain(candidates[i].first.first, candidates[i].first.second, candidates[i].second.getSaved(), candidates[i].second.getCostInside(), candidates[i].second.getCostOutside());
		allCandidates.push_back(sICs);
		allInstsCandidates[sICs->getS1()] = true;
		allInstsCandidates[sICs->getS2()] = true;
	}
}

void CandidatePairs::findGlobalIsomorphicChain() {
	for (unsigned int i = 0; i < allCandidates.size(); i++) {
		if (0 < allCandidates[i]->getLevel().max) {
			//map<Insts *, bool> allInstsGlobalICs;
			//if (smallICs[i]->getS1()->getSerial()[0]==230 && smallICs[i]->getS2()->getSerial()[0]==577 )
			//	findPredIsomorphicChain(smallICs[i], smallICs[i], 1, &allInstsGlobalICs, true);
			//else
				GlobalIsomorphicChain *root = new GlobalIsomorphicChain(allCandidates[i]);
				globalICs[root] = true;

				//if (allCandidates[i]->getS1()->getSerial()[0]==91 && allCandidates[i]->getS2()->getSerial()[0]==254 )
				//	findPredIsomorphicChain(root, allCandidates[i], 1, true);
				//else
					findPredIsomorphicChain(root, allCandidates[i], 1, false);
			//allInstsGlobalICs.clear();
		}
	}
	//findGlobalIsomorphicChainAverageHeight();
}

/*void CandidatePairs::findGlobalIsomorphicChainAverageHeight(){
	for (map<SmallIsomorphicChain *, map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> > >::iterator I = globalICs.begin(); I != globalICs.end(); I++) {
		globalICsNumCSmallICs[I->first] = 0;
		globalICsNumBSmallICs[I->first] = 0;
		globalICsNumHSmallICs[I->first] = 0;
		if (I->first->getLevel().max == 2)
			globalICsNumCSmallICs[I->first]++;
		else if (I->first->getLevel().max == 1)
			globalICsNumBSmallICs[I->first]++;
		else
			globalICsNumHSmallICs[I->first]++;

		float max_sum = 0.0;
		float min_sum = 0.0;
		unsigned int sizeChain = 0;
		unsigned int max_heightMax = 0;
		unsigned int max_heightMin = 0;
		int max_benefit = I->first->getSaved() - I->first->getCostInside().max;
		int min_benefit = I->first->getSaved() - I->first->getCostInside().min;
		for (map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> >::iterator II = I->second.begin(); II != I->second.end(); II++) {
			globalICsNumCSmallICs[II->first] = 0;
			globalICsNumBSmallICs[II->first] = 0;
			globalICsNumHSmallICs[II->first] = 0;
			if (I->first->getLevel().max == 2)
				globalICsNumCSmallICs[II->first]++;
			else if (I->first->getLevel().max == 1)
				globalICsNumBSmallICs[II->first]++;
			else
				globalICsNumHSmallICs[II->first]++;

			max_sum += (float) II->second.first;
			min_sum += (float) II->second.second;
			sizeChain++;
			if (max_heightMax < II->second.first)
				max_heightMax = II->second.first;
			if (max_heightMin < II->second.second)
				max_heightMin = II->second.second;
			max_benefit += II->first->getSaved() - II->first->getCostInside().max;
			min_benefit += II->first->getSaved() - II->first->getCostInside().min;
		}
		if (sizeChain) {
			globalICsAverageHeight[I->first].first = max_sum / (float) sizeChain;
			globalICsAverageHeight[I->first].second = min_sum / (float) sizeChain;
		}
		else {
			globalICsAverageHeight[I->first].first = 0;
			globalICsAverageHeight[I->first].second = 0;
		}
		globalICsHeight[I->first] = make_pair(max_heightMax, max_heightMin);
		globalICsBenefit[I->first] = make_pair(max_benefit, min_benefit);
	}
}

void CandidatePairs::findGlobalIsomorphicChainSelectedICs() {
	for (map<SmallIsomorphicChain *, map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> > >::iterator I = globalICs.begin(); I != globalICs.end(); I++) {
		if (allInstsCandidates[I->first->getS1()] && allInstsCandidates[I->first->getS2()]) {
			unsigned int numSelectedICs = 0;
			unsigned int numSelectedInstsTaken = 0;
			for (map<SmallIsomorphicChain *, pair<unsigned int, unsigned int> >::iterator II = I->second.begin(); II != I->second.end(); II++) {
				if (selectedICs.find(II->first) == selectedICs.end()) {
					if (!allInstsCandidates[II->first->getS1()])
						numSelectedInstsTaken++;
					if (!allInstsCandidates[II->first->getS2()])
						numSelectedInstsTaken++;
				}
				else
					numSelectedICs++;
			}
			globalICsSelectedICs[I->first] = make_pair(numSelectedICs, numSelectedInstsTaken);
		}
	}
}*/

void CandidatePairs::selectIsomorphicChain(unsigned int config) {
	errs() << "<DEBUG (selectICs)>\n";
	while (1) {
		//int max_height = 0;
		unsigned int max_selectedICs = 0;
		unsigned int max_globalICsize = 0;
		unsigned int max_BC = 0;
		unsigned int max_C = 0;
		unsigned int min_H = 100;
		unsigned int max_globalIClength = 0;
		unsigned int max_globalICHeight = 0;
		int max_globalBenefit = -100;
		int min_globalBenefit = -100;
		float max_globalICAverageHeight = 0.0;
		float min_globalICAverageHeight = 100.0;
		GlobalIsomorphicChain *max_IC;

		//findGlobalIsomorphicChainSelectedICs();
		for (map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++)
			if (I->second)
				I->first->updateProperties(selectedICs, allInstsCandidates);

		for (map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++)
			if(!I->first->getSmallICsSize() || I->first->getConflictSmallICs())
				I->second = false;

		for (map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++) {
			if (I->second)
			if (allInstsCandidates[I->first->getRoot()->getS1()] && allInstsCandidates[I->first->getRoot()->getS2()]) {
				if (config == 1) {
					if (max_selectedICs < I->first->getSelectedSmallICs()) {
						max_selectedICs = I->first->getSelectedSmallICs();
						max_globalBenefit = I->first->getMaxBenefit();
						min_globalBenefit = I->first->getMinBenefit();
						max_globalICAverageHeight = I->first->getAverageMaxHeight();
						min_globalICAverageHeight = I->first->getAverageMinHeight();
						max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
						min_H = I->first->getNumHarmfulSmallICs();
						max_C = I->first->getNumCompleteSmallICs();
						max_globalICsize = I->first->getSmallICsSize();
						max_IC = I->first;
					}
					else if (max_selectedICs == I->first->getSelectedSmallICs()) {
						if (max_globalBenefit < I->first->getMaxBenefit()) {
							max_globalBenefit = I->first->getMaxBenefit();
							min_globalBenefit = I->first->getMinBenefit();
							max_globalICAverageHeight = I->first->getAverageMaxHeight();
							min_globalICAverageHeight = I->first->getAverageMinHeight();
							max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
							min_H = I->first->getNumHarmfulSmallICs();
							max_C = I->first->getNumCompleteSmallICs();
							max_globalICsize = I->first->getSmallICsSize();
							max_IC = I->first;
						}
						else if (max_globalBenefit < I->first->getMaxBenefit()) {
							if (min_globalBenefit < I->first->getMinBenefit()) {
								min_globalBenefit = I->first->getMinBenefit();
								max_globalICAverageHeight = I->first->getAverageMaxHeight();
								min_globalICAverageHeight = I->first->getAverageMinHeight();
								max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
								min_H = I->first->getNumHarmfulSmallICs();
								max_C = I->first->getNumCompleteSmallICs();
								max_globalICsize = I->first->getSmallICsSize();
								max_IC = I->first;
							}
							else if (min_globalBenefit == I->first->getMinBenefit()) {
								if (max_globalICAverageHeight < I->first->getAverageMaxHeight()) {
									max_globalICAverageHeight = I->first->getAverageMaxHeight();
									min_globalICAverageHeight = I->first->getAverageMinHeight();
									max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
									min_H = I->first->getNumHarmfulSmallICs();
									max_C = I->first->getNumCompleteSmallICs();
									max_globalICsize = I->first->getSmallICsSize();
									max_IC = I->first;
								}
								else if (max_globalICAverageHeight == I->first->getAverageMaxHeight()) {
									if (min_globalICAverageHeight > I->first->getAverageMinHeight()) {
										min_globalICAverageHeight = I->first->getAverageMinHeight();
										max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
										min_H = I->first->getNumHarmfulSmallICs();
										max_C = I->first->getNumCompleteSmallICs();
										max_globalICsize = I->first->getSmallICsSize();
										max_IC = I->first;
									}
									else if (min_globalICAverageHeight == I->first->getAverageMinHeight()) {
										if (max_BC < I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs()) {
											max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
											min_H = I->first->getNumHarmfulSmallICs();
											max_C = I->first->getNumCompleteSmallICs();
											max_globalICsize = I->first->getSmallICsSize();
											max_IC = I->first;
										}
										else if (max_BC == I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs()) {
											if (min_H > I->first->getNumHarmfulSmallICs()) {
												min_H = I->first->getNumHarmfulSmallICs();
												max_C = I->first->getNumCompleteSmallICs();
												max_globalICsize = I->first->getSmallICsSize();
												max_IC = I->first;
											}
											else if (min_H == I->first->getNumHarmfulSmallICs()) {
												if (max_C < I->first->getNumCompleteSmallICs()) {
													max_C = I->first->getNumCompleteSmallICs();
													max_globalICsize = I->first->getSmallICsSize();
													max_IC = I->first;
												}
												else if (max_C == I->first->getNumCompleteSmallICs()) {
													if (max_globalICsize < I->first->getSmallICsSize()) {
														max_globalICsize = I->first->getSmallICsSize();
														max_IC = I->first;
													}
												}
											}
										}
									}
								}
							}
						}
					}
					//icvec2
					/*if (max_selectedICs < I->first->getSelectedSmallICs()) {
						max_selectedICs = I->first->getSelectedSmallICs();
						max_globalBenefit = I->first->getMaxBenefit();
						min_globalBenefit = I->first->getMinBenefit();
						max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
						min_H = I->first->getNumHarmfulSmallICs();
						max_C = I->first->getNumCompleteSmallICs();
						max_globalICsize = I->first->getSmallICsSize();
						max_globalICAverageHeight = I->first->getAverageMaxHeight();
						min_globalICAverageHeight = I->first->getAverageMinHeight();
						max_IC = I->first;
					}
					else if (max_selectedICs == I->first->getSelectedSmallICs()) {
						if (max_globalBenefit < I->first->getMaxBenefit()) {
							max_globalBenefit = I->first->getMaxBenefit();
							min_globalBenefit = I->first->getMinBenefit();
							max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
							min_H = I->first->getNumHarmfulSmallICs();
							max_C = I->first->getNumCompleteSmallICs();
							max_globalICsize = I->first->getSmallICsSize();
							max_globalICAverageHeight = I->first->getAverageMaxHeight();
							min_globalICAverageHeight = I->first->getAverageMinHeight();
							max_IC = I->first;
						}
						else if (max_globalBenefit < I->first->getMaxBenefit()) {
							if (min_globalBenefit < I->first->getMinBenefit()) {
								min_globalBenefit = I->first->getMinBenefit();
								max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
								min_H = I->first->getNumHarmfulSmallICs();
								max_C = I->first->getNumCompleteSmallICs();
								max_globalICsize = I->first->getSmallICsSize();
								max_globalICAverageHeight = I->first->getAverageMaxHeight();
								min_globalICAverageHeight = I->first->getAverageMinHeight();
								max_IC = I->first;
							}
							else if (min_globalBenefit == I->first->getMinBenefit()) {
								if (max_BC < I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs()) {
									max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
									min_H = I->first->getNumHarmfulSmallICs();
									max_C = I->first->getNumCompleteSmallICs();
									max_globalICsize = I->first->getSmallICsSize();
									max_globalICAverageHeight = I->first->getAverageMaxHeight();
									min_globalICAverageHeight = I->first->getAverageMinHeight();
									max_IC = I->first;
								}
								else if (max_BC == I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs()) {
									if (min_H > I->first->getNumHarmfulSmallICs()) {
										min_H = I->first->getNumHarmfulSmallICs();
										max_C = I->first->getNumCompleteSmallICs();
										max_globalICsize = I->first->getSmallICsSize();
										max_globalICAverageHeight = I->first->getAverageMaxHeight();
										min_globalICAverageHeight = I->first->getAverageMinHeight();
										max_IC = I->first;
									}
									else if (min_H == I->first->getNumHarmfulSmallICs()) {
										if (max_C < I->first->getNumCompleteSmallICs()) {
											max_C = I->first->getNumCompleteSmallICs();
											max_globalICsize = I->first->getSmallICsSize();
											max_globalICAverageHeight = I->first->getAverageMaxHeight();
											min_globalICAverageHeight = I->first->getAverageMinHeight();
											max_IC = I->first;
										}
										else if (max_C == I->first->getNumCompleteSmallICs()) {
											if (max_globalICsize < I->first->getSmallICsSize()) {
												max_globalICsize = I->first->getSmallICsSize();
												max_globalICAverageHeight = I->first->getAverageMaxHeight();
												min_globalICAverageHeight = I->first->getAverageMinHeight();
												max_IC = I->first;
											}
											else if (max_globalICsize == I->first->getSmallICsSize()) {
												if (max_globalICAverageHeight < I->first->getAverageMaxHeight()) {
													max_globalICAverageHeight = I->first->getAverageMaxHeight();
													min_globalICAverageHeight = I->first->getAverageMinHeight();
													max_IC = I->first;
												}
												else if (max_globalICAverageHeight == I->first->getAverageMaxHeight()) {
													if (min_globalICAverageHeight > I->first->getAverageMinHeight()) {
														min_globalICAverageHeight = I->first->getAverageMinHeight();
														max_IC = I->first;
													}
												}
											}
										}
									}
								}
							}
						}
					}*/
					//icvec
					/*if (max_globalICsize < I->first->getSmallICsSize()) {
						max_globalICsize = I->first->getSmallICsSize();
						max_globalICAverageHeight = I->first->getAverageMaxHeight();
						min_globalICAverageHeight = I->first->getAverageMinHeight();
						max_globalBenefit = I->first->getMaxBenefit();
						max_IC = I->first;
					}
					else if (max_globalICsize == I->first->getSmallICsSize()) {
						if (max_globalICAverageHeight < I->first->getAverageMaxHeight()) {
							max_globalICAverageHeight = I->first->getAverageMaxHeight();
							min_globalICAverageHeight = I->first->getAverageMinHeight();
							max_globalBenefit = I->first->getMaxBenefit();
							max_IC = I->first;
						}
						if (max_globalICAverageHeight == I->first->getAverageMaxHeight()) {
							if (min_globalICAverageHeight > I->first->getAverageMinHeight()) {
								min_globalICAverageHeight = I->first->getAverageMinHeight();
								max_globalBenefit = I->first->getMaxBenefit();
								max_IC = I->first;
							}
							else if (min_globalICAverageHeight == I->first->getAverageMinHeight()) {
								if (max_globalBenefit < I->first->getMaxBenefit()) {
									max_globalBenefit = I->first->getMaxBenefit();
									max_IC = I->first;
								}
							}
						}
					}*/
				}
				else {
					if (isEvenAllHeightDepth(I->first->getRoot())) {
						if (max_selectedICs < I->first->getSelectedSmallICs()) {
							max_selectedICs = I->first->getSelectedSmallICs();
							max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
							min_H = I->first->getNumHarmfulSmallICs();
							max_C = I->first->getNumCompleteSmallICs();
							max_globalICsize = I->first->getSmallICsSize();
							max_globalICAverageHeight = I->first->getAverageMaxHeight();
							min_globalICAverageHeight = I->first->getAverageMinHeight();
							max_globalBenefit = I->first->getMaxBenefit();
							max_IC = I->first;
						}
						else if (max_selectedICs == I->first->getSelectedSmallICs()) {
							if (max_BC < I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs()) {
								max_BC = I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs();
								min_H = I->first->getNumHarmfulSmallICs();
								max_C = I->first->getNumCompleteSmallICs();
								max_globalICsize = I->first->getSmallICsSize();
								max_globalICAverageHeight = I->first->getAverageMaxHeight();
								min_globalICAverageHeight = I->first->getAverageMinHeight();
								max_globalBenefit = I->first->getMaxBenefit();
								max_IC = I->first;
							}
							else if (max_BC == I->first->getNumCompleteSmallICs()+I->first->getNumBeneficialSmallICs()) {
								if (min_H > I->first->getNumHarmfulSmallICs()) {
									min_H = I->first->getNumHarmfulSmallICs();
									max_C = I->first->getNumCompleteSmallICs();
									max_globalICsize = I->first->getSmallICsSize();
									max_globalICAverageHeight = I->first->getAverageMaxHeight();
									min_globalICAverageHeight = I->first->getAverageMinHeight();
									max_globalBenefit = I->first->getMaxBenefit();
									max_IC = I->first;
								}
								else if (min_H == I->first->getNumHarmfulSmallICs()) {
									if (max_C < I->first->getNumCompleteSmallICs()) {
										max_C = I->first->getNumCompleteSmallICs();
										max_globalICsize = I->first->getSmallICsSize();
										max_globalICAverageHeight = I->first->getAverageMaxHeight();
										min_globalICAverageHeight = I->first->getAverageMinHeight();
										max_globalBenefit = I->first->getMaxBenefit();
										max_IC = I->first;
									}
									else if (max_C == I->first->getNumCompleteSmallICs()) {
										if (max_globalICsize < I->first->getSmallICsSize()) {
											max_globalICsize = I->first->getSmallICsSize();
											max_globalICAverageHeight = I->first->getAverageMaxHeight();
											min_globalICAverageHeight = I->first->getAverageMinHeight();
											max_globalBenefit = I->first->getMaxBenefit();
											max_IC = I->first;
										}
										else if (max_globalICsize == I->first->getSmallICsSize()) {
											if (max_globalICAverageHeight < I->first->getAverageMaxHeight()) {
												max_globalICAverageHeight = I->first->getAverageMaxHeight();
												min_globalICAverageHeight = I->first->getAverageMinHeight();
												max_globalBenefit = I->first->getMaxBenefit();
												max_IC = I->first;
											}
											if (max_globalICAverageHeight == I->first->getAverageMaxHeight()) {
												if (min_globalICAverageHeight > I->first->getAverageMinHeight()) {
													min_globalICAverageHeight = I->first->getAverageMinHeight();
													max_globalBenefit = I->first->getMaxBenefit();
													max_IC = I->first;
												}
												else if (min_globalICAverageHeight == I->first->getAverageMinHeight()) {
													if (max_globalBenefit < I->first->getMaxBenefit()) {
														max_globalBenefit = I->first->getMaxBenefit();
														max_IC = I->first;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		if (!max_globalICsize)
			break;

		if (selectedICs.find(max_IC->getRoot()) != selectedICs.end())
			errs() << "ERROR: max_IC should not be in selectedICs";
		selectedICs[max_IC->getRoot()] = 1;
		allInstsCandidates[max_IC->getRoot()->getS1()] = false;
		allInstsCandidates[max_IC->getRoot()->getS2()] = false;
		for (map<SmallIsomorphicChain *, unsigned int>::iterator II = max_IC->getSmallICsBegin(); II != max_IC->getSmallICsEnd(); II++) {
			if (selectedICs.find(II->first) == selectedICs.end())
				selectedICs[II->first] = 1;
			else
				selectedICs[II->first]++;
			allInstsCandidates[II->first->getS1()] = false;
			allInstsCandidates[II->first->getS2()] = false;
		}
		selectedGlobalICs++;
		finalSelectedSeeds.push_back(max_IC->getRoot());
		max_IC->dumpAll(allInstsHeight, allInstsDepth);
	}
	errs() << "<DEBUG (select remain Level4 SmallICs)>\n";
	for (unsigned int i = 0; i < allCandidates.size(); i++)
		if (1 < allCandidates[i]->getLevel().max)
			if (allInstsCandidates[allCandidates[i]->getS1()] && allInstsCandidates[allCandidates[i]->getS2()]) {
				allCandidates[i]->dump();
				//selectedICs[allCandidates[i]] = 1;
			}
	errs() << "\n";

	errs() << "<DEBUG (Remain GlobaICs)>\n";
	for (map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++) {
		if (I->second)
		if (selectedICs.find(I->first->getRoot()) == selectedICs.end()) {
		//if (allInstsCandidates[I->first->getS1()] && allInstsCandidates[I->first->getS2()]) {
			if (isEvenAllHeightDepth(I->first->getRoot())) {
				I->first->dumpAll(allInstsHeight, allInstsDepth);
			}
		}
	}
	errs() << "\n";
	errs() << "<DEBUG (Remain Candidates)>\n";
	for (unsigned int i = 0; i < allCandidates.size(); i++) {
		if (allInstsCandidates[allCandidates[i]->getS1()] && allInstsCandidates[allCandidates[i]->getS2()]) {
			errs() << "{" << allCandidates[i]->getS1()->getSerial()[0] << ", " << allCandidates[i]->getS2()->getSerial()[0] << "} ";
		}
	}
	errs() << "\n";

	for (map<SmallIsomorphicChain *, unsigned int>::iterator I = selectedICs.begin(); I != selectedICs.end(); I++)
		finalSelectedICs.push_back(I->first);
}

unsigned int CandidatePairs::getNumSmallIsomorphicChain() {
	return allCandidates.size();
}

SmallIsomorphicChain *CandidatePairs::getSmallIsomorphicChianbyIdx(unsigned int idx) {
	return allCandidates[idx];
}

unsigned int CandidatePairs::getNumSelecteds() {
	return finalSelectedICs.size();
}

SmallIsomorphicChain *CandidatePairs::getSelectedsbyIdx(unsigned int idx) {
	return finalSelectedICs[idx];
}

unsigned int CandidatePairs::getNumSelectedSeeds() {
	return finalSelectedSeeds.size();
}

SmallIsomorphicChain *CandidatePairs::getSelectedSeedsbyIdx(unsigned int idx) {
	return finalSelectedSeeds[idx];
}

void CandidatePairs::dumpCandidates() {
	errs() << "<DEBUG (Candidates)>\n";
	errs() << "Number of candidates: " << candidates.size() << "\n";
	for (unsigned int i = 0; i < candidates.size(); i++) {
		errs() << "{" << candidates[i].first.first->getSerial()[0] << ", " << candidates[i].first.second->getSerial()[0] << "} ";
		candidates[i].second.dump();
	}
}

void CandidatePairs::dumpSmallICs() {
	errs() << "<DEBUG (SmallICs)>\n";
	for (unsigned int i = 0; i < allCandidates.size(); i++) {
		if (0 < allCandidates[i]->getLevel().max) {
			errs() << "{" << allCandidates[i]->getS1()->getSerial()[0] << ", " << allCandidates[i]->getS2()->getSerial()[0] << "} ";
			allCandidates[i]->dumpCostOnly();
			errs() << allInstsHeight[allCandidates[i]->getS1()].max << "-" << allInstsHeight[allCandidates[i]->getS1()].min << "-" << allInstsDepth[allCandidates[i]->getS1()].max << "-" << allInstsDepth[allCandidates[i]->getS1()].min << ", "
				   << allInstsHeight[allCandidates[i]->getS2()].max << "-" << allInstsHeight[allCandidates[i]->getS2()].min << "-" << allInstsDepth[allCandidates[i]->getS2()].max << "-" << allInstsDepth[allCandidates[i]->getS2()].min << "\n";
		}
	}
}

void CandidatePairs::dumpGlobalICs() {
	errs() << "<DEBUG (GlobalICs)>\n";

	for (map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++)
		if (I->second)
			I->first->updateProperties(selectedICs, allInstsCandidates);

	for (map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++)
		I->first->dumpAll(allInstsHeight, allInstsDepth);
}

void CandidatePairs::dumpSummary() {
	unsigned int smallICLevel1 = 0;
	unsigned int smallICLevel2 = 0;
	unsigned int smallICLevel3 = 0;
	unsigned int smallICLevel4 = 0;
	unsigned int nGlobalICs = 0;
	for (unsigned int i = 0; i < allCandidates.size(); i++) {
		if (0 < allCandidates[i]->getLevel().min) {
			smallICLevel1++;
			if (1 < allCandidates[i]->getLevel().min) {
				smallICLevel3++;
			}
		}
		if (0 < allCandidates[i]->getLevel().max) {
			smallICLevel2++;
			if (1 < allCandidates[i]->getLevel().max) {
				smallICLevel4++;
			}
		}
	}
	for(map<GlobalIsomorphicChain *, bool>::iterator I = globalICs.begin(); I != globalICs.end(); I++)
		if (I->first->getSmallICsSize())
			nGlobalICs++;
	//errs() << "Number of precandies: " << pre_candidates.size() << "\n";
	errs() << "Number of candidates: " << candidates.size() << "\n";
	errs() << "Number of SC(Level1): " << smallICLevel1 << "\n";
	errs() << "Number of SC(Level2): " << smallICLevel2 << "\n";
	errs() << "Number of SC(Level3): " << smallICLevel3 << "\n";
	errs() << "Number of SC(Level4): " << smallICLevel4 << "\n";
	errs() << "Number of GC        : " << nGlobalICs << "\n";
	errs() << "Number of SelectedGC: " << selectedGlobalICs << "\n";
	errs() << "Number of SelectedSC: " << finalSelectedICs.size() << "\n";
}
