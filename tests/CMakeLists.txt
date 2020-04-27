#add_executable(WeightedUrnTest WeightedUrnTest.cpp)
#target_link_libraries(WeightedUrnTest gtest_main tlx)
#add_test(WeightedUrnTest WeightedUrnTest)

add_executable(SimulatorNoLossesTest SimulatorNoLossesTest.cpp)
target_link_libraries(SimulatorNoLossesTest gtest_main tlx)
add_test(SimulatorNoLossesTest SimulatorNoLossesTest)


add_executable(UrnsTest UrnsTest.cpp)
target_link_libraries(UrnsTest gtest_main tlx)
add_test(UrnsTest UrnsTest)
