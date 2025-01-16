import sys, json, os, hashlib, importlib, inspect
from pathlib import Path
from Cheetah.Template import Template
from enum import Enum
from typing import NamedTuple
from google.protobuf.descriptor import FieldDescriptor
import pprint
import re

def calc_hash(file_name):
    with open(file_name, "rb") as f:
        bytes = f.read()  # read entire file as bytes
        return hashlib.sha256(bytes).hexdigest()


def make_cpp_type_name(t):
    return "::" + t.replace(".", "::")

class ProtoEntryIODirection(Enum):
    PROTO_ENTRY_IO_INCOMING = 1
    PROTO_ENTRY_IO_OUTGOING = 2
    PROTO_ENTRY_IO_INCOMING_OUTGOING = 3

class ProtoEntryMessage(NamedTuple):
    cpp_type: str
    enum_id: str
    message_str_tag: str
    io_direction: ProtoEntryIODirection

def get_io_direction( opts_desc ):
    for k, v in opts_desc.ListFields():
        if k.name == "proto_entry_io_direction":
            return ProtoEntryIODirection[dict(k.enum_type.values_by_number)[v].name]

    return None

def get_enum_id( opts_desc, remaining_enum_values, type_type_desc ):
    for k, v in opts_desc.ListFields():
        if k.name == "proto_entry_enum_id":
            if v in remaining_enum_values:
                remaining_enum_values.remove(v)
                return make_cpp_type_name(type_type_desc.full_name) +"::" + v
            else:
                raise Exception(f"{v} is already used by other message "
                                "or not exist in protocol enumeration")
    return None

def scan_spec_from_message_descriptors(message_descriptors, type_type_desc):
    remaining_enum_values = list(map( lambda v: v.name, list(type_type_desc.values)))

    protocol_messages = []

    for m in message_descriptors:
        io_direction = get_io_direction(m.GetOptions())
        enum_id = get_enum_id(m.GetOptions(),
                              remaining_enum_values,
                              type_type_desc)

        if io_direction and enum_id:
            protocol_messages.append(
                ProtoEntryMessage(
                    make_cpp_type_name(m.full_name),
                    enum_id,
                    enum_id.split("::")[-1].lower(),
                    io_direction ) )

    proto_spec = json.loads("{}")
    proto_spec["namespace"] = "/*TODO: must be defined later*/"
    proto_spec["proto_namespace"] = make_cpp_type_name(type_type_desc.file.package)
    proto_spec["headers"] = [type_type_desc.file.name.replace(".proto", ".pb.h")]
    proto_spec["protocol"] = {"incoming": [], "outgoing": []}

    for msg in protocol_messages:
        m = { "enum_id" : msg.enum_id,
              "message_str_tag": msg.message_str_tag,
              "type": msg.cpp_type }

        if msg.io_direction in [ProtoEntryIODirection.PROTO_ENTRY_IO_INCOMING, ProtoEntryIODirection.PROTO_ENTRY_IO_INCOMING_OUTGOING]:
            proto_spec["protocol"]["incoming"].append(m)
        if msg.io_direction in [ProtoEntryIODirection.PROTO_ENTRY_IO_OUTGOING, ProtoEntryIODirection.PROTO_ENTRY_IO_INCOMING_OUTGOING]:
            proto_spec["protocol"]["outgoing"].append(m)

    return proto_spec

def scan_spec_protofile(input_package):
    target_pkg = importlib.import_module(input_package)

    message_descriptors = []
    for name, obj in inspect.getmembers( target_pkg ):
        if inspect.isclass(obj) and obj.DESCRIPTOR.has_options:
            message_descriptors.append( obj.DESCRIPTOR )

    if not hasattr(target_pkg, "MessageType"):
        raise Exception(f"PROTO_ENTRY Protocol must define enum called 'MessageType' "
                        "(which is missing in a proto file of a given protocol)")

    return scan_spec_from_message_descriptors(
        message_descriptors, target_pkg.MessageType.DESCRIPTOR )

def is_valid_cpp_namespace(namespace_str):
    if "" == namespace_str:
        return False

    name_regex = r'^[a-zA-Z_][a-zA-Z0-9_]*$'
    names = namespace_str.split('::')
    for n in names:
        if not re.match(name_regex, n):
            return False

    return True


def main(argv):
    n = len(argv)
    if n < 5:
        print("not enough arguments: {}".format(argv))
        sys.exit(2)

    try:
        print(f"Generator args: {argv}")

        template_path = argv[0]

        output_path = Path(argv[1])

        if not is_valid_cpp_namespace(argv[2]):
            raise Exception(f"{repr(argv[2])} is not a valid C++ namespace path.\n"
                            "Check OUTPUT_NAMESPACE parameter of "
                            "'proto_entry_generate_protocol_entry()' cmake-function")

        output_namespace = argv[2]


        # A pb-package. For example: orc_pb2.py
        if not argv[3] in ["client", "server"]:
            raise Exception(f"{repr(argv[3])} is not 'client' or 'server'.\n"
                            "Check CLIENT_SERVER_ROLE parameter of "
                            "'proto_entry_generate_protocol_entry()' cmake-function")

        client_server_role = argv[3]
        input_package = argv[4]

        # Introduce additional paths to search for packages.
        # At least we expect one path here, which contains a pb package itself
        additional_sys_path = argv[5:]
        for p in additional_sys_path:
            sys.path.append(p)

        os.makedirs(output_path.parent.absolute(), exist_ok=True)

        template = Template.compile(
            file=template_path,
            compilerSettings=dict(
                directiveStartToken="//#",
                directiveEndToken="//#",
                commentStartToken="//##",
            ),
            baseclass=dict,
            useCache=False,
        )

        template_hash = calc_hash(template_path)

        print("Scaning specification: {}".format(input_package))
        proto_spec = scan_spec_protofile( input_package )

        proto_spec["namespace"] = output_namespace
        proto_spec["template_hash"] = template_hash

        if client_server_role == "client":
            x = proto_spec["protocol"]["incoming"]
            proto_spec["protocol"]["incoming"] = proto_spec["protocol"]["outgoing"]
            proto_spec["protocol"]["outgoing"] = x

        print(json.dumps( proto_spec ))

        print("Generating file: {}".format(output_path))
        with open(output_path, "w") as fout:
            fout.write(str(template(proto_spec)))
        print("Generating OK")

    except Exception as error:
        print("\n================ ERROR in proto_entry_generate_protocol_entry() ================\n"
              f"{error}\n"
              "========================================================================\n")
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
