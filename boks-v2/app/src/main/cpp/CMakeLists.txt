cmake_minimum_required(VERSION 3.18)
project(boks C)

set(GLUE ${ANDROID_NDK}/sources/android/native_app_glue)
add_library(native_app_glue STATIC ${GLUE}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${GLUE})

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(boks SHARED main.c game.c)
# Google Play: 16 KB sahifa o'lchamini qo'llab-quvvatlash
target_link_options(boks PRIVATE -Wl,-z,max-page-size=16384)
target_link_libraries(boks native_app_glue android EGL GLESv2 OpenSLES log m)
