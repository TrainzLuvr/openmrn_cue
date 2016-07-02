#ifndef _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_
#define _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_

#include "nmranet/ConfigRepresentation.hxx"
#include "nmranet/ConfiguredConsumer.hxx"
#include "nmranet/ConfiguredProducer.hxx"
#include "nmranet/MemoryConfig.hxx"
#include "custom/SpeedFeedbackController.hxx"

CDI_GROUP(MotorControl, Name("Motor control"));
CDI_GROUP_ENTRY(pwm_frequency, nmranet::Uint16ConfigEntry,
                Name("PWM frequency"),
                Description(
                    "Specifies what frequency the motor should be driven at. "
                    "Typical values are in the 3000-20000 range."),
                Min(3), Max(50000), Default(20000));
CDI_GROUP_ENTRY(enable_kick, nmranet::Uint8ConfigEntry,
                Name("Enable motor kick"),
                Description(
                    "Repeatedly sends a high-power pulse to the motor to avoid "
                    "stalling at low speed steps."),
                Min(0), Max(1), Default(1));
CDI_GROUP_ENTRY(
    kick_delay, nmranet::Uint16ConfigEntry, Name("Motor kick delay"),
    Description("How many milliseconds to wait between motor kicks."), Min(1),
    Default(200));
CDI_GROUP_ENTRY(
    kick_length, nmranet::Uint8ConfigEntry, Name("Motor kick length"),
    Description("Length of kick pulse in milliseconds to send to motor."),
    Min(0), Max(100), Default(5));
CDI_GROUP_ENTRY(load_control, FeedbackParams);
CDI_GROUP_END();

/// Defines the main segment in the configuration CDI. This is laid out at
/// origin 128 to give space for the ACDI user data at the beginning.
CDI_GROUP(TrainBoardSegment, Segment(nmranet::MemoryConfigDefs::SPACE_CONFIG),
          Offset(128));
CDI_GROUP_ENTRY(internal_data, nmranet::InternalConfigData);
CDI_GROUP_ENTRY(motor_control, MotorControl);
CDI_GROUP_ENTRY(address, nmranet::Uint16ConfigEntry, Name("Cab number"),
                Description(
                    "Reports this address to the OpenLCB bus in order to be "
                    "acquired by throttles using the numeric keypad."),
                Default(11000));
CDI_GROUP_END();

namespace nmranet {

/// Defines the identification information for the node. The arguments are:
///
/// - 4 (version info, always 4 by the standard
/// - Manufacturer name
/// - Model name
/// - Hardware version
/// - Software version
///
/// This data will be used for all purposes of the identification:
///
/// - the generated cdi.xml will include this data
/// - the Simple Node Ident Info Protocol will return this data
/// - the ACDI memory space will contain this data.
extern const SimpleNodeStaticValues SNIP_STATIC_DATA = {
    4, "Balazs Racz", "Dead-rail train", "ESP12", "0.1"};

static const uint16_t EXPECTED_VERSION = 0x1bd6;

/// The main structure of the CDI. ConfigDef is the symbol we use in main.cxx
/// to refer to the configuration defined here.
CDI_GROUP(ConfigDef, MainCdi());
/// Adds the <identification> tag with the values from SNIP_STATIC_DATA above.
CDI_GROUP_ENTRY(ident, Identification);
/// Adds an <acdi> tag.
CDI_GROUP_ENTRY(acdi, Acdi);
/// Adds a segment for changing the values in the ACDI user-defined
/// space. UserInfoSegment is defined in the system header.
CDI_GROUP_ENTRY(userinfo, UserInfoSegment);
/// Adds the main configuration segment.
CDI_GROUP_ENTRY(seg, TrainBoardSegment);
CDI_GROUP_END();

}  // namespace nmranet

#endif  // _APPLICATIONS_IO_BOARD_TARGET_CONFIG_HXX_