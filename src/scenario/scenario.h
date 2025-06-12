#pragma once

#include "mom.h"
#include "only_cpu.h"

#define SCENARIO_LIST                                                                                                  \
    X(ONLY_CPU, "One CPU task")                                                                                        \
    X(MOM_PUBLISH, "MOM Publish scenario, multiple steps")

#define X(name, description) name,
typedef enum
{
    SCENARIO_LIST SCENARIO_COUNT
} scenario_t;
#undef X

#define X(name, description) description,
static const char *scenario_descriptions[] = {SCENARIO_LIST};
#undef X

static inline const char *get_scenario_description(scenario_t scenario)
{
    if (scenario < 0 || scenario >= SCENARIO_COUNT)
        return "Unknown scenario";
    return scenario_descriptions[scenario];
}

static inline bool is_scenario_valid(scenario_t scenario) { return scenario >= 0 && scenario < SCENARIO_COUNT; }