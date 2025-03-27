# Copyright ACCESS-NRI and contributors. See the top-level LICENSE file for details.

cmake_minimum_required(VERSION 3.18)

#[==============================================================================[
#                         Basic project definition                              #
#]==============================================================================]

project(CICE 
        DESCRIPTION "CICE Sea Ice Model"
        HOMEPAGE_URL https://github.com/ciCE-Consortium/cice
        LANGUAGES C Fortran)

#[==============================================================================[
#                                   Options                                     #
#]==============================================================================]

message(STATUS "Build options")

option(CICE_IO "CICE IO Method" OFF)
if (NOT CICE_IO)
  set(CICE_IO "Binary") #set a default
endif()
if(NOT CICE_IO MATCHES "^(NetCDF|PIO|Binary)$")
  message(FATAL_ERROR " CICE_IO   ${CICE_IO} not valid, choose NetCDF|PIO|Binary")
else()
  message(STATUS "  - CICE_IO   ${CICE_IO}")
endif()

option(CICE_ACCESS3    "Use ACCESS3 dependencies and install ACCESS3 libraries" OFF)
option(CICE_OPENMP      "Enable OpenMP threading"                               OFF)
message(STATUS "  - CICE_ACCESS3   ${CICE_ACCESS3}")
message(STATUS "  - CICE_OPENMP    ${CICE_OPENMP}")

#placeholder CICE_DRIVER option to allow for a nuopc/access driver in the future
option(CICE_DRIVER           "CICE driver code to use" OFF)
if(NOT CICE_DRIVER)
  if(CICE_ACCESS3)
    set(CICE_DRIVER "nuopc/cmeps")
  else()
    set(CICE_DRIVER "standalone/cice")
  endif()
endif()
message(STATUS "  - CICE_DRIVER   ${CICE_DRIVER}")

#[==============================================================================[
#                           Project configuration                               #
#]==============================================================================]

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(FortranLib)

# Common compiler flags and definitions
if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
  set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fbacktrace -fconvert=big-endian -ffree-line-length-none -ffixed-line-length-none")
  if(${CMAKE_Fortran_COMPILER_VERSION} VERSION_GREATER_EQUAL 10)
    set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fallow-argument-mismatch")
  endif()
  set(CMAKE_Fortran_FLAGS_RELEASE "-O")
  set(CMAKE_Fortran_FLAGS_DEBUG "-g -Wall -Og -ffpe-trap=zero,overflow -fcheck=bounds")
elseif(CMAKE_Fortran_COMPILER_ID MATCHES "Intel")
  set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -qno-opt-dynamic-align  -convert big_endian -assume byterecl -ftz -traceback -assume realloc_lhs -fp-model precise")
  set(CMAKE_Fortran_FLAGS_RELEASE "-O2 -debug minimal")
  set(CMAKE_Fortran_FLAGS_DEBUG "-O0 -g -check uninit -check bounds -check pointers -fpe0 -check noarg_temp_created")
else()
  message(WARNING "Fortran compiler with ID ${CMAKE_Fortran_COMPILER_ID} will be used with CMake default options")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
  set(CMAKE_C_FLAGS_RELEASE "-O")
  set(CMAKE_C_FLAGS_DEBUG "-g -Wall -Og -fbacktrace -ffpe-trap=invalid,zero,overflow -fcheck=bounds")
elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -traceback -qno-opt-dynamic-align -fp-model precise -std=gnu99")
  set(CMAKE_C_FLAGS_RELEASE "-O2 -debug minimal")
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
else()
  message(WARNING "C compiler with ID ${CMAKE_C_COMPILER_ID} will be used with CMake default options")
endif()

if (CICE_ACCESS3)
  add_compile_definitions(
    CESMCOUPLED
  )
endif()

## Fortran modules path; currently this is simply set to be the include dir
set(CMAKE_INSTALL_MODULEDIR ${CMAKE_INSTALL_INCLUDEDIR}
  CACHE STRING
  "Fortran module installation path (Not a cmake native variable)"
)

#[==============================================================================[
#                              External packages                                #
#]==============================================================================]

