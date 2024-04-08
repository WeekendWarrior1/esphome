import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client, time
from esphome.const import (
    CONF_ID,
    CONF_BATTERY_LEVEL,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_ENERGY,
    CONF_ENERGY,
    CONF_POWER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_KILOWATT_HOURS,
    UNIT_WATT,
    UNIT_PERCENT,
    CONF_TIME_ID,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@WeekendWarrior1"]
DEPENDENCIES = ["ble_client"]

emerald_ble_ns = cg.esphome_ns.namespace("emerald_ble")
Emerald = emerald_ble_ns.class_("Emerald", ble_client.BLEClientNode, cg.Component)

CONF_PAIRING_CODE = "pairing_code"
CONF_NOTIFICATION_INTERVAL = "notification_interval"
CONF_PULSES_PER_KWH = "pulses_per_kwh"
CONF_DAILY_ENERGY = "daily_energy"

def _validate(config):
    if CONF_DAILY_ENERGY in config and CONF_TIME_ID not in config:
        _LOGGER.warning(
            "Using daily_energy without a time_id means relying on your Emerald Electricity Advisor's RTC for packet times, which is not recommended. "
            "Please consider adding a time component to your ESPHome yaml, and it's time_id to your emerald_ble component."
        )
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Emerald),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DAILY_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Required(CONF_PAIRING_CODE): cv.int_range(min=1, max=999999),
            cv.Required(CONF_PULSES_PER_KWH): cv.float_range(min=1),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_BATTERY,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_))

    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))

    if CONF_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY])
        cg.add(var.set_energy_sensor(sens))

    if CONF_DAILY_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_DAILY_ENERGY])
        cg.add(var.set_daily_energy_sensor(sens))

    if CONF_PAIRING_CODE in config:
        cg.add(var.set_pairing_code(config[CONF_PAIRING_CODE]))

    if CONF_PULSES_PER_KWH in config:
        cg.add(var.set_pulses_per_kwh(config[CONF_PULSES_PER_KWH]))

    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery(sens))
