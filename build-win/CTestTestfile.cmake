# CMake generated Testfile for 
# Source directory: D:/LEAP_Protocol
# Build directory: D:/LEAP_Protocol/build-win
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(leap_tests "D:/LEAP_Protocol/build-win/Debug/leap_tests.exe")
  set_tests_properties(leap_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/LEAP_Protocol/CMakeLists.txt;73;add_test;D:/LEAP_Protocol/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(leap_tests "D:/LEAP_Protocol/build-win/Release/leap_tests.exe")
  set_tests_properties(leap_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/LEAP_Protocol/CMakeLists.txt;73;add_test;D:/LEAP_Protocol/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(leap_tests "D:/LEAP_Protocol/build-win/MinSizeRel/leap_tests.exe")
  set_tests_properties(leap_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/LEAP_Protocol/CMakeLists.txt;73;add_test;D:/LEAP_Protocol/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(leap_tests "D:/LEAP_Protocol/build-win/RelWithDebInfo/leap_tests.exe")
  set_tests_properties(leap_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/LEAP_Protocol/CMakeLists.txt;73;add_test;D:/LEAP_Protocol/CMakeLists.txt;0;")
else()
  add_test(leap_tests NOT_AVAILABLE)
endif()
subdirs("leap_core")
subdirs("leap_cli")
subdirs("leap_studio_qt")
