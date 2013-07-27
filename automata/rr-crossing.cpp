#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
using namespace std;

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"


#include "../cs/src/base.h"
//#define OFS_GLOBAL_BITS 24

using namespace automata;


StateRef StateInit(0);
StateRef StateBase(1);

StateRef StateUser1(2);
StateRef StateUser2(3);


Board brd;

EventBasedVariable led(&brd,
                       0x0502010202650512ULL,
                       0x0502010202650513ULL,
                       0, OFS_GLOBAL_BITS, 1);

EventBasedVariable intev(&brd,
                         0x0502010202650022ULL,
                         0x0502010202650023ULL,
                         0, OFS_GLOBAL_BITS, 3);

EventBasedVariable repev(&brd,
                         0x0502010202650032ULL,
                         0x0502010202650033ULL,
                         0, OFS_GLOBAL_BITS, 4);

EventBasedVariable vled1(&brd,
                         0x0502010202650040ULL,
                         0x0502010202650041ULL,
                         1, OFS_IOA, 1);
EventBasedVariable vled2(&brd,
                         0x0502010202650042ULL,
                         0x0502010202650043ULL,
                         1, OFS_IOA, 0);
EventBasedVariable vled3(&brd,
                         0x0502010202650060ULL,
                         0x0502010202650061ULL,
                         2, OFS_IOA, 1);
EventBasedVariable vled4(&brd,
                         0x0502010202650062ULL,
                         0x0502010202650063ULL,
                         2, OFS_IOA, 0);

EventBasedVariable inpb1(&brd,
                         0x0502010202650082ULL,
                         0x0502010202650083ULL,
                         1, OFS_IOB, 2);

EventBasedVariable inpb2(&brd,
                         0x0502010202650084ULL,
                         0x0502010202650085ULL,
                         2, OFS_IOB, 2);


DefAut(testaut, brd, {
        Def().IfState(StateInit).ActState(StateBase);
    });


DefAut(blinker, brd, {
        LocalVariable& repvar(ImportVariable(&led));
        LocalVariable& l1(ImportVariable(&vled1));
        LocalVariable& l2(ImportVariable(&vled3));
        Def().IfState(StateInit).ActState(StateUser1);
        Def().IfState(StateUser1).IfTimerDone().ActTimer(1).ActState(StateUser2);
        Def().IfState(StateUser2).IfTimerDone().ActTimer(1).ActState(StateUser1);
        Def().IfState(StateUser1).ActReg1(repvar);
        Def().IfState(StateUser2).ActReg0(repvar);

        Def().IfState(StateUser1).ActReg1(l1);
        Def().IfState(StateUser2).ActReg0(l1);

        Def().IfState(StateUser2).ActReg1(l2);
        Def().IfState(StateUser1).ActReg0(l2);

   });

DefAut(copier, brd, {
        LocalVariable& ledvar(ImportVariable(&led));
        LocalVariable& intvar(ImportVariable(&intev));
        Def().IfReg1(ledvar).ActReg1(intvar);
        Def().IfReg0(ledvar).ActReg0(intvar);
    });

DefAut(xcopier, brd, {
        LocalVariable& ledvar(ImportVariable(&vled4));
        LocalVariable& btnvar(ImportVariable(&inpb1));
        //DefCopy(btnvar, ledvar);
    });

DefAut(xcopier2, brd, {
        LocalVariable& ledvar(ImportVariable(&vled2));
        LocalVariable& btnvar(ImportVariable(&inpb2));
        //DefNCopy(btnvar, ledvar);
    });


int main(int argc, char** argv) {
    string output;
    brd.Render(&output);
    printf("const char automata_code[] = {\n  ");
    int c = 0;
    for (char t : output) {
      printf("0x%02x, ", (uint8_t)t);
      if (++c >= 12) {
        printf("\n  ", t);
        c = 0;
      }
    }
    printf("};\n");
    return 0;
};
