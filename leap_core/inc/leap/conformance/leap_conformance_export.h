/*
 * leap_conformance_export.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_EXPORT_H
#define LEAP_CONFORMANCE_EXPORT_H

#include <stdio.h>

#include "leap/conformance/leap_conformance_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapConformanceExportMeta
{
    const char* started_local;
    const char* repo_git;
    const char* nic_name;
    const char* tool_version;
    unsigned    cyclic_seconds;
    const char* device_mac;
    const char* device_platform;
    const char* device_product;
    const char* device_vendor;
    const char* device_fw;
    const char* leap_protocol;
} LeapConformanceExportMeta;

int leap_conformance_export_markdown(
    FILE*                            fp,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta);

int leap_conformance_export_csv(
    FILE*                            fp,
    const LeapConformanceRunResult*  result);

int leap_conformance_export_json(
    FILE*                            fp,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta);

int leap_conformance_export_markdown_path(
    const char*                      path,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta);

int leap_conformance_export_csv_path(
    const char*                      path,
    const LeapConformanceRunResult*  result);

int leap_conformance_export_json_path(
    const char*                      path,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_EXPORT_H */
