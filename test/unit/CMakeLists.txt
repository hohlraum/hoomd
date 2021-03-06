# Maintainer: joaander

###############################
# need a few settings for the unit test library
# first, they need the boost unit test framework added as a library
set(ADDITIONAL_LIBS libhoomd
        ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
        )

# then we need to specify BOOST_TEST_DYN_LINK if the unit test framework library is a shared lib
if("${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}" MATCHES "\\.dylib$")
add_definitions(-DBOOST_TEST_DYN_LINK)
endif("${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}" MATCHES "\\.dylib$")

if("${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}" MATCHES "\\.so$")
add_definitions(-DBOOST_TEST_DYN_LINK)
endif("${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}" MATCHES "\\.so$")

####################################
## Setup valgrind options
if (UNIX AND NOT APPLE)
find_program(VALGRIND valgrind DOC
    "valgrind executable for advanced regression tests")
endif (UNIX AND NOT APPLE)

## User gets an option to enable valgrind tests
if (VALGRIND)
    option(ENABLE_VALGRIND OFF "Enable running of unit tests through valgrind")

    set(VALGRIND_COMMON_ARGS
            "--leak-check=full;--suppressions=${CMAKE_CURRENT_SOURCE_DIR}/combined_valgrind.supp;--error-exitcode=1")
    mark_as_advanced(VALGRIND)
endif (VALGRIND)

if (ENABLE_VALGRIND)
    set(DART_TESTING_TIMEOUT 4500 CACHE STRING "Testing timeout" FORCE)
endif (ENABLE_VALGRIND)

enable_testing()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.ocelot ${CMAKE_CURRENT_BINARY_DIR}/configure.ocelot COPYONLY)

###################################
## Setup all of the test executables in a for loop
set(TEST_LIST
    test_quat
    test_rotmat2
    test_rotmat3
    test_vec2
    test_vec3
    test_messenger
    test_cell_list
    test_cell_list_stencil
    test_const_external_field_dipole
    test_index1d
    test_gpu_array
    test_pdata
    test_particle_group
    test_utils
    test_harmonic_bond_force
    test_harmonic_angle_force
    test_harmonic_dihedral_force
    test_harmonic_improper_force
    test_cgcmm_angle_force
    test_fenebond_force
    test_external_periodic
    test_neighborlist
    test_lj_force
    test_mie_force
    test_table_potential
    test_bondtable_bond_force
    test_slj_force
    test_gaussian_force
    test_yukawa_force
    test_cgcmm_force
    test_morse_force
    test_force_shifted_lj
    test_nvt_integrator
    test_berendsen_integrator
    test_zero_momentum_updater
    test_temp_rescale_updater
    test_hoomd_xml
    test_system
    test_fire_energy_minimizer
    test_enforce2d_updater
    test_constraint_sphere
    test_ewald_force
    test_pppm_force
    test_table_dihedral_force
    test_table_angle_force
    test_gridshift_correct
    test_dipole_force
    test_gayberne_force
    test_opls_dihedral_force
    test_walldata
    )

option(HOOMD_SKIP_LONG_TESTS "Skip long unit tests" on)

if (NOT HOOMD_SKIP_LONG_TESTS)
    # put the longest tests last
set(TEST_LIST ${TEST_LIST}
    test_dpd_integrator
    test_bd_rigid_updater
    test_npt_mtk_integrator
    test_nvt_mtk_integrator
    test_nve_integrator)
endif()

if(ENABLE_MPI AND NOT HOOMD_SKIP_LONG_TESTS)
    MACRO(ADD_TO_MPI_TESTS _KEY _VALUE)
    SET("NProc_${_KEY}" "${_VALUE}")
    SET(MPI_TEST_LIST ${MPI_TEST_LIST} ${_KEY})
    ENDMACRO(ADD_TO_MPI_TESTS)

    # define every test together with the number of processors
    ADD_TO_MPI_TESTS(test_communication 8)
    ADD_TO_MPI_TESTS(test_load_balancer 8)
    ADD_TO_MPI_TESTS(test_nvt_integrator_mpi 3)
endif()

foreach (CUR_TEST ${TEST_LIST} ${MPI_TEST_LIST})
    # Need to define NO_IMPORT_ARRAY in every file but hoomd_module.cc
    set_source_files_properties(${CUR_TEST}.cc PROPERTIES COMPILE_DEFINITIONS NO_IMPORT_ARRAY)

    # add and link the unit test executable
    if(ENABLE_CUDA AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${CUR_TEST}.cu)
    cuda_add_executable(${CUR_TEST} EXCLUDE_FROM_ALL ${CUR_TEST}.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/${CUR_TEST}.cu OPTIONS
            ${CUDA_ADDITIONAL_OPTIONS})
    else(ENABLE_CUDA AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${CUR_TEST}.cu)
    add_executable(${CUR_TEST} EXCLUDE_FROM_ALL ${CUR_TEST}.cc)
    endif(ENABLE_CUDA AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${CUR_TEST}.cu)

    add_dependencies(test_all ${CUR_TEST})

    target_link_libraries(${CUR_TEST} libhoomd ${ADDITIONAL_LIBS} ${HOOMD_COMMON_LIBS})
    fix_cudart_rpath(${CUR_TEST})

    if (ENABLE_MPI)
        # set appropriate compiler/linker flags
        if(MPI_COMPILE_FLAGS)
            set_target_properties(${CUR_TEST} PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
        endif(MPI_COMPILE_FLAGS)
        if(MPI_LINK_FLAGS)
            set_target_properties(${CUR_TEST} PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
        endif(MPI_LINK_FLAGS)
    endif (ENABLE_MPI)
endforeach (CUR_TEST)

# add non-MPI tests to test list first
foreach (CUR_TEST ${TEST_LIST})
    # add it to the unit test list
    get_target_property(CUR_TEST_EXE ${CUR_TEST} LOCATION)

    if (ENABLE_MPI)
        add_test(${CUR_TEST} ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 ${MPIEXEC_POSTFLAGS} ${CUR_TEST_EXE})
    else()
        add_test(${CUR_TEST} ${CUR_TEST_EXE})
    endif()
endforeach(CUR_TEST)

# add MPI tests
foreach (CUR_TEST ${MPI_TEST_LIST})
    # add it to the unit test list
    get_target_property(CUR_TEST_EXE ${CUR_TEST} LOCATION)

    # add mpi- prefix to distinguish these tests
    set(MPI_TEST_NAME mpi-${CUR_TEST})

    add_test(${MPI_TEST_NAME}
             ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG}
             ${NProc_${CUR_TEST}} ${MPIEXEC_POSTFLAGS}
             ${CUR_TEST_EXE})
endforeach(CUR_TEST)
