# Maintainer: joaander

# handle native python installs separately from the others
if (PYTHON_SITEDIR)
    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/hoomd.in ${CMAKE_CURRENT_BINARY_DIR}/hoomd @ONLY)
    # install the hoomd runner script to transparently process hoomd into python
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/hoomd
            DESTINATION ${BIN_INSTALL_DIR}
            )
else (PYTHON_SITEDIR)
    # setup and install the hoomd executable
    set(_hoomd_run_input ${CMAKE_CURRENT_SOURCE_DIR}/hoomd_run.in)

    add_executable(hoomd hoomd.cc)
    target_link_libraries(hoomd libhoomd ${HOOMD_COMMON_LIBS})
    fix_cudart_rpath(hoomd)

    # set the rpath so that the exe can find libs stashed in lib/hoomd/python-module
    set_target_properties(hoomd PROPERTIES INSTALL_RPATH "$ORIGIN/../lib/hoomd/python-module")

    if (ENABLE_MPI)
       # if we are compiling with MPI support built in, set appropriate
       # compiler/linker flags
       if(MPI_COMPILE_FLAGS)
           set_target_properties(hoomd PROPERTIES COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
       endif(MPI_COMPILE_FLAGS)
       if(MPI_LINK_FLAGS)
           set_target_properties(hoomd PROPERTIES LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
       endif(MPI_LINK_FLAGS)
    endif(ENABLE_MPI)


    # install the executable
    install(TARGETS hoomd
            RUNTIME DESTINATION ${BIN_INSTALL_DIR})
endif (PYTHON_SITEDIR)
