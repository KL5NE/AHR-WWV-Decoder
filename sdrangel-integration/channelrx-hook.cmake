# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors
# --- AHR WWV/WWVH Time Decoder (external plugin) -------------------------------
# https://github.com/KL5NE/AHR-WWV-Decoder  - added by apply_integration.cmake
set(AHRWWV_PLUGIN_DIR "" CACHE PATH "Path to the AHR-WWV-Decoder plugin/ directory (empty = do not build)")
if(AHRWWV_PLUGIN_DIR)
    add_subdirectory(${AHRWWV_PLUGIN_DIR} ahrwwv)
else()
    message(STATUS "Not building AHR WWV time decoder (AHRWWV_PLUGIN_DIR not set)")
endif()
