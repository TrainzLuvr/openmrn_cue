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
 * \file TrainDbCdi.hxx
 *
 * CDI entry defining the commandstation traindb entry.
 *
 * @author Balazs Racz
 * @date 8 Feb 2016
 */

#ifndef _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_
#define _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_

#include "ConfigLocal.hxx"

#include "openlcb/ConfigRepresentation.hxx"
#include "commandstation/TrainDbDefs.hxx"
#include "commandstation/ProgrammingTrackSpaceConfig.hxx"

namespace commandstation {

#ifndef TRAINDB_TRAIN_COUNT
#error must define TRAINDB_TRAIN_COUNT in your headers
#endif
static constexpr unsigned STORED_TRAIN_COUNT = TRAINDB_TRAIN_COUNT;

static const char MOMENTARY_MAP[] =
    "<relation><property>0</property><value>Latching</value></relation>"
    "<relation><property>1</property><value>Momentary</value></relation>";

static const char FNDISPLAY_MAP[] =
    "<relation><property>0</property><value>Unavailable</value></relation>"
    "<relation><property>1</property><value>Light</value></relation>"
    "<relation><property>2</property><value>Beamer</value></relation>"
    "<relation><property>3</property><value>Bell</value></relation>"
    "<relation><property>4</property><value>Horn</value></relation>"
    "<relation><property>5</property><value>Shunting mode</value></relation>"
    "<relation><property>6</property><value>Pantograph</value></relation>"
    "<relation><property>7</property><value>Smoke</value></relation>"
    "<relation><property>8</property><value>Momentum off</value></relation>"
    "<relation><property>9</property><value>Whistle</value></relation>"
    "<relation><property>10</property><value>Sound</value></relation>"
    "<relation><property>11</property><value>F</value></relation>"
    "<relation><property>12</property><value>Announce</value></relation>"
    "<relation><property>13</property><value>Engine</value></relation>"
    "<relation><property>14</property><value>Light1</value></relation>"
    "<relation><property>15</property><value>Light2</value></relation>"
    "<relation><property>17</property><value>Uncouple</value></relation>"
    "<relation><property>255</property><value>Unavailable_</value></relation>";

CDI_GROUP(TrainDbCdiFunctionGroup, Name("Functions"),
          Description("Defines what each function button does."));
CDI_GROUP_ENTRY(icon, openlcb::Uint8ConfigEntry, Name("Display"),
                Description("Defines how throttles display this function."),
                Default(FN_NONEXISTANT), MapValues(FNDISPLAY_MAP));
CDI_GROUP_ENTRY(is_momentary, openlcb::Uint8ConfigEntry, Name("Momentary"),
                Description(
                    "Momentary functions are automatically turned off when you "
                    "release the respective button on the throttles."),
                MapValues(MOMENTARY_MAP), Default(0));
CDI_GROUP_END();

using TrainDbCdiRepFunctionGroup =
    openlcb::RepeatedGroup<TrainDbCdiFunctionGroup, DCC_MAX_FN - 1>;

CDI_GROUP(F0Group, Name("F0"),
          Description("F0 is permanently assigned to Light."));
CDI_GROUP_ENTRY(blank, openlcb::EmptyGroup<TrainDbCdiFunctionGroup::size()>);
CDI_GROUP_END();

CDI_GROUP(TrainDbCdiAllFunctionGroup);
CDI_GROUP_ENTRY(f0, F0Group);
CDI_GROUP_ENTRY(all_functions, TrainDbCdiRepFunctionGroup, RepName("Fn"));
CDI_GROUP_END();


static const char DCC_DRIVE_MODE_MAP[] =
    "<relation><property>0</property><value>Unused</value></relation>"
    "<relation><property>10</property><value>DCC 28-step</value></relation>"
    "<relation><property>11</property><value>DCC 128-step</value></relation>"
    "<relation><property>5</property><value>Marklin-Motorola "
    "I</value></relation>"
    "<relation><property>6</property><value>Marklin-Motorola "
    "II</value></relation>"
    "<relation><property>14</property><value>DCC 28-step (forced long "
    "address)</value></relation>"
    "<relation><property>15</property><value>DCC 128-step (forced long "
    "address)</value></relation>";

CDI_GROUP(TrainDbCdiEntry, Description("Configures a single train"));
CDI_GROUP_ENTRY(address, openlcb::Uint16ConfigEntry, Name("Address"),
                Description("Track protocol address of the train."),
                Default(0));
CDI_GROUP_ENTRY(
    mode, openlcb::Uint8ConfigEntry, Name("Protocol"),
    Description("Protocol to use on the track for driving this train."),
    MapValues(DCC_DRIVE_MODE_MAP), Default(DCC_28));
CDI_GROUP_ENTRY(name, openlcb::StringConfigEntry<16>, Name("Name"),
                Description("Identifies the train node on the LCC bus."));
CDI_GROUP_ENTRY(functions, TrainDbCdiAllFunctionGroup);
CDI_GROUP_END();

CDI_GROUP(TrainSegment, Segment(openlcb::MemoryConfigDefs::SPACE_CONFIG));
CDI_GROUP_ENTRY(train, TrainDbCdiEntry);
CDI_GROUP_END();

CDI_GROUP(TrainConfigDef, MainCdi());
// We do not support ACDI and we do not support adding the <identification>
// information in here because both of these vary train by train.
CDI_GROUP_ENTRY(ident, openlcb::Identification, Model("Virtual train node"));
CDI_GROUP_ENTRY(train, TrainSegment);
CDI_GROUP_ENTRY(cv, ProgrammingTrackSpaceConfig);
CDI_GROUP_END();

CDI_GROUP(TmpTrainSegment, Segment(openlcb::MemoryConfigDefs::SPACE_CONFIG),
          Offset(0), Name("Non-stored train"),
          Description(
              "This train is not part of the train database, thus no "
              "configuration settings can be changed on it."));
CDI_GROUP_END();

/// This alternate CDI for a virtual train node will be in use for trains that
/// are not coming from the database. It will not offer any settings for the
/// user.
CDI_GROUP(TrainTmpConfigDef, MainCdi());
CDI_GROUP_ENTRY(ident, openlcb::Identification, Model("Virtual train node"));
CDI_GROUP_ENTRY(train, TmpTrainSegment);
CDI_GROUP_ENTRY(cv, ProgrammingTrackSpaceConfig);
CDI_GROUP_END();

using TrainDbConfig =
    openlcb::RepeatedGroup<TrainDbCdiEntry, STORED_TRAIN_COUNT>;

}  // namespace commandstation

RENDER_CDI(commandstation, TrainConfigDef, "TRAINCDI", 3);
RENDER_CDI(commandstation, TrainTmpConfigDef, "TRAINTMPCDI", 4);

#endif  // _BRACZ_COMMANDSTATION_TRAINDBCDI_HXX_
