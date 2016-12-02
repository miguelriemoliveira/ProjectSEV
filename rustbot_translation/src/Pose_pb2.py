# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: Pose.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()


import Point_pb2 as Point__pb2
import Quaternion_pb2 as Quaternion__pb2


DESCRIPTOR = _descriptor.FileDescriptor(
  name='Pose.proto',
  package='',
  syntax='proto3',
  serialized_pb=_b('\n\nPose.proto\x1a\x0bPoint.proto\x1a\x10Quaternion.proto\"B\n\x04Pose\x12\x18\n\x08position\x18\x01 \x01(\x0b\x32\x06.Point\x12 \n\x0borientation\x18\x02 \x01(\x0b\x32\x0b.Quaternionb\x06proto3')
  ,
  dependencies=[Point__pb2.DESCRIPTOR,Quaternion__pb2.DESCRIPTOR,])
_sym_db.RegisterFileDescriptor(DESCRIPTOR)




_POSE = _descriptor.Descriptor(
  name='Pose',
  full_name='Pose',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='position', full_name='Pose.position', index=0,
      number=1, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='orientation', full_name='Pose.orientation', index=1,
      number=2, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
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
  serialized_start=45,
  serialized_end=111,
)

_POSE.fields_by_name['position'].message_type = Point__pb2._POINT
_POSE.fields_by_name['orientation'].message_type = Quaternion__pb2._QUATERNION
DESCRIPTOR.message_types_by_name['Pose'] = _POSE

Pose = _reflection.GeneratedProtocolMessageType('Pose', (_message.Message,), dict(
  DESCRIPTOR = _POSE,
  __module__ = 'Pose_pb2'
  # @@protoc_insertion_point(class_scope:Pose)
  ))
_sym_db.RegisterMessage(Pose)


# @@protoc_insertion_point(module_scope)
