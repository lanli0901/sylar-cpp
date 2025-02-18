cmake_minimum_required(VERSION 3.5.0)  # 版本要求
project(sylar VERSION 0.1.0 LANGUAGES C CXX)  # 项目名称

include (cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)  # 显示详细的make命令

# 设置 C++ 编译器的标志。具体标志包括：
# $ENV{CXXFLAGS}: 使用环境变量中的 CXXFLAGS（可以从外部传递编译选项）。
# -rdynamic: 启用动态符号表，通常用于调试和符号查找。
# -O3: 启用优化级别 3，进行最大优化。
# -g: 生成调试信息。
# -std=c++11: 使用 C++11 标准。
# -Wall: 启用所有警告信息。
# -Wno-deprecated: 禁用关于过时代码的警告。
# -Werror: 将警告视为错误。
# -Wno-unused-function: 禁用未使用函数的警告。
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")

# 将目录添加到编译器的包含路径中
include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)
link_directories(/usr/local/lib64)

# 查找名为 yaml-cpp 的库，并将其路径存储在 YAMLCPP 变量中。
find_library(YAMLCPP yaml-cpp)
# 输出 YAMLCPP 变量的值
message("***", ${YAMLCPP})

# 定义一个变量 LIB_SRC，其中包含了所有要编译为库的源文件路径
set(LIB_SRC
    sylar/address.cc
    sylar/config.cc
    sylar/fd_manager.cc
    sylar/fiber.cc
    sylar/hook.cc
    sylar/log.cc
    sylar/iomanager.cc
    sylar/scheduler.cc
    sylar/util.cc
    sylar/thread.cc
    sylar/timer.cc
    )

# 使用上面定义的源文件 LIB_SRC，生成一个共享库 sylar。SHARED 指定生成的是一个动态链接库（共享库）。
add_library(sylar SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(sylar)    # 重定义__FILE__这个宏

set(LIB_LIB
    sylar
    dl
    pthread
    ${YAMLCPP}
    )


# 定义一个可执行文件 test，它的源文件为 tests/test.cc。
# 确保在构建 test 可执行文件之前，先构建 sylar 库。即 test 依赖于 sylar 库。
# 将 sylar 库和 yaml-cpp 库链接到 test 可执行文件。
add_executable(test tests/test.cc)
add_dependencies(test sylar)
force_redefine_file_macro_for_sources(test)    # 重定义__FILE__这个宏
target_link_libraries(test ${LIB_LIB})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config sylar)
force_redefine_file_macro_for_sources(test_config)    # 重定义__FILE__这个宏
target_link_libraries(test_config ${LIB_LIB})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread sylar)
force_redefine_file_macro_for_sources(test_thread)    # 重定义__FILE__这个宏
target_link_libraries(test_thread ${LIB_LIB})

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util sylar)
force_redefine_file_macro_for_sources(test_util)    # 重定义__FILE__这个宏
target_link_libraries(test_util ${LIB_LIB})

add_executable(test_fiber tests/test_fiber.cc)
add_dependencies(test_fiber sylar)
force_redefine_file_macro_for_sources(test_fiber)    # 重定义__FILE__这个宏
target_link_libraries(test_fiber ${LIB_LIB})

add_executable(test_scheduler tests/test_scheduler.cc)
add_dependencies(test_scheduler sylar)
force_redefine_file_macro_for_sources(test_scheduler)    # 重定义__FILE__这个宏
target_link_libraries(test_scheduler ${LIB_LIB})

add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager sylar)
force_redefine_file_macro_for_sources(test_iomanager)    # 重定义__FILE__这个宏
target_link_libraries(test_iomanager ${LIB_LIB})

add_executable(test_hook tests/test_hook.cc)
add_dependencies(test_hook sylar)
force_redefine_file_macro_for_sources(test_hook)    # 重定义__FILE__这个宏
target_link_libraries(test_hook ${LIB_LIB})

add_executable(test_address tests/test_address.cc)
add_dependencies(test_address sylar)
force_redefine_file_macro_for_sources(test_address)    # 重定义__FILE__这个宏
target_link_libraries(test_address ${LIB_LIB})

# 设置所有可执行文件的输出目录为项目的 bin 目录。${PROJECT_SOURCE_DIR} 是指项目的根目录。
# 设置所有库文件的输出目录为项目的 lib 目录。
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

