# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: PointXYZRGB.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='PointXYZRGB.proto',
  package='',
  syntax='proto3',
  serialized_pb=_b('\n\x11PointXYZRGB.proto\";\n\x0bPointXYZRGB\x12\t\n\x01x\x18\x01 \x01(\x02\x12\t\n\x01y\x18\x02 \x01(\x02\x12\t\n\x01z\x18\x03 \x01(\x02\x12\x0b\n\x03rgb\x18\x04 \x01(\x02\x62\x06proto3')
)
_sym_db.RegisterFileDescriptor(DESCRIPTOR)




_POINTXYZRGB = _descriptor.Descriptor(
  name='PointXYZRGB',
  full_name='PointXYZRGB',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='x', full_name='PointXYZRGB.x', index=0,
      number=1, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='y', full_name='PointXYZRGB.y', index=1,
      number=2, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='z', full_name='PointXYZRGB.z', index=2,
      number=3, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='rgb', full_name='PointXYZRGB.rgb', index=3,
      number=4, type=2, cpp_type=6, label=1,
      has_default_value=False, default_value=float(0),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=21,
  serialized_end=80,
)

DESCRIPTOR.message_types_by_name['PointXYZRGB'] = _POINTXYZRGB

PointXYZRGB = _reflection.GeneratedProtocolMessageType('PointXYZRGB', (_message.Message,), dict(
  DESCRIPTOR = _POINTXYZRGB,
  __module__ = 'PointXYZRGB_pb2'
  # @@protoc_insertion_point(class_scope:PointXYZRGB)
  ))
_sym_db.RegisterMessage(PointXYZRGB)


# @@protoc_insertion_point(module_scope)