if(CICE_ACCESS3)
  find_package(Access3Share REQUIRED cdeps timing share nuopc_cap_share) 
  if(NOT TARGET ESMF::ESMF) #Access3Share probably already has ESMF with PUBLIC interface
    message(FATAL_ERROR "ESMF interface missing from Access3Share package")  
  endif()
else()
  find_package(MPI REQUIRED)
endif()

if(CICE_OPENMP)
  find_package(OpenMP REQUIRED COMPONENTS Fortran)
endif()

if(CICE_IO MATCHES "^(NetCDF|PIO)$" AND NOT TARGET NetCDF::NetCDF_Fortran)
  # Code has not been tested with versions older than 4.7.3, but probably still works fine 
  find_package(NetCDF 4.7.3 REQUIRED Fortran)
endif()
if(CICE_IO MATCHES "PIO" AND NOT TARGET PIO::PIO_Fortran)
  find_package(PIO 2.5.3 REQUIRED COMPONENTS Fortran)
  # Code has not been tested with versions older than 2.5.3, but probably still works fine
endif()

#[==============================================================================[
#                             Main definitions                                  #
#]==============================================================================]

### Targets

## CICE library

set(CICE_CORE "${CMAKE_SOURCE_DIR}/../../../cicecore")
set(ICEPACK "${CMAKE_SOURCE_DIR}/../../../icepack")

add_fortran_library(cicelib mod STATIC)

if(CICE_IO MATCHES "^(NetCDF|PIO)$")
  target_compile_definitions(cicelib PRIVATE FORTRANUNDERSCORE ncdf)
  target_compile_definitions(cicelib PRIVATE USE_NETCDF)
endif()
if(CICE_ACCESS3)
  target_link_libraries(cicelib
    PUBLIC ESMF::ESMF
    PRIVATE Access3::nuopc_cap_share Access3::share Access3::timing Access3::cdeps-common
  )
endif()
if(CICE_IO MATCHES "^(NetCDF|PIO)$")
  target_link_libraries(cicelib PRIVATE NetCDF::NetCDF_Fortran)
  if(CICE_IO MATCHES "PIO")
    target_link_libraries(cicelib PRIVATE PIO::PIO_Fortran)
  endif()
endif()

if(CICE_OPENMP)
  target_link_libraries(cicelib PUBLIC OpenMP::OpenMP_Fortran)
endif()



target_sources(cicelib PRIVATE
  # Shared List:
  ${CICE_CORE}/shared/ice_arrays_column.F90
  ${CICE_CORE}/shared/ice_calendar.F90
  ${CICE_CORE}/shared/ice_constants.F90
  ${CICE_CORE}/shared/ice_domain_size.F90
  ${CICE_CORE}/shared/ice_fileunits.F90
  ${CICE_CORE}/shared/ice_init_column.F90
  ${CICE_CORE}/shared/ice_kinds_mod.F90
  ${CICE_CORE}/shared/ice_restart_column.F90
  ${CICE_CORE}/shared/ice_restart_shared.F90
  ${CICE_CORE}/shared/ice_spacecurve.F90
  ${CICE_CORE}/shared/ice_distribution.F90

  # Analysis
  ${CICE_CORE}/cicedyn/analysis/ice_diagnostics.F90
  ${CICE_CORE}/cicedyn/analysis/ice_diagnostics_bgc.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_bgc.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_drag.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_fsd.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_mechred.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_pond.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_shared.F90
  ${CICE_CORE}/cicedyn/analysis/ice_history_snow.F90

  # Dynamics
  ${CICE_CORE}/cicedyn/dynamics/ice_dyn_core1d.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_dyn_eap.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_dyn_evp.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_dyn_evp1d.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_dyn_shared.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_dyn_vp.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_transport_driver.F90
  ${CICE_CORE}/cicedyn/dynamics/ice_transport_remap.F90

  # General
  ${CICE_CORE}/cicedyn/general/ice_init.F90
  ${CICE_CORE}/cicedyn/general/ice_flux.F90
  ${CICE_CORE}/cicedyn/general/ice_flux_bgc.F90
  ${CICE_CORE}/cicedyn/general/ice_forcing.F90
  ${CICE_CORE}/cicedyn/general/ice_forcing_bgc.F90
  ${CICE_CORE}/cicedyn/general/ice_state.F90
  ${CICE_CORE}/cicedyn/general/ice_step_mod.F90

  # Infrastructure
  ${CICE_CORE}/cicedyn/infrastructure/ice_blocks.F90
  ${CICE_CORE}/cicedyn/infrastructure/ice_grid.F90
  ${CICE_CORE}/cicedyn/infrastructure/ice_memusage.F90
  ${CICE_CORE}/cicedyn/infrastructure/ice_memusage_gptl.c
  ${CICE_CORE}/cicedyn/infrastructure/ice_read_write.F90
  ${CICE_CORE}/cicedyn/infrastructure/ice_restart_driver.F90
  ${CICE_CORE}/cicedyn/infrastructure/ice_restoring.F90
  ${CICE_CORE}/cicedyn/infrastructure/ice_domain.F90

  # Icepack
  ${ICEPACK}/columnphysics/icepack_aerosol.F90
  ${ICEPACK}/columnphysics/icepack_age.F90
  ${ICEPACK}/columnphysics/icepack_algae.F90
  ${ICEPACK}/columnphysics/icepack_atmo.F90
  ${ICEPACK}/columnphysics/icepack_brine.F90
  ${ICEPACK}/columnphysics/icepack_firstyear.F90
  ${ICEPACK}/columnphysics/icepack_flux.F90
  ${ICEPACK}/columnphysics/icepack_fsd.F90
  ${ICEPACK}/columnphysics/icepack_intfc.F90
  ${ICEPACK}/columnphysics/icepack_isotope.F90
  ${ICEPACK}/columnphysics/icepack_itd.F90
  ${ICEPACK}/columnphysics/icepack_kinds.F90
  ${ICEPACK}/columnphysics/icepack_mechred.F90
  ${ICEPACK}/columnphysics/icepack_meltpond_lvl.F90
  ${ICEPACK}/columnphysics/icepack_meltpond_sealvl.F90
  ${ICEPACK}/columnphysics/icepack_meltpond_topo.F90
  ${ICEPACK}/columnphysics/icepack_mushy_physics.F90
  ${ICEPACK}/columnphysics/icepack_ocean.F90
  ${ICEPACK}/columnphysics/icepack_orbital.F90
  ${ICEPACK}/columnphysics/icepack_parameters.F90
  ${ICEPACK}/columnphysics/icepack_shortwave_data.F90
  ${ICEPACK}/columnphysics/icepack_shortwave.F90
  ${ICEPACK}/columnphysics/icepack_snow.F90
  ${ICEPACK}/columnphysics/icepack_therm_bl99.F90
  ${ICEPACK}/columnphysics/icepack_therm_itd.F90
  ${ICEPACK}/columnphysics/icepack_therm_mushy.F90
  ${ICEPACK}/columnphysics/icepack_therm_shared.F90
  ${ICEPACK}/columnphysics/icepack_therm_vertical.F90
  ${ICEPACK}/columnphysics/icepack_tracers.F90
  ${ICEPACK}/columnphysics/icepack_warnings.F90
  ${ICEPACK}/columnphysics/icepack_wavefracspec.F90
  ${ICEPACK}/columnphysics/icepack_zbgc.F90
  ${ICEPACK}/columnphysics/icepack_zbgc_shared.F90

  # Shared C
  ${CICE_CORE}/cicedyn/infrastructure/ice_shr_reprosum86.c

  # MPI
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_boundary.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_broadcast.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_communicate.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_exit.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_gather_scatter.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_global_reductions.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_reprosum.F90
  ${CICE_CORE}/cicedyn/infrastructure/comm/mpi/ice_timers.F90
)

if(CICE_DRIVER MATCHES "nuopc/cmeps")
  target_sources(cicelib PRIVATE
    # NUOPC CMEPS driver
    ${CICE_CORE}/drivers/${CICE_DRIVER}/CICE_FinalMod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/CICE_InitMod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/CICE_RunMod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/cice_wrapper_mod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/ice_comp_nuopc.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/ice_import_export.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/ice_mesh_mod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/ice_prescribed_mod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/ice_scam.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/ice_shr_methods.F90
  )
elseif(CICE_DRIVER MATCHES "standalone/cice")
  target_sources(cicelib PRIVATE
    # CICE standalone
    ${CICE_CORE}/drivers/${CICE_DRIVER}/CICE_FinalMod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/CICE_InitMod.F90
    ${CICE_CORE}/drivers/${CICE_DRIVER}/CICE_RunMod.F90
  )
else()
  message(FATAL_ERROR "CICE_DRIVER: ${CICE_DRIVER} is currently not supported by the CMake build system")
endif()

# Select IO source files based on CICE_IO
if(CICE_IO MATCHES "NetCDF")
  target_sources(cicelib PRIVATE
    ${CICE_CORE}/cicedyn/infrastructure/io/io_netcdf/ice_history_write.F90
    ${CICE_CORE}/cicedyn/infrastructure/io/io_netcdf/ice_restart.F90
  )
elseif(CICE_IO MATCHES "PIO")
  target_sources(cicelib PRIVATE
    ${CICE_CORE}/cicedyn/infrastructure/io/io_pio2/ice_history_write.F90
    ${CICE_CORE}/cicedyn/infrastructure/io/io_pio2/ice_pio.F90
    ${CICE_CORE}/cicedyn/infrastructure/io/io_pio2/ice_restart.F90
  )
elseif(CICE_IO MATCHES "Binary")
  target_sources(cicelib PRIVATE
    ${CICE_CORE}/cicedyn/infrastructure/io/io_binary/ice_history_write.F90
    ${CICE_CORE}/cicedyn/infrastructure/io/io_binary/ice_restart.F90
  )
endif()

#[==============================================================================[
#                             Install or Export                                 #
#]==============================================================================]

if(CICE_ACCESS3)
  ## Library
  set_target_properties(cicelib PROPERTIES
    OUTPUT_NAME access-cicelib
    EXPORT_NAME cicelib
  )
  install(TARGETS cicelib
    EXPORT CicelibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT AccessCICE_runtime NAMELINK_COMPONENT AccessCICE_Development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT AccessCICE_Development
  )
  # Fortran module files are a special case, as currently there is no standard
  # way of handling them in CMake
  target_include_directories(cicelib PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_MODULEDIR}/cicelib>")
  get_target_property(cice_moddir cicelib Fortran_MODULE_DIRECTORY)
  install(FILES ${cice_moddir}/ice_comp_nuopc.mod
    DESTINATION ${CMAKE_INSTALL_MODULEDIR}/cicelib
    COMPONENT AccessCICECmeps_Development
  )
  install(EXPORT CicelibTargets
    FILE CicelibTargets.cmake
    NAMESPACE Access3::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cicelib
  )

  # Make sure the dependencies get exported too
  configure_package_config_file(
    CicelibConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/CicelibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cicelib
  )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CicelibConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cicelib
    COMPONENT AccessCICE_Development 
  )

  if(CICE_IO MATCHES "NetCDF")
    install(FILES ${CMAKE_SOURCE_DIR}/FindNetCDF.cmake
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cicelib
      COMPONENT IO_NetCDF
    )
  elseif(CICE_IO MATCHES "PIO")
    install(FILES ${CMAKE_SOURCE_DIR}/FindNetCDF.cmake ${CMAKE_SOURCE_DIR}/FindPIO.cmake 
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cicelib
      COMPONENT IO_PIO 
    )
  endif()

else()
  set_target_properties(cicelib PROPERTIES
      OUTPUT_NAME cicelib-standalone
      EXPORT_NAME cicelib
  )
  add_executable(CICE ${CICE_CORE}/drivers/standalone/cice/CICE.F90)
  target_link_libraries(CICE PRIVATE cicelib MPI::MPI_Fortran)

  set_target_properties(CICE PROPERTIES
    LINKER_LANGUAGE Fortran
    OUTPUT_NAME cice
  )
  install(TARGETS CICE
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT IO_${CICE_IO}
  )
endif()
