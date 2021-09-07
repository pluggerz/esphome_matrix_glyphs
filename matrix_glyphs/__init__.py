from esphome.components.font import (
    CONF_RAW_DATA_ID,
    CONF_RAW_GLYPH_ID
)
from esphome.core import CORE
import importlib
from esphome.cpp_generator import MockObj, Pvariable
from esphome.helpers import read_file
from esphome.voluptuous_schema import _Schema
from esphome import core
from pathlib import Path
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import loader
import sys
import json
import os
from urllib.request import urlopen
from esphome.components import sensor, binary_sensor, switch, image, font
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_FILE,
    CONF_GLYPHS,
    CONF_ID,
    CONF_RANDOM,
    CONF_SENSOR,
    CONF_SIZE,
    CONF_SOURCE,
    CONF_TYPE
)

MDI_NAMES = {
    "bed",
    "keyboard-space",
    "walk",
    "run",
    "doorbell",
    "countertop",
    "sofa-single",
    "silverware-spoon",
    "thermometer",
    "sun-thermometer",
}

MDI_MAP = dict()

CONF_GLYPH = 'glyph'
CONF_SWITCH = 'switch'
CONF_STICKY = 'sticky'
CONF_WIDGETS = 'widgets'
CONF_GROUPS = 'groups'

DEPENDENCIES = ["display"]

CODEOWNERS = ["@hvandenesker"]

# ADAPT FOR CODE
b_matrix_glyph_ns = cg.esphome_ns.namespace("matrix_glyphs")

Controller = b_matrix_glyph_ns.class_("Controller")
Group = b_matrix_glyph_ns.class_("Group")
SensorWidget = b_matrix_glyph_ns.class_("SensorWidget")
BinarySensorWidget = b_matrix_glyph_ns.class_("BinarySensorWidget")

GLYPH_ICON_SCHEMA = _Schema({
    cv.Required('id'): cv.use_id(image.Image_),
})

def mdiIcon(value):
    """Validate that a given config value is a valid icon."""
    if not value:
        return value
    if value in MDI_NAMES:
        return value
    raise cv.Invalid(f'Name should be in: {MDI_NAMES}')


GLYPH_MDI_SCHEMA = _Schema({
    cv.Required('id'): mdiIcon,
})

GLYPH_TYPE_SCHEMA = cv.Any(
    cv.typed_schema(
        {
            'image': cv.Schema(GLYPH_ICON_SCHEMA),
            'mdi':  cv.Schema(GLYPH_MDI_SCHEMA)
        }
    ),
)

WIDGET_SENSOR_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(SensorWidget),
    cv.Required(CONF_ID): cv.use_id(sensor.Sensor)
})

WIDGET_BINARY_SENSOR_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(BinarySensorWidget),
    cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_GLYPH): GLYPH_TYPE_SCHEMA,
    cv.Required(CONF_SWITCH): cv.NAMEABLE_SCHEMA.extend({
        cv.GenerateID(CONF_ID): cv.declare_id(switch.Switch),
        cv.Optional(CONF_STICKY): cv.boolean,
    }),
    cv.Required(CONF_BINARY_SENSOR): cv.NAMEABLE_SCHEMA.extend({
        cv.GenerateID(CONF_ID): cv.declare_id(binary_sensor.BinarySensor),
    }),
})


WIDGET_TYPE_SCHEMA = cv.Any(
    cv.typed_schema(
        {
            CONF_BINARY_SENSOR: cv.Schema(WIDGET_BINARY_SENSOR_SCHEMA),
            CONF_SENSOR: cv.Schema(WIDGET_SENSOR_SCHEMA),
        }
    ),
)

WIDGET_SCHEMA = _Schema({
    cv.Required(CONF_SOURCE): WIDGET_TYPE_SCHEMA,
})

GROUP_SCHEMA = _Schema({
    cv.Required(CONF_ID): cv.declare_id(Group),
    cv.Required(CONF_GLYPH): GLYPH_TYPE_SCHEMA,
    cv.Required(CONF_WIDGETS): cv.ensure_list(WIDGET_SCHEMA),
})

CONFIG_SCHEMA = _Schema(
    {
        cv.Required(CONF_GROUPS): cv.ensure_list(GROUP_SCHEMA),
    }
)

def _validate_sensor(type, id):
    print(f"   \- _validate_sensor: {id}({type})")


async def _process_sensor(groupVar: Pvariable, config: dict):
    widgetVar = cg.new_Pvariable(config[CONF_RANDOM])

    cg.add(widgetVar.set_sensor(await cg.get_variable(config[CONF_ID])))
    cg.add(groupVar.add(widgetVar))


