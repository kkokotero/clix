add_executable(clix_tests test_main.cpp)
target_link_libraries(clix_tests PRIVATE clix::clix)
target_compile_features(clix_tests PRIVATE cxx_std_17)
add_test(NAME clix.tests COMMAND clix_tests)
