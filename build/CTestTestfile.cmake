# CMake generated Testfile for 
# Source directory: /home/viktoria/oop/rss-reader
# Build directory: /home/viktoria/oop/rss-reader/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[CoreTests]=] "/home/viktoria/oop/rss-reader/build/unit_tests")
set_tests_properties([=[CoreTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/viktoria/oop/rss-reader/CMakeLists.txt;57;add_test;/home/viktoria/oop/rss-reader/CMakeLists.txt;0;")
subdirs("_deps/doctest-build")
