# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: TileConfiguration.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='TileConfiguration.proto',
  package='lightdb.serialization',
  syntax='proto2',
  serialized_options=None,
  serialized_pb=_b('\n\x17TileConfiguration.proto\x12\x15lightdb.serialization\"\x83\x01\n\x11TileConfiguration\x12\x0f\n\x07version\x18\x01 \x02(\r\x12\x17\n\x0fnumberOfColumns\x18\x02 \x02(\r\x12\x14\n\x0cnumberOfRows\x18\x03 \x02(\r\x12\x17\n\x0fwidthsOfColumns\x18\x04 \x03(\r\x12\x15\n\rheightsOfRows\x18\x05 \x03(\r')
)




_TILECONFIGURATION = _descriptor.Descriptor(
  name='TileConfiguration',
  full_name='lightdb.serialization.TileConfiguration',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='version', full_name='lightdb.serialization.TileConfiguration.version', index=0,
      number=1, type=13, cpp_type=3, label=2,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='numberOfColumns', full_name='lightdb.serialization.TileConfiguration.numberOfColumns', index=1,
      number=2, type=13, cpp_type=3, label=2,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='numberOfRows', full_name='lightdb.serialization.TileConfiguration.numberOfRows', index=2,
      number=3, type=13, cpp_type=3, label=2,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='widthsOfColumns', full_name='lightdb.serialization.TileConfiguration.widthsOfColumns', index=3,
      number=4, type=13, cpp_type=3, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='heightsOfRows', full_name='lightdb.serialization.TileConfiguration.heightsOfRows', index=4,
      number=5, type=13, cpp_type=3, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=51,
  serialized_end=182,
)

DESCRIPTOR.message_types_by_name['TileConfiguration'] = _TILECONFIGURATION
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

TileConfiguration = _reflection.GeneratedProtocolMessageType('TileConfiguration', (_message.Message,), {
  'DESCRIPTOR' : _TILECONFIGURATION,
  '__module__' : 'TileConfiguration_pb2'
  # @@protoc_insertion_point(class_scope:lightdb.serialization.TileConfiguration)
  })
_sym_db.RegisterMessage(TileConfiguration)


# @@protoc_insertion_point(module_scope)