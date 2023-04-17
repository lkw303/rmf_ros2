/*
 * Copyright (C) 2020 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "../phases/WaitForCharge.hpp"

#include "../events/GoToPlace.hpp"
#include "../events/LegacyPhaseShim.hpp"

#include "ChargeBattery.hpp"

#include <rmf_task_sequence/events/Bundle.hpp>
#include <rmf_task_sequence/phases/SimplePhase.hpp>
#include <rmf_task_sequence/events/Placeholder.hpp>

#include <rmf_task_sequence/Task.hpp>

#include <rmf_fleet_adapter/schemas/event_description__charge_battery.hpp>

namespace rmf_fleet_adapter {
namespace tasks {

//==============================================================================
struct GoToChargerDescription
  : public rmf_task_sequence::events::Placeholder::Description
{
  GoToChargerDescription()
  : rmf_task_sequence::events::Placeholder::Description("Go to charger", ""),
    charger_waypoint(0),
    has_charger_waypoint(false)
  {
    // Do nothing
  }

  // Overload constructor with option to input waypoint
  GoToChargerDescription(const std::size_t charger_waypoint)
  : rmf_task_sequence::events::Placeholder::Description("Go to charger", ""),
    charger_waypoint(charger_waypoint),
    has_charger_waypoint(true)
  {
    // Do nothing
  }

  std::size_t charger_waypoint;
  bool has_charger_waypoint;

  static rmf_task_sequence::Event::StandbyPtr standby(
    const rmf_task_sequence::Event::AssignIDPtr& id,
    const std::function<rmf_task::State()>& get_state,
    const rmf_task::ConstParametersPtr& parameters,
    const GoToChargerDescription& description,
    std::function<void()> update)
  {
    using GoToPlace = rmf_task_sequence::events::GoToPlace::Description;
    const auto state = get_state();
    const auto context = state.get<agv::GetContext>()->value;

    // Get specified charger waypoint if available
    const std::size_t charger_waypoint = description.has_charger_waypoint ?
      description.charger_waypoint : context->dedicated_charger_wp();

    return events::GoToPlace::Standby::make(
      id, get_state, parameters,
      *GoToPlace::make(charger_waypoint),
      std::move(update));
  }

  static void add(rmf_task_sequence::Event::Initializer& initializer)
  {
    initializer.add<GoToChargerDescription>(
      [](
        const rmf_task_sequence::Event::AssignIDPtr& id,
        const std::function<rmf_task::State()>& get_state,
        const rmf_task::ConstParametersPtr& parameters,
        const GoToChargerDescription& description,
        std::function<void()> update) -> rmf_task_sequence::Event::StandbyPtr
      {
        return standby(
          id, get_state, parameters, description, std::move(update));
      },
      [](
        const rmf_task_sequence::Event::AssignIDPtr& id,
        const std::function<rmf_task::State()>& get_state,
        const rmf_task::ConstParametersPtr& parameters,
        const GoToChargerDescription& description,
        const nlohmann::json&,
        std::function<void()> update,
        std::function<void()> checkpoint,
        std::function<void()> finished) -> rmf_task_sequence::Event::ActivePtr
      {
        return standby(
          id, get_state, parameters, description, std::move(update))
        ->begin(std::move(checkpoint), std::move(finished));
      });
  }
};

//==============================================================================
struct WaitForChargeDescription
  : public rmf_task_sequence::events::Placeholder::Description
{
  WaitForChargeDescription()
  : rmf_task_sequence::events::Placeholder::Description(
      "Wait for charging", "")
  {
    // Do nothing
  }

  static rmf_task_sequence::Event::StandbyPtr standby(
    const rmf_task_sequence::Event::AssignIDPtr& id,
    const std::function<rmf_task::State()>& get_state,
    const rmf_task::ConstParametersPtr& parameters,
    const WaitForChargeDescription&,
    std::function<void()> update)
  {
    const auto state = get_state();
    const auto context = state.get<agv::GetContext>()->value;

    auto legacy = phases::WaitForCharge::make(
      context,
      parameters->battery_system(),
      context->task_planner()->configuration().constraints().recharge_soc());

    return events::LegacyPhaseShim::Standby::make(
      std::move(legacy), context->worker(), context->clock(), id,
      std::move(update));
  }

  static void add(rmf_task_sequence::Event::Initializer& initializer)
  {
    initializer.add<WaitForChargeDescription>(
      [](
        const rmf_task_sequence::Event::AssignIDPtr& id,
        const std::function<rmf_task::State()>& get_state,
        const rmf_task::ConstParametersPtr& parameters,
        const WaitForChargeDescription& description,
        std::function<void()> update) -> rmf_task_sequence::Event::StandbyPtr
      {
        return standby(
          id, get_state, parameters, description, std::move(update));
      },
      [](
        const rmf_task_sequence::Event::AssignIDPtr& id,
        const std::function<rmf_task::State()>& get_state,
        const rmf_task::ConstParametersPtr& parameters,
        const WaitForChargeDescription& description,
        const nlohmann::json&,
        std::function<void()> update,
        std::function<void()> checkpoint,
        std::function<void()> finished) -> rmf_task_sequence::Event::ActivePtr
      {
        return standby(
          id, get_state, parameters, description, std::move(update))
        ->begin(std::move(checkpoint), std::move(finished));
      });
  }
};

//==============================================================================
// TODO(MXG): Consider making the ChargeBatteryEvent description public so it
// can be incorporated into other task types
struct ChargeBatteryEventDescription
  : public rmf_task_sequence::events::Placeholder::Description
{
  ChargeBatteryEventDescription()
  : rmf_task_sequence::events::Placeholder::Description(
      "Charge Battery", ""),
    charger_waypoint(0),
    has_charger_waypoint(false)
  {
    // Do nothing
  }

  ChargeBatteryEventDescription(const std::size_t charger_waypoint)
  : rmf_task_sequence::events::Placeholder::Description(
      "Charge Battery", ""),
    charger_waypoint(charger_waypoint),
    has_charger_waypoint(true)
  {
    // Do nothing
  }

  std::size_t charger_waypoint;
  bool has_charger_waypoint;
};

//==============================================================================
void add_charge_battery(
  agv::TaskDeserialization& deserialization,
  rmf_task::Activator& task_activator,
  const rmf_task_sequence::Phase::ConstActivatorPtr& phase_activator,
  rmf_task_sequence::Event::Initializer& event_initializer,
  std::function<rmf_traffic::Time()> clock)
{
  using Bundle = rmf_task_sequence::events::Bundle;
  using Phase = rmf_task_sequence::phases::SimplePhase;
  using ChargeBattery = rmf_task::requests::ChargeBattery;

  deserialization.add_schema(schemas::event_description__charge_battery);
  auto validate_charge_battery = deserialization.make_validator_shared(
    schemas::event_description__charge_battery);

  auto deserialize_charge_battery =
    [place_deser = deserialization.place](const nlohmann::json& msg)
    -> agv::DeserializedEvent
    {
      std::shared_ptr<const rmf_task_sequence::Event::Description> desc;
      const auto charger_place_it = msg.find("charger_waypoint");
      if (charger_place_it == msg.end())
        desc = std::make_shared<ChargeBatteryEventDescription>();
      // If an invalid place is given send it back to dedicated charger
      else if (!place_deser(charger_place_it.value()).description.has_value())
        return {nullptr,
        std::move(place_deser(charger_place_it.value()).errors)};
      else
        desc = std::make_shared<ChargeBatteryEventDescription>(
          place_deser(charger_place_it.value()).description.value().waypoint());

      agv::DeserializedEvent deserialized = {desc, {}};
      return deserialized;
    };

  deserialization.event->add(
    "charge_battery", validate_charge_battery, deserialize_charge_battery);

  auto private_initializer =
    std::make_shared<rmf_task_sequence::Event::Initializer>();

  WaitForChargeDescription::add(*private_initializer);
  GoToChargerDescription::add(*private_initializer);

  auto charge_battery_event_unfolder =
    [](const ChargeBatteryEventDescription& desc)
    {
      // Set charger waypoint if available
      if (desc.has_charger_waypoint)
        return Bundle::Description({
              std::make_shared<GoToChargerDescription>(
                desc.charger_waypoint),
              std::make_shared<WaitForChargeDescription>()
            }, Bundle::Sequence, "Charge Battery");

      return Bundle::Description({
            std::make_shared<GoToChargerDescription>(),
            std::make_shared<WaitForChargeDescription>()
          }, Bundle::Sequence, "Charge Battery");
    };

  Bundle::unfold<ChargeBatteryEventDescription>(
    std::move(charge_battery_event_unfolder),
    event_initializer, private_initializer);

  auto charge_battery_task_unfolder =
    [](const rmf_task::requests::ChargeBattery::Description&)
    {
      rmf_task_sequence::Task::Builder builder;
      builder
      .add_phase(
        Phase::Description::make(
          std::make_shared<ChargeBatteryEventDescription>(),
          "Charge Battery", ""), {});

      return *builder.build("Charge Battery", "");
    };

  rmf_task_sequence::Task::unfold<ChargeBattery::Description>(
    std::move(charge_battery_task_unfolder),
    task_activator,
    phase_activator,
    std::move(clock));
}

} // namespace task
} // namespace rmf_fleet_adapter
