#pragma once

// This is the master list of hard uniform locations and names, shaders should comply.
#define UNIFORM_NORMAL_MATRIX_LOC 1
#define UNIFORM_CAMPOS_LOC 2
#define UNIFORM_MVP_LOC 3
#define UNIFORM_MODEL_LOC 4
#define UNIFORM_BALLPOS_LOC 5
#define UNIFORM_ENABLE_NMAP_LOC 6

#define UNIFORM_POINT_LIGHT_SOURCES_NAME "point_light_sources"
#define UNIFORM_POINT_LIGHT_SOURCES_POSITION_NAME "position"
#define UNIFORM_POINT_LIGHT_SOURCES_COLOR_NAME "color"

#define UNIFORM_POINT_LIGHT_SOURCES_LEN 3

#define TEX_TEXT_SAMPLER 0
#define SIMPLE_TEXTURE_SAMPLER 0
#define SIMPLE_NORMAL_SAMPLER 1
#define SIMPLE_ROUGHNESS_SAMPLER 2
#define FUR_FUR_SAMPLER 3
#define FUR_TURBULENCE_SAMPLER 4