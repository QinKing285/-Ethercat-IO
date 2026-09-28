# Copyright (c) 2024 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)


set(CONFIG_EEPROM_EMULATION 1)

set(RV_ABI "ilp32f")
set(RV_ARCH "rv32imafc")

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(yeezi_ecat_io)

set(ECAT_INTEGRATION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/ethercat)
set(ECAT_SSC_DIR ${ECAT_INTEGRATION_DIR}/SSC/Src)
set(ECAT_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ethercat/port)

sdk_inc(${ECAT_INTEGRATION_DIR})
sdk_inc(${ECAT_SSC_DIR})
sdk_inc(${ECAT_PORT_DIR})

sdk_app_src(src/main.c)

sdk_app_src(${ECAT_SSC_DIR}/digital_io.c)
sdk_app_src(${ECAT_SSC_DIR}/coeappl.c)
sdk_app_src(${ECAT_SSC_DIR}/ecatappl.c)
sdk_app_src(${ECAT_SSC_DIR}/ecatcoe.c)
sdk_app_src(${ECAT_SSC_DIR}/ecatslv.c)
sdk_app_src(${ECAT_SSC_DIR}/mailbox.c)
sdk_app_src(${ECAT_SSC_DIR}/objdef.c)
sdk_app_src(${ECAT_SSC_DIR}/sdoserv.c)

sdk_app_src(${ECAT_PORT_DIR}/hpm_ecat_hw.c)
sdk_app_src(${ECAT_PORT_DIR}/hpm_ecat_phy.c)
sdk_app_src(${ECAT_PORT_DIR}/hpm_ecat_e2p_emulation.c)

generate_ide_projects()
