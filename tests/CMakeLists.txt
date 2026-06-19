find_package(Catch2 3 REQUIRED)

add_executable(chessbotx_tests
    test_uci_parser.cpp
    test_fen.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/UciParser.cpp
    ${CMAKE_SOURCE_DIR}/src/chess/Fen.cpp
)

target_include_directories(chessbotx_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(chessbotx_tests PRIVATE
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(chessbotx_tests)
