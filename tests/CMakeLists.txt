cmake_minimum_required(VERSION 3.7)

function (add_test TEST_NAME TEST_FILE)
    add_executable(${TEST_NAME} ${TEST_FILE}
        test_utils/table.cpp
    )
    target_link_libraries(${TEST_NAME} ConcurrencyLib)
endfunction()


project (ConcurrencyLibTests)

add_test(ContractTest           contract_test.cpp)
add_test(ThreadPoolTest         thread_pool_test.cpp)
add_test(TaskGroupTest          task_group_test.cpp)

add_test(MatrixTest             matrix_test.cpp)
add_test(SortTest               sort_test.cpp)

add_test(LinearEquations        linear_equations_test.cpp)
