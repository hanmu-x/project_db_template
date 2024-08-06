# 检查Windows平台下GRIP_API的一些信息, Linux平台下使用 ecCodes
if (IS_WINDOWS)
    if ("" STREQUAL "$ENV{GRIB_DIR}")
        message(WARNING "GRIB_DEFINITION_PATH NOT FOUND.")
    else ()
        set(GRIB_API_INCLUDE "$ENV{GRIB_DIR}/include")
        set(GRIB_API_LIBRARY "$ENV{GRIB_DIR}/lib/${CMAKE_BUILD_TYPE}/grib_api_lib.lib")
        message(INFO "USE GRIB_API_INCLUDE ${GRIB_API_INCLUDE}.")
        message(INFO "USE GRIB_API_LIBRARY ${GRIB_API_LIBRARY}.")
        include_directories(${GRIB_API_INCLUDE})
        LINK_LIBRARIES(${GRIB_API_LIBRARY})
        add_definitions("-DGRIB_ENABLED")
    endif ()
else()
    message(INFO "临时处理.")
    set(ECCODES_DIR /usr/local/eccodes)
    set(GRIB_API_INCLUDE "${ECCODES_DIR}/include")
        set(GRIB_API_LIBRARY "${ECCODES_DIR}/lib64/libeccodes.so")
        include_directories(${GRIB_API_INCLUDE})
        LINK_LIBRARIES(${GRIB_API_LIBRARY})
        add_definitions("-DGRIB_ENABLED")
   
    if("" STREQUAL "$ENV{ECCODES_DIR}")
        message(WARNING "ECCODES_DIR NOT FOUND.")
    else()
        set(GRIB_API_INCLUDE "$ENV{ECCODES_DIR}/include")
        set(GRIB_API_LIBRARY "$ENV{ECCODES_DIR}/lib64/libeccodes.so")
        include_directories(${GRIB_API_INCLUDE})
        LINK_LIBRARIES(${GRIB_API_LIBRARY})
        add_definitions("-DGRIB_ENABLED")
    endif()

endif ()