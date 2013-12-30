
#ifdef TARGET_LPC11Cxx
#include "mbed.h"
#include "LPC11xx.h"

#define I2CONSET CONSET
#define I2CONCLR CONCLR
#define I2DAT DAT
#define I2STAT STAT

#elif defined(TARGET_LPC2368)
#include "LPC23xx.h"
#elif defined(TARGET_LPC1768)
#include "LPC17xx.h"
#elif !defined(__FreeRTOS__)
#define SKIP_DRIVER
#else
#error don_t know about your CPU
#endif

#ifndef SKIP_DRIVER

#ifndef INTERRUPT_ATTRIBUTE
#define INTERRUPT_ATTRIBUTE
#endif

#include "i2c_driver.hxx"

#include "executor/executor.hxx"

#define CON_AA 4
#define CON_SI 8
#define CON_STO 16
#define CON_STA 32
#define CON_EN 64

I2CDriver::I2CDriver() : done_(nullptr) {
  HASSERT(!instance_);
  instance_ = this;
#ifndef TARGET_LPC11Cxx
  //NVIC_SetVector(IRQn(), irqptr[id]);
#endif
  InitHardware();
  Release();
  os_timer_start(os_timer_create(&I2CDriver::Timeout, nullptr, nullptr),
                 MSEC_TO_NSEC(500));
}

I2CDriver *I2CDriver::instance_ = nullptr;

void I2CDriver::StartWrite(uint8_t address, uint8_t len, Notifiable *done) {
  address_ = address;
  write_offset_ = 0;
  HASSERT(len <= kMaxWriteSize);
  write_len_ = len;
  read_offset_ = 0;
  read_len_ = 0;
  timeout_ = 0;
  done_ = done;
  StartTransaction();
}

void I2CDriver::StartRead(uint8_t address, uint8_t write_len, uint8_t read_len,
                          Notifiable *done) {
  address_ = address;
  write_offset_ = 0;
  HASSERT(write_len <= kMaxWriteSize);
  write_len_ = write_len;
  read_offset_ = 0;
  HASSERT(read_len <= kMaxReadSize);
  read_len_ = read_len;
  timeout_ = 0;
  done_ = done;
  StartTransaction();
}

#ifdef TARGET_LPC11Cxx
// Returns the NVIC interrupt request number for this instance.
::IRQn I2CDriver::IRQn() { return I2C_IRQn; }

// Returns the hardware address for the device.
LPC_I2C_TypeDef *I2CDriver::dev() { return LPC_I2C; }

extern "C" {
void I2C_IRQHandler(void) INTERRUPT_ATTRIBUTE;

// Overrides the system's weak interrupt handler and calls the interrupt
// handler routine.
void I2C_IRQHandler(void) { I2CDriver::instance_->isr(); }
}

void I2CDriver::InitHardware() {
  // Enables power to the i2c hardware circuit.
  LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 5);
  // Pulls reset line to the i2c hardware circuit.
  LPC_SYSCON->PRESETCTRL &= ~(1 << 1);

  // Sets up interrupt, but does not enable it yet.
  NVIC_DisableIRQ(IRQn());
  NVIC_SetPriority(IRQn(), 1);

  // Releases reset line to the i2c hardware circuit.
  LPC_SYSCON->PRESETCTRL |= 1 << 1;


  // Sets frequency.
  // No peripheral clock divider on the M0
  uint32_t PCLK = SystemCoreClock;
  uint32_t hz = 100000;  // 100 kHz
  uint32_t pulse = PCLK / (hz * 2);
  // I2C Rate. We use 50% duty cycle (same time low and high).
  dev()->SCLL = pulse;
  dev()->SCLH = pulse;
  
  // Clears all status requests and enables on I2C state machine.
  dev()->I2CONCLR = CON_EN | CON_AA | CON_SI | CON_STO | CON_STA;
  dev()->I2CONSET = CON_EN;
  
  // Sets output pins to I2C mode.
  // I2C mode, standard / fastmode with input glitch filter.
  LPC_IOCON->PIO0_4 = 1;
  LPC_IOCON->PIO0_5 = 1;
}

#endif


void I2CDriver::StartTransaction() {
  NVIC_EnableIRQ(IRQn());
  // Send start condition. IRQ will kick in to commence transfer.
  dev()->I2CONSET = CON_STA;
}

void I2CDriver::isr() {
  switch (dev()->I2STAT) {
  case 0x08: // Start bit transmited successfully.
  case 0x10: // Repeated start bit transmitted successfully
  {
    dev()->I2CONCLR = CON_STA;
    // Transmit the address and RW bit.
    uint8_t address_dat = address_ << 1;
    if (write_offset_ >= write_len_) {
      // Nothing to write, proceed with reading.
      address_dat |= 1;
    }
    dev()->I2DAT = address_dat;
    break;
  }
  case 0x18: // (W) address transmitted and acked.
  case 0x28: // (W) data byte transmitted and acked.
  {
    // Do we have any more data bytes to transmit?
    if (write_offset_ < write_len_) {
      // Send a data byte.
      dev()->I2DAT = write_bytes_[write_offset_++];
    } else if (read_len_) { // or stuff to read?
      // Switch to read by sending a restart.
      dev()->I2CONSET = CON_STA;
    } else {
      // End of transfer.
      TransferDoneFromISR(true);
    }
    break;
  }
  case 0x00: // Bus error
  case 0x20: // No ack on write address+W
  case 0x30: // No ack on write data
  case 0x48: // No ack on write address+R
  {
    // Abort transaction with failure.
    TransferDoneFromISR(false);
    break;
  }
  case 0x38: // Arbitrarion lost
  {
    // Re-try transfer from sending start condition.
    write_offset_ = 0;
    read_offset_ = 0;
    dev()->I2CONSET = CON_STA;
    break;
  }
  case 0x50: // Data received and acked.
  {
    read_bytes_[read_offset_++] = dev()->I2DAT;
    // fall through
  }
  case 0x40: // (R) Address transmitted and acked.
  {
    if (read_offset_ < read_len_ - 1) {
      dev()->I2CONSET = CON_AA;
    } else {
      dev()->I2CONCLR = CON_AA;
    }
    break;
  }
  case 0x58: // Data received and nack sent.
  {
    read_bytes_[read_offset_++] = dev()->I2DAT;
    // End of transfer.
    TransferDoneFromISR(true);
    break;
  }
  } // switch

  dev()->I2CONCLR = CON_SI;
  timeout_ = 0;  // reset watchdog flag.

#ifdef INTERRUPT_ACK
  LPC_VIC->Address = 0;
#endif
}

/* static */
long long I2CDriver::Timeout(void*, void*) {
  if (!instance_->done_) {
    // No transaction in progress.
    return OS_TIMER_RESTART;
  }
  if (!instance_->timeout_) {
    // We set the flag to act upon it next time.
    instance_->timeout_ = 1;
    return OS_TIMER_RESTART;
  }
  // Now: we have a timeout condition.
  instance_->InitHardware(); // will disable IRQ.

  instance_->success_ = false;
  extern Executor g_executor;
  g_executor.Add(instance_);

  return OS_TIMER_RESTART;
}

void I2CDriver::TransferDoneFromISR(bool succeeded) {
  dev()->I2CONSET = CON_STO;
  NVIC_DisableIRQ(IRQn());
  success_ = succeeded ? 1 : 0;
  extern Executor g_executor;
  g_executor.AddFromIsr(this);
}

void I2CDriver::Run() {
  Notifiable* done = done_;
  done_ = nullptr;
  done->Notify();
}

#endif // SKIP_DRIVER
