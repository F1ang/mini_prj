cmake_minimum_required(VERSION 3.8)
project(rc_handle_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rc_handle_msgs REQUIRED)

add_library(${PROJECT_NAME}_driver SHARED
  src/joystick_device.cpp
  src/joy_node.cpp
)
target_include_directories(${PROJECT_NAME}_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(${PROJECT_NAME}_driver
  rclcpp rcl_interfaces sensor_msgs rc_handle_msgs)

add_executable(joy_node src/joy_node_main.cpp)
target_link_libraries(joy_node ${PROJECT_NAME}_driver)
ament_target_dependencies(joy_node rclcpp)

add_executable(joy_raw_listener src/joy_raw_listener.cpp)
ament_target_dependencies(joy_raw_listener rclcpp rc_handle_msgs)

install(TARGETS ${PROJECT_NAME}_driver
  EXPORT export_${PROJECT_NAME}_driver
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS joy_node joy_raw_listener DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})
install(FILES README.md LICENSE DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_joystick_device test/test_joystick_device.cpp)
  target_link_libraries(test_joystick_device ${PROJECT_NAME}_driver)
  ament_add_gtest(test_joy_node test/test_joy_node.cpp TIMEOUT 30)
  target_link_libraries(test_joy_node ${PROJECT_NAME}_driver)
  ament_target_dependencies(test_joy_node rclcpp rc_handle_msgs sensor_msgs)
endif()

ament_export_targets(export_${PROJECT_NAME}_driver HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp rcl_interfaces sensor_msgs rc_handle_msgs)
ament_package()
