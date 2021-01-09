#file(GLOB_RECURSE TEST_SOURCES "tests/*.*")

#add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O0)
#add_link_options(-fsanitize=address)

add_executable(${PROJECT_NAME}-Test
               ${TEST_SOURCES}
               tests/catch2/test_main.cpp
               tests/catch2/proba.cpp
               tests/calc.c
               )