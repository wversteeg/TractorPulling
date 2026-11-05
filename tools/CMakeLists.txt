add_executable(tp_pull tp_pull.cpp)
target_link_libraries(tp_pull PRIVATE tp::core tp::data)
tp_configure_target(tp_pull)
