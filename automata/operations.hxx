#ifndef _bracz_train_automata_operations_hxx_
#define _bracz_train_automata_operations_hxx_

#include <initializer_list>

#include "system.hxx"
#include "../cs/src/automata_defs.h"

namespace automata {

#define GET_INVERSE_MASK(x) ( (~(x)) & ((x) - 1) ) 

struct StateRef {
    StateRef(int id) {
        assert((id & GET_INVERSE_MASK(_IF_STATE_MASK)) == id);
        state = id;
    }
    int state;
};

class OpCallback {
 public:
  virtual void Run(Automata::Op* op) = 0;
  virtual ~OpCallback() {};
};

template<class T, class P1> class OpCallback1 : public OpCallback {
 public:
  typedef void (T::*fptr_t)(P1 p1, Automata::Op* op);

  OpCallback1(T* parent, P1 p1, fptr_t fptr)
      : parent_(parent), p1_(p1), fptr_(fptr) {}
  virtual ~OpCallback1() {}
  virtual void Run(Automata::Op* op) {
    (parent_->*fptr_)(p1_, op);
  }

 private:
  T* parent_;
  P1 p1_;
  fptr_t fptr_;
};

template<class T, class P1> OpCallback1<T, P1> NewCallback(T* obj, typename OpCallback1<T, P1>::fptr_t fptr, P1 p1) {
  return OpCallback1<T, P1>(obj, p1, fptr);
}

class Automata::Op {
public:
    Op(Automata* parent)
        : parent_(parent), output_(parent->output_) {}

    Op(Automata* parent, string* output)
        : parent_(parent), output_(output) {}

    ~Op() {
        CreateOperation(output_, ifs_, acts_);
    }

    static void CreateOperation(string* output,
                                const vector<uint8_t>& ifs,
                                const vector<uint8_t>& acts) {
        assert(ifs.size() < 16);
        assert(acts.size() < 16);
        if (!output) return;
        uint8_t hdr = 0;
        hdr = (acts.size() & 0xf) << 4;
        hdr |= ifs.size() & 0xf;
        output->push_back(hdr);
        output->append((char*)ifs.data(), ifs.size());
        output->append((char*)acts.data(), acts.size());
    }

    Op& IfState(StateRef& state) {
        ifs_.push_back(_IF_STATE | state.state);
        return *this;
    }

    Op& IfReg0(const Automata::LocalVariable& var) {
        uint8_t v = _IF_REG;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& IfReg1(const Automata::LocalVariable& var) {
        uint8_t v = _IF_REG | _REG_1;
        if (output_) v |= var.GetId();
        ifs_.push_back(v);
        return *this;
    }

    Op& ActReg0(Automata::LocalVariable* var) {
        uint8_t v = _ACT_REG;
        if (output_) v |= var->GetId();
        acts_.push_back(v);
        return *this;
    }

    Op& ActReg1(Automata::LocalVariable* var) {
        uint8_t v = _ACT_REG | _REG_1;
        if (output_) v |= var->GetId();
        acts_.push_back(v);
        return *this;
    }

    Op& ActState(StateRef& state) {
        acts_.push_back(_ACT_STATE | state.state);
        return *this;
    }

    Op& IfTimerNotDone() {
        return IfReg1(parent_->timer_bit_);
    }

    Op& IfTimerDone() {
        return IfReg0(parent_->timer_bit_);
    }

    Op& ActTimer(int seconds) {
        assert((seconds & _ACT_TIMER_MASK) == 0);
        uint8_t v = _ACT_TIMER;
        v |= seconds;
        acts_.push_back(v);
        return *this;
    }
    
    Op& AddIf(uint8_t byte) {
        ifs_.push_back(byte);
        return *this;
    }
    Op& AddAct(uint8_t byte) {
        acts_.push_back(byte);
        return *this;
    }

  Op& RunCallback(OpCallback* cb) {
    if (cb) cb->Run(this);
    return *this;
  }

  typedef Op& (Op::*const_var_fn_t)(const Automata::LocalVariable&);
  typedef Op& (Op::*mutable_var_fn_t)(Automata::LocalVariable*);

  Op& Rept(mutable_var_fn_t fn,
           const std::initializer_list<GlobalVariable*>& vars) {
    for (auto v : vars) {
      (this->*fn)(parent()->ImportVariable(v));
    }
    return *this;
  }

  Op& Rept(const_var_fn_t fn,
           const std::initializer_list<const GlobalVariable*>& vars) {
    for (const GlobalVariable* v : vars) {
      (this->*fn)(parent()->ImportVariable(*v));
    }
    return *this;
  }

  Automata* parent() { return parent_; }

private:
    DISALLOW_COPY_AND_ASSIGN(Op);

    vector<uint8_t> ifs_;
    vector<uint8_t> acts_;

    Automata* parent_;
    // This may be NULL to indicate null output.
    string* output_;
};

/*Automata::Op Automata::Def() {
    return Op(this, output_);
    }*/

}  // namespace automata

#endif // _bracz_train_automata_operations_hxx_
