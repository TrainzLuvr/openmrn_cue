#include "control-logic.hxx"

namespace automata {

typedef Automata::LocalVariable LocalVariable;

void StraightTrack::SimulateAllOccupancy(Automata* aut) {
  auto* sim_occ = aut->ImportVariable(&simulated_occupancy_);
  auto* tmp = aut->ImportVariable(&tmp_seen_train_in_next_);
  auto side_b_release = NewCallback(
      this, &StraightTrack::ReleaseRouteCallback, side_b());
  auto side_a_release = NewCallback(
      this, &StraightTrack::ReleaseRouteCallback, side_a());
  SimulateOccupancy(aut,
                    sim_occ,
                    tmp,
                    aut->ImportVariable(route_set_ab_),
                    side_a(),
                    side_b(),
                    &side_b_release);
  SimulateOccupancy(aut,
                    sim_occ,
                    tmp,
                    aut->ImportVariable(route_set_ba_),
                    side_b(),
                    side_a(),
                    &side_a_release);
}

const CtrlTrackInterface* StraightTrack::FindOtherSide(
    const CtrlTrackInterface* s) {
  if (s == &side_a_) {
    return &side_b_;
  } else if (s == &side_b_) {
    return &side_a_;
  } else {
    extern bool could_not_find_opposide_side();
    HASSERT(false && could_not_find_opposide_side());
  }
  return NULL;
}



void SimulateOccupancy(Automata* aut,
                       Automata::LocalVariable* sim_occ,
                       Automata::LocalVariable* tmp_seen_train_in_next,
                       const Automata::LocalVariable& route_set,
                       CtrlTrackInterface* past_if,
                       CtrlTrackInterface* next_if,
                       OpCallback* release_cb) {
  // tmp_seen_train_in_next is 1 if we have seen a train in the next (close)
  // occupancy block.
  HASSERT(past_if);
  HASSERT(next_if);
  const LocalVariable& previous_occ = aut->ImportVariable(
      *past_if->binding()->LookupNextDetector());

  Def()
      .IfReg1(route_set)
      .IfReg1(previous_occ)
      .IfReg0(*sim_occ)
      .ActReg1(sim_occ)
      .ActReg0(tmp_seen_train_in_next);

  if (next_if->binding()->LookupFarDetector()) {
    //printf("Have far detector\n");
    const LocalVariable& next_trainlength_occ = aut->ImportVariable(
        *next_if->binding()->LookupFarDetector());
    // If the train has shown up in a far-away block, it has surely passed
    // through us.
    Def().IfReg1(route_set).IfReg1(next_trainlength_occ)
        .ActReg0(sim_occ)
        .RunCallback(release_cb);

  }

  if (next_if->binding()->LookupCloseDetector()) {
    //printf("Have close detector\n");
    const LocalVariable& next_close_occ = aut->ImportVariable(
        *next_if->binding()->LookupCloseDetector());
    // We save a bit when a train has shown up in the next block.
    Def()
        .IfReg1(route_set).IfReg1(*sim_occ).IfReg1(next_close_occ)
        .ActReg1(tmp_seen_train_in_next);
    // And when it is gone from there, we must be unoccupied too.
    Def()
        .IfReg1(route_set).IfReg1(*sim_occ)
        .IfReg1(*tmp_seen_train_in_next).IfReg0(next_close_occ)
        .ActReg0(sim_occ)
        .RunCallback(release_cb);
  }
}


}  // namespace automata
