/*
 * leap_conformance_export.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_export.h"

#include "leap/leap_build_info.h"
#include "leap/leap_protocol.h"

#include <string.h>

static void leap_conf_export_escape_csv(const char* src, char* dst, size_t dst_cap)
{
    size_t i;
    size_t j = 0u;

    if (dst == NULL || dst_cap == 0u)
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    for (i = 0u; src[i] != '\0' && j + 1u < dst_cap; i++)
    {
        if (src[i] == '"' || src[i] == ',')
        {
            if (j + 2u >= dst_cap)
            {
                break;
            }
            dst[j++] = ' ';
        }
        else
        {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

static int leap_conf_export_has_device(const LeapConformanceExportMeta* meta)
{
    return meta != NULL && meta->device_mac != NULL && meta->device_mac[0] != '\0';
}

int leap_conformance_export_markdown(
    FILE*                            fp,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta)
{
    size_t i;
    if (fp == NULL || result == NULL)
    {
        return -1;
    }

    fprintf(fp, "# LEAP Conformance Report\n\n");

    if (leap_conf_export_has_device(meta))
    {
        fprintf(fp, "## Device\n\n");
        fprintf(fp, "MAC: %s\n", meta->device_mac);
        if (meta->device_platform != NULL && meta->device_platform[0] != '\0')
        {
            fprintf(fp, "Platform: %s\n", meta->device_platform);
        }
        if (meta->device_product != NULL && meta->device_product[0] != '\0')
        {
            fprintf(fp, "Product: %s\n", meta->device_product);
        }
        if (meta->device_vendor != NULL && meta->device_vendor[0] != '\0')
        {
            fprintf(fp, "Vendor: %s\n", meta->device_vendor);
        }
        if (meta->device_fw != NULL && meta->device_fw[0] != '\0')
        {
            fprintf(fp, "FW: %s\n", meta->device_fw);
        }
        if (meta->leap_protocol != NULL && meta->leap_protocol[0] != '\0')
        {
            fprintf(fp, "LEAP: %s\n", meta->leap_protocol);
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "## Result\n\n");
    fprintf(
        fp,
        "%u/%u %s\n\n",
        result->summary.passed,
        result->summary.total,
        result->summary.failed > 0u ? "FAIL" : "PASS");

    fprintf(fp, "## Tests\n\n");
    for (i = 0u; i < result->step_count; i++)
    {
        const LeapConformanceStepResult* step = &result->steps[i];
        fprintf(
            fp,
            "%s %s\n",
            leap_conformance_step_status_text(step->status),
            step->name);
    }

    fprintf(fp, "\n## Run metadata\n\n");
    fprintf(fp, "| Field | Value |\n");
    fprintf(fp, "|-------|-------|\n");

    if (meta != NULL && meta->started_local != NULL)
    {
        fprintf(fp, "| Date (local) | %s |\n", meta->started_local);
    }
    fprintf(fp, "| Duration | %us |\n", result->summary.elapsed_ms / 1000u);
    fprintf(fp, "| Scenario | %s |\n", result->summary.scenario_id);
    fprintf(fp, "| Adapter | %s |\n", result->summary.adapter);
    if (meta != NULL && meta->nic_name != NULL)
    {
        fprintf(fp, "| NIC | %s |\n", meta->nic_name);
    }
    fprintf(fp, "| Expected peer MAC | %s |\n", result->summary.peer_mac);
    if (meta != NULL && meta->repo_git != NULL)
    {
        fprintf(fp, "| Repo git | %s |\n", meta->repo_git);
    }
    if (meta != NULL)
    {
        fprintf(fp, "| Cyclic seconds | %u |\n", meta->cyclic_seconds);
    }
    if (result->summary.pcap_path[0] != '\0')
    {
        fprintf(fp, "| PCAP | %s |\n", result->summary.pcap_path);
    }

    fprintf(
        fp,
        "| Protocol | %u.%u |\n",
        (unsigned)LEAP_VERSION_MAJOR,
        (unsigned)LEAP_VERSION_MINOR);
    fprintf(fp, "| leap_core git | %s |\n", LEAP_BUILD_GIT);
    if (meta != NULL && meta->tool_version != NULL)
    {
        fprintf(fp, "| Tool | %s |\n", meta->tool_version);
    }

    fprintf(fp, "\n## Step details\n\n");
    fprintf(fp, "| Phase | Test | Status | Duration ms | Detail |\n");
    fprintf(fp, "|-------|------|--------|-------------|--------|\n");

    for (i = 0u; i < result->step_count; i++)
    {
        const LeapConformanceStepResult* step = &result->steps[i];
        fprintf(
            fp,
            "| %s | %s | %s | %u | %s |\n",
            step->phase,
            step->name,
            leap_conformance_step_status_text(step->status),
            step->duration_ms,
            step->detail);
    }

    fprintf(fp, "\n## Summary\n\n");
    fprintf(fp, "- Passed: %u\n", result->summary.passed);
    fprintf(fp, "- Failed: %u\n", result->summary.failed);
    fprintf(fp, "- Skipped: %u\n", result->summary.skipped);
    fprintf(fp, "- Total: %u\n", result->summary.total);
    fprintf(
        fp,
        "\n**OVERALL: %s**\n",
        result->summary.failed > 0u ? "FAIL" : "PASS");

    return 0;
}

int leap_conformance_export_csv(
    FILE*                            fp,
    const LeapConformanceRunResult*  result)
{
    size_t i;

    if (fp == NULL || result == NULL)
    {
        return -1;
    }

    fprintf(fp, "phase,name,status,duration_ms,detail\n");

    for (i = 0u; i < result->step_count; i++)
    {
        const LeapConformanceStepResult* step = &result->steps[i];
        char phase[LEAP_CONF_PHASE_MAX];
        char name[LEAP_CONF_NAME_MAX];
        char detail[LEAP_CONF_DETAIL_MAX];

        leap_conf_export_escape_csv(step->phase, phase, sizeof(phase));
        leap_conf_export_escape_csv(step->name, name, sizeof(name));
        leap_conf_export_escape_csv(step->detail, detail, sizeof(detail));

        fprintf(
            fp,
            "%s,%s,%s,%u,%s\n",
            phase,
            name,
            leap_conformance_step_status_text(step->status),
            step->duration_ms,
            detail);
    }

    return 0;
}

int leap_conformance_export_json(
    FILE*                            fp,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta)
{
    size_t i;
    if (fp == NULL || result == NULL)
    {
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"scenario\": \"%s\",\n", result->summary.scenario_id);
    fprintf(fp, "  \"adapter\": \"%s\",\n", result->summary.adapter);
    fprintf(fp, "  \"peer_mac\": \"%s\",\n", result->summary.peer_mac);
    if (leap_conf_export_has_device(meta))
    {
        fprintf(fp, "  \"device\": {\n");
        fprintf(fp, "    \"mac\": \"%s\"", meta->device_mac);
        if (meta->device_platform != NULL && meta->device_platform[0] != '\0')
        {
            fprintf(fp, ",\n    \"platform\": \"%s\"", meta->device_platform);
        }
        if (meta->device_product != NULL && meta->device_product[0] != '\0')
        {
            fprintf(fp, ",\n    \"product\": \"%s\"", meta->device_product);
        }
        if (meta->device_vendor != NULL && meta->device_vendor[0] != '\0')
        {
            fprintf(fp, ",\n    \"vendor\": \"%s\"", meta->device_vendor);
        }
        if (meta->device_fw != NULL && meta->device_fw[0] != '\0')
        {
            fprintf(fp, ",\n    \"fw\": \"%s\"", meta->device_fw);
        }
        if (meta->leap_protocol != NULL && meta->leap_protocol[0] != '\0')
        {
            fprintf(fp, ",\n    \"leap\": \"%s\"", meta->leap_protocol);
        }
        fprintf(fp, "\n  },\n");
    }
    fprintf(fp, "  \"elapsed_ms\": %u,\n", result->summary.elapsed_ms);
    fprintf(fp, "  \"passed\": %u,\n", result->summary.passed);
    fprintf(fp, "  \"failed\": %u,\n", result->summary.failed);
    fprintf(fp, "  \"skipped\": %u,\n", result->summary.skipped);

    if (result->summary.pcap_path[0] != '\0')
    {
        fprintf(fp, "  \"pcap_path\": \"%s\",\n", result->summary.pcap_path);
    }

    if (meta != NULL && meta->repo_git != NULL)
    {
        fprintf(fp, "  \"repo_git\": \"%s\",\n", meta->repo_git);
    }

    fprintf(
        fp,
        "  \"protocol_version\": \"%u.%u\",\n",
        (unsigned)LEAP_VERSION_MAJOR,
        (unsigned)LEAP_VERSION_MINOR);
    fprintf(fp, "  \"leap_core_git\": \"%s\",\n", LEAP_BUILD_GIT);

    fprintf(fp, "  \"steps\": [\n");
    for (i = 0u; i < result->step_count; i++)
    {
        const LeapConformanceStepResult* step = &result->steps[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"id\": \"%s\",\n", step->step_id);
        fprintf(fp, "      \"phase\": \"%s\",\n", step->phase);
        fprintf(fp, "      \"name\": \"%s\",\n", step->name);
        fprintf(
            fp,
            "      \"status\": \"%s\",\n",
            leap_conformance_step_status_text(step->status));
        fprintf(fp, "      \"duration_ms\": %u,\n", step->duration_ms);
        fprintf(fp, "      \"detail\": \"%s\"\n", step->detail);
        fprintf(fp, "    }%s\n", (i + 1u < result->step_count) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    return 0;
}

static int leap_conf_export_path(
    const char* path,
    int (*writer)(FILE*, const LeapConformanceRunResult*,
                  const LeapConformanceExportMeta*),
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta)
{
    FILE* fp;

    if (path == NULL || writer == NULL || result == NULL)
    {
        return -1;
    }

    fp = fopen(path, "w");
    if (fp == NULL)
    {
        return -1;
    }

    if (writer(fp, result, meta) != 0)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int leap_conformance_export_markdown_path(
    const char*                      path,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta)
{
    return leap_conf_export_path(path, leap_conformance_export_markdown, result, meta);
}

static int leap_conf_export_csv_wrapper(
    FILE*                            fp,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta)
{
    (void)meta;
    return leap_conformance_export_csv(fp, result);
}

int leap_conformance_export_csv_path(
    const char*                      path,
    const LeapConformanceRunResult*  result)
{
    return leap_conf_export_path(
        path, leap_conf_export_csv_wrapper, result, NULL);
}

int leap_conformance_export_json_path(
    const char*                      path,
    const LeapConformanceRunResult*  result,
    const LeapConformanceExportMeta* meta)
{
    return leap_conf_export_path(path, leap_conformance_export_json, result, meta);
}
