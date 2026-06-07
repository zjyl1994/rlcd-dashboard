cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS
    components/ExternLib
    ../../../ESP32-S3-RLCD-4.2/02_Example/ESP-IDF/10_FactoryProgram/components/ExternLib/codec_board)
project(rlcd_dashboard)
