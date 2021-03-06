#include <stdint.h>
#include <initializer_list>

//#include "utils/logging.h"

extern "C" {
  void resetblink(uint32_t pattern);
}

#include "mbed_i2c_update.hxx"

#ifdef HAVE_MBED

#include "mbed.h"

I2CUpdaterBase::I2CUpdaterBase(
    I2C* port, int address, std::initializer_list<uint8_t> preamble,
    uint8_t* data_offset, int data_length, int extra_data_length)
    : port_(port), data_offset_(data_offset), address_(address),
      data_length_(data_length), preamble_size_(preamble.size())
{
  storage_ = (uint8_t*)malloc(preamble_size_ + extra_data_length);
  int ofs = 0;
  for (uint8_t d : preamble) {
    storage_[ofs++] = d;
  }
}

void I2COutUpdater::PerformUpdate() {
  uint8_t all_data[preamble_size_ + data_length_];
  memcpy(all_data, preamble(), preamble_size_);
  memcpy(all_data + preamble_size_, data_offset_, data_length_);
  port_->write(address_ << 1, (const char*)(all_data),
               preamble_size_ + data_length_);
}

void I2CInUpdater::OnFailure() {
  for (int i = 0; i < data_length_; i++) {
    count_unchanged(i) = 0;
  }
  const uint32_t kPattern = 0x8002;
  if (blinker_pattern != kPattern) {
    resetblink(kPattern);
  }
}

void I2CInUpdater::OnSuccess() {
  resetblink(0);
}


void I2CInUpdater::PerformUpdate() {
  int ret;
  if (preamble_size_) {
    if ((ret = port_->write(address_ << 1, (const char*)preamble(),
                            preamble_size_, false))) {
      // Error writing.
      OnFailure();
      return;
    }
  }
  uint8_t new_data[data_length_];
  if ((ret = port_->read((address_ << 1) | 1,
                         (char*)new_data, data_length_))) {
    // Error reading.
    OnFailure();
    return;
  }
  for (int i = 0; i < data_length_; i++) {
    if (shadow(i) == new_data[i]) {
      if (count_unchanged(i) == I2C_READ_UNCHANGED_COUNT) {
        data_offset_[i] = listener_->OnChanged(listener_offset_ + i, data_offset_[i], shadow(i));
      } else {
        ++count_unchanged(i);
      }
    } else {
      shadow(i) = new_data[i];
      count_unchanged(i) = 0;
    }
  }
  OnSuccess();
}

#endif  // HAVE_MBED
