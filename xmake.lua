-- Copyright 2026
-- Author: Ayvar Aleksiev

set_project("One Billion Row Challenge")
set_version("1.0.0")

-- Compiler / language settings
set_languages("c++23")
if is_plat("windows") then
    set_toolchains("clang-cl", {llvm = true})
else
    set_toolchains("clang")
end
set_warnings("all")

-- Build modes
add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_defaultmode("releasedbg")

-- External dependencies.
add_requires("boost 1.92.0", {configs = {iostreams = true}})

-- Main executable
target("1br")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("boost")
