# godot-p4-plugin
This plugin is intended to be used as an integrated VCS tool for Perforce users in Godot 4.2+. 

The implementation is based on https://github.com/godotengine/godot-git-plugin with the help of the official Perforce API. 

## Utilities
* Perforce API Docs: https://www.perforce.com/manuals/p4api/Content/P4API/sample-application.html
* GDExtension Docs: https://godotengine.org/article/introducing-gd-extensions/
* P4 Cheat Sheet: https://www.perforce.com/blog/vcs/perforce-cheat-sheet

## Building

### Requirements
* SCons (v3.0.1+)
* OpenSSL v1.0.2 (Included) 
    * The P4 API uses an outdated SSL version with maps to ssleay32.lib and libeay32.lib (rather than libssl.lib and libcrypto.lib in subsequent versions)
* C++17 and C90 compilers detectable by SCons and present in PATH