/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file FindProtocolDefs.hxx
 *
 * Implementation of the find protocol event handler.
 *
 * @author Balazs Racz
 * @date 18 Feb 2016
 */

#ifndef _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_
#define _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_

#include "commandstation/FindProtocolDefs.hxx"
#include "commandstation/AllTrainNodes.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/TractionTrain.hxx"

namespace commandstation {

class FindProtocolServer : public nmranet::SimpleEventHandler {
 public:
  FindProtocolServer(AllTrainNodes *nodes) : parent_(nodes) {
    nmranet::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, FindProtocolDefs::TRAIN_FIND_BASE),
        FindProtocolDefs::TRAIN_FIND_MASK);
  }

  ~FindProtocolServer() {
    nmranet::EventRegistry::instance()->unregister_handler(this);
  }

  void HandleIdentifyGlobal(const EventRegistryEntry &registry_entry,
                            EventReport *event,
                            BarrierNotifiable *done) override {
    AutoNotify an(done);

    if (event && event->dst_node) {
      // Identify addressed
      auto *impl = parent_->find_node(event->dst_node);
      if (!impl) return;
      static_assert(((FindProtocolDefs::TRAIN_FIND_BASE >>
                      FindProtocolDefs::TRAIN_FIND_MASK) &
                     1) == 1,
                    "The lowermost bit of the TRAIN_FIND_BASE must be 1 or "
                    "else the event produced range encoding must be updated.");
      nmranet::event_write_helper1.WriteAsync(
          event->dst_node, nmranet::Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
          nmranet::WriteHelper::global(),
          nmranet::eventid_to_buffer(FindProtocolDefs::TRAIN_FIND_BASE),
          done->new_child());
    } else {
      // Identify global

      if (pendingGlobalIdentify_) {
        // We have not started processing the global identify yet. Swallow this
        // one.
        return;
      }
      // We do a synchronous alloc here but there isn't a much better choice.
      auto *b = flow_.alloc();
      // Can't do this -- see handleidentifyproducer.
      // b->set_done(done->new_child());
      pendingGlobalIdentify_ = true;
      b->data()->reset(REQUEST_GLOBAL_IDENTIFY);
      flow_.send(b);
    }
  }

  void HandleIdentifyProducer(const EventRegistryEntry &registry_entry,
                              EventReport *event,
                              BarrierNotifiable *done) override {
    AutoNotify an(done);

    auto *b = flow_.alloc();
    // This would be nice in that we would prevent allocating more buffers
    // while the previous request is serviced. However, thereby we also block
    // the progress on the event handler flow, which will cause deadlocks,
    // since servicing the request involves sending events out that will be
    // looped back.
    //
    // b->set_done(done->new_child());
    b->data()->reset(event->event);
    flow_.send(b);
  };

  // For testing.
  bool is_idle() {
    return flow_.is_waiting();
  }

 private:
  enum {
    // Send this in the event_ field if there is a global identify
    // pending. This is not a valid EventID, because the upper byte is 0.
    REQUEST_GLOBAL_IDENTIFY = 0x0001000000000000U,
  };
  struct Request {
    void reset(nmranet::EventId event) { event_ = event; }
    EventId event_;
  };

  class FindProtocolFlow : public StateFlow<Buffer<Request>, QList<1> > {
   public:
    FindProtocolFlow(FindProtocolServer *parent)
        : StateFlow(parent->parent_->tractionService_), parent_(parent) {}

    Action entry() override {
      eventId_ = message()->data()->event_;
      release();
      if (eventId_ == REQUEST_GLOBAL_IDENTIFY) {
        if (!parent_->pendingGlobalIdentify_) {
          // Duplicate global identify, or the previous one was already handled.
          return exit();
        }
        parent_->pendingGlobalIdentify_ = false;
      }
      nextTrainId_ = 0;
      hasMatches_ = false;
      return call_immediately(STATE(iterate));
    }

    Action iterate() {
      if (nextTrainId_ >= nodes()->size()) {
        return call_immediately(STATE(iteration_done));
      }
      if (eventId_ == REQUEST_GLOBAL_IDENTIFY) {
        if (parent_->pendingGlobalIdentify_) {
          // Another notification arrived. Start iteration from 0.
          nextTrainId_ = 0;
          parent_->pendingGlobalIdentify_ = false;
          return again();
        }
        return allocate_and_call(
            nodes()->tractionService_->iface()->global_message_write_flow(),
            STATE(send_response));
      }
      auto db_entry = nodes()->get_traindb_entry(nextTrainId_);
      if (!db_entry) return call_immediately(STATE(next_iterate));
      if (FindProtocolDefs::match_query_to_node(eventId_, db_entry.get())) {
        hasMatches_ = true;
        return allocate_and_call(
            nodes()->tractionService_->iface()->global_message_write_flow(),
            STATE(send_response));
      }
      return yield_and_call(STATE(next_iterate));
    }

    Action send_response() {
      auto *b = get_allocation_result(
          nodes()->tractionService_->iface()->global_message_write_flow());
      b->set_done(bn_.reset(this));
      if (eventId_ == REQUEST_GLOBAL_IDENTIFY) {
        b->data()->reset(
            nmranet::Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
            nodes()->get_train_node_id(nextTrainId_),
            nmranet::eventid_to_buffer(FindProtocolDefs::TRAIN_FIND_BASE));
      } else {
        b->data()->reset(nmranet::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
                         nodes()->get_train_node_id(nextTrainId_),
                         nmranet::eventid_to_buffer(eventId_));
      }
      b->data()->set_flag_dst(nmranet::NMRAnetMessage::WAIT_FOR_LOCAL_LOOPBACK);
      parent_->parent_->tractionService_->iface()
          ->global_message_write_flow()
          ->send(b);

      return wait_and_call(STATE(next_iterate));
    }

    Action next_iterate() {
      ++nextTrainId_;
      return call_immediately(STATE(iterate));
    }

    Action iteration_done() {
      if (!hasMatches_ && (eventId_ & FindProtocolDefs::ALLOCATE)) {
        // TODO: we should wait some time, maybe 200 msec for any responses
        // from other nodes, possibly a deadrail train node, before we actually
        // allocate a new train node.
        DccMode mode;
        unsigned address = FindProtocolDefs::query_to_address(eventId_, &mode);
        newNodeId_ = nodes()->allocate_node(mode, address);
        // We sleep a bit to ensure that the new node is registered and the
        // initialized is enqueued.
        return sleep_and_call(&timer_, MSEC_TO_NSEC(100),
                              STATE(new_node_reply));
      }
      return exit();
    }

    Action new_node_reply() {
      return allocate_and_call(
          nodes()->tractionService_->iface()->global_message_write_flow(),
          STATE(send_new_node_response));
    }

    Action send_new_node_response() {
      auto *b = get_allocation_result(
          nodes()->tractionService_->iface()->global_message_write_flow());
      b->data()->reset(nmranet::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
                       newNodeId_,
                       nmranet::eventid_to_buffer(eventId_));
      parent_->parent_->tractionService_->iface()
          ->global_message_write_flow()
          ->send(b);
      return exit();
    }

   private:
    AllTrainNodes *nodes() { return parent_->parent_; }

    nmranet::EventId eventId_;
    union {
      unsigned nextTrainId_;
      nmranet::NodeID newNodeId_;
    };
    BarrierNotifiable bn_;
    bool hasMatches_;
    FindProtocolServer *parent_;
    StateFlowTimer timer_{this};
  };

  AllTrainNodes *parent_;

  /// Set to true when a global identify message is received. When a global
  /// identify starts processing, it shall be set to false. If a global
  /// identify request arrives with no pendingGlobalIdentify_, that is a
  /// duplicate request that can be ignored.
  bool pendingGlobalIdentify_{false};

  FindProtocolFlow flow_{this};
};

}  // namespace

#endif  // _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_