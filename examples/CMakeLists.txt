function(clix_add_example target_name output_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE clix::clix)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${output_name})
endfunction()

clix_add_example(
    clix_hello
    hello
    hello/main.cpp
)

clix_add_example(
    clix_calculator
    calculator
    calculator/main.cpp
)

clix_add_example(
    clix_file_editor
    file-editor
    file_editor/main.cpp
    file_editor/file_service.cpp
)

clix_add_example(
    clix_random_number
    random-number
    random_number/main.cpp
)
