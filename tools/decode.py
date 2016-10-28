#!/usr/bin/env python3

import mmh3
import struct
import sys
import os
import re
import b
import pkgutil
import google.protobuf.message
from google.protobuf.json_format import MessageToJson

def register(command):

    # Install help
    command.help = 'Decode an nbs file into a series of json objects'

    # Drone arguments
    command.add_argument('file'
        , metavar='file'
        , help='The file to decode into a strint')

def run(file, **kwargs):

    # Open up our message output directory to get our protobuf types
    shared_path = os.path.join(b.cmake_cache[b.cmake_cache["CMAKE_PROJECT_NAME"] + '_BINARY_DIR'], 'shared')

    sys.path.append(shared_path)

    # Load all of the protobuf files as modules to use
    for dir_name, subdir, files in os.walk(shared_path):
        modules = pkgutil.iter_modules(path=[dir_name])
        for loader, module_name, ispkg in modules:
            if module_name.endswith('pb2'):

                # Load our protobuf module
                module = loader.find_module(module_name).load_module(module_name)

    parsers = {}

    # Now that we've imported them all get all the subclasses of protobuf message
    for message in google.protobuf.message.Message.__subclasses__():

        # Work out our original protobuf type
        pb_type = message.__module__.split('.')[:-1]
        pb_type.append(message.__name__)
        pb_type = '.'.join(pb_type).encode('utf-8')
        pb_hash = mmh3.hash_bytes(pb_type, 0x4e55436c, True)

        parsers[pb_hash] = message

    # Now open the passed file
    with open(file, 'rb') as f:
        # While we can read a header
        while len(f.read(3)) == 3:

            # Read our size
            size = struct.unpack('<I', f.read(4))[0]

            # Read our payload
            payload = f.read(size)

            # Read our timestamp
            timestamp = struct.unpack('<Q', payload[:8])[0]

            # Read our hash
            type_hash = payload[8:24]

            # If we know how to parse this type, parse it
            if type_hash in parsers:
                msg = parsers[type_hash].FromString(payload[24:])

                # Print as a json object
                print(timestamp, re.sub(r'\s+', ' ', MessageToJson(msg, True)))
