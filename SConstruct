#!/usr/bin/env python

import os

EnsureSConsVersion(3, 0, 0)
EnsurePythonVersion(3, 5)

opts = Variables([], ARGUMENTS)

env = Environment(ENV=os.environ)

# Define our options
opts.Add(PathVariable("target_path",
         "The path where the lib is installed.", "demo/addons/godot-p4-plugin/"))
opts.Add(PathVariable("target_name", "The library name.",
         "libp4_plugin", PathVariable.PathAccept))

# Updates the environment with the option variables.
opts.Update(env)

if ARGUMENTS.get("custom_api_file", "") != "":
    ARGUMENTS["custom_api_file"] = "../" + ARGUMENTS["custom_api_file"]

ARGUMENTS["target"] = "editor"
env = SConscript("godot-cpp/SConstruct").Clone()
opts.Update(env)

Export("env")

#SConscript("thirdparty/SCsub")

SConscript("src/SCsub")

# Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))
