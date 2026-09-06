# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\PlantMap_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\PlantMap_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\plantcore_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\plantcore_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\plantmap_selftest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\plantmap_selftest_autogen.dir\\ParseCache.txt"
  "PlantMap_autogen"
  "plantcore_autogen"
  "plantmap_selftest_autogen"
  )
endif()