async def _process_binary_sensor(groupVar: Pvariable, config: dict):
    widgetVar = cg.new_Pvariable(config[CONF_RANDOM])

    # if CONF_GLYPH in config:
    #    await _set_glyph(widgetVar, config[CONF_GLYPH])

    cg.add(widgetVar.set_sensor(await cg.get_variable(config[CONF_ID])))
    cg.add(groupVar.add(widgetVar))

    binarySensorConf = config[CONF_BINARY_SENSOR]
    binarySensorVar: Pvariable = cg.Pvariable(
        binarySensorConf[CONF_ID], widgetVar.get_alert_sensor())
    await binary_sensor.register_binary_sensor(binarySensorVar, binarySensorConf)

    switchConf = config[CONF_SWITCH]
    stickySwitchVar: Pvariable = cg.Pvariable(
        switchConf[CONF_ID], widgetVar.get_sticky_switch())
    if (CONF_STICKY in switchConf):
        cg.add(stickySwitchVar.publish_state(switchConf[CONF_STICKY]))
    await switch.register_switch(stickySwitchVar, switchConf)


async def _set_glyph_image(var: Pvariable, config: dict):
    cg.add(var.set_image(await cg.get_variable(config[CONF_ID])))

async def _set_glyph_mdi(var: Pvariable, config: dict):
    cg.add(var.set_image(MockObj(f"std::make_shared<esphome::matrix_glyphs::MdiGlyph>(\"{config[CONF_ID]}\")")))


async def _set_glyph(var: Pvariable, config: dict):
    type = config[CONF_TYPE]

    if (type == 'image'):
        await _set_glyph_image(var, config)
    elif (type == 'mdi'):
        await _set_glyph_mdi(var, config)
    else:
        raise cv.Invalid(f"Not yet supported: {type}")


async def _process_source(groupVar: Pvariable, config: dict):
    type = config[CONF_TYPE]
    id = config[CONF_ID]

    _validate_sensor(type, id)

    if (type == CONF_BINARY_SENSOR):
        await _process_binary_sensor(groupVar, config)
    elif (type == CONF_SENSOR):
        await _process_sensor(groupVar, config)
    else:
        raise


async def _process_widget(groupVar: Pvariable, config: dict):
    await _process_source(groupVar, config[CONF_SOURCE])


async def _process_group(controllerVar: Pvariable, config: dict):
    id = config[CONF_ID]
    var = cg.new_Pvariable(id)

    cg.add(controllerVar.add(var))

    await _set_glyph(var, config[CONF_GLYPH])

    widgets: dict = config[CONF_WIDGETS]
    for i, c in enumerate(widgets):
        await _process_widget(var, c)


def find_file2(file: str) -> Path:
    for p in sys.path:
        ret = os.path.join(p, file)
        print(f"possible finder: {ret}")
    raise cv.Invalid(f"Not found: {file}")


def find_file(file: str) -> Path:
    for p in sys.meta_path:
        print(f"possible meta_path: {p}")
        if (isinstance(p, loader.ComponentMetaFinder)):
            for f in p._finders:
                if (isinstance(f, importlib.machinery.FileFinder)):
                    print(f"possible finder: {f}")
                    path = Path(str(f.path)) / str(file)
                    print(f"path: {path}")
                    if (Path(path).exists()):
                        return path
    raise cv.Invalid(f"Not found: {file}")


async def to_code(config):
    mdi_meta = find_file("mdi.meta.json")
    mdi_ttf = find_file("mdi.ttf")

    with open(mdi_meta) as metaJson:
        MDI_MAP = dict((i['name'], chr(int(i['codepoint'], 16)))
                       for i in json.load(metaJson) if i['name'] in MDI_NAMES)
    print(f"Imported{mdi_meta}): {MDI_MAP}")

    # check if we are complete
    for icon in MDI_NAMES:
        if icon not in MDI_MAP:
            raise cv.Invalid(f"Icon not mapped: {icon}")

    # lets introduce the font
    print(f"ttf: {mdi_ttf}")
    base_name = "matrix_glyphs"
    fontId = base_name+"_font"
    fontConfig = {
        CONF_ID: fontId,
        CONF_FILE: str(mdi_ttf),
        CONF_SIZE: 8,
        CONF_GLYPHS: MDI_MAP.values(),
        CONF_RAW_DATA_ID: core.ID(base_name+"_prog_arr", is_declaration=True, type=cg.uint8),
        CONF_RAW_GLYPH_ID: core.ID(
            base_name+"_data", is_declaration=True, type=font.GlyphData)
    }
    # delegete the work
    validated = font.CONFIG_SCHEMA(fontConfig)
    await font.to_code(validated)

    var = MockObj('esphome::matrix_glyphs::controller')

    cg.add(var.set_mdi_font(MockObj(fontId)))
    for k, v in MDI_MAP.items():
        cg.add(var.add_mdi_code(k, v))

    groups: dict = config[CONF_GROUPS]
    for i, c in enumerate(groups):
        await _process_group(var, c)
