/** \copyright
 * Copyright (c) 2014, Balazs Racz
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
 * \file UpdateProcessor.cxx
 *
 * Control flow central to the command station: it round-robins between
 * refreshing the individual trains, while giving priority to the update
 * packets.
 *
 * @author Balazs Racz
 * @date 13 May 2014
 */

#include "commandstation/UpdateProcessor.hxx"

#include "dcc/PacketSource.hxx"

namespace commandstation {

// We have only one instance allowed from this class.
static UpdateProcessor* instance_ = nullptr;

// Urgent updates will take BufferBase from this pool to add stuff to the
// priorityUpdates qlist.
static Pool* urgent_update_buffer_pool() { return mainBufferPool; }

struct PriorityUpdate {
  void reset(dcc::PacketSource* source, unsigned code) {
    this->source = source;
    this->code = code;
  }
  dcc::PacketSource* source;
  unsigned code;
};

UpdateProcessor::UpdateProcessor(Service* service, PacketFlowInterface* track_send)
    : StateFlow<Buffer<dcc::Packet>, QList<1> >(service),
      trackSend_(track_send),
      nextRefreshIndex_(0),
      hasRefreshSource_(0) {
  HASSERT(!instance_);
  instance_ = this;
}

UpdateProcessor::~UpdateProcessor() {
  HASSERT(instance_ == this);
  instance_ = nullptr;
}

void UpdateProcessor::notify_update(dcc::PacketSource* source, unsigned code) {
  Buffer<PriorityUpdate>* b;
  urgent_update_buffer_pool()->alloc(&b, nullptr);
  HASSERT(b);
  b->data()->reset(source, code);
  AtomicHolder l(this);
  priorityUpdates_.insert(b, 0);
}

StateFlowBase::Action UpdateProcessor::entry() {
  // We have an empty packet to fill. It is accessible in message()->data().
  // First we check if there is an urgent update.
  Buffer<PriorityUpdate>* b = nullptr;
  {
    AtomicHolder h(this);
    b = static_cast<Buffer<PriorityUpdate>*>(priorityUpdates_.next().item);
  }
  if (b) {
    // found a priority entry.
    b->data()->source->get_next_packet(b->data()->code, message()->data());
    b->unref();
  } else if (hasRefreshSource_) {
    // No new update. Find the next background source.
    dcc::PacketSource* s;
    {
      AtomicHolder h(this);
      if (nextRefreshIndex_ >= refreshSources_.size()) {
        nextRefreshIndex_ = 0;
      }
      s = refreshSources_[nextRefreshIndex_++];
    }
    // requests next background packet from that source.
    s->get_next_packet(0, message()->data());
  } else {
    // No update, no source. We are idle!
    message()->data()->set_dcc_idle();
  }
  // We pass on the filled packet to the track processor.
  trackSend_->send(transfer_message());
  return exit();
}

}  // namespace commandstation

namespace dcc {

// Implementation of the update loop interface.

void packet_processor_notify_update(PacketSource* source, unsigned code) {
  HASSERT(commandstation::instance_);
  commandstation::instance_->notify_update(source, code);
}

/** Adds a new refresh source to the background refresh loop. */
void packet_processor_add_refresh_source(PacketSource* source) {
  HASSERT(commandstation::instance_);
  commandstation::instance_->register_refresh_source(source);
}

/** Removes a refresh source from the background refresh loop. */
void packet_processor_remove_refresh_source(PacketSource* source) {
  HASSERT(commandstation::instance_);
  commandstation::instance_->unregister_refresh_source(source);
}

}  // namespace dcc
