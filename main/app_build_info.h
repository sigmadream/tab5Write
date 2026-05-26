#pragma once

#define SCRIBE_VERSION_MAJOR 0
#define SCRIBE_VERSION_MINOR 1
#define SCRIBE_VERSION_PATCH 0

#define SCRIBE_BUILD_TIMESTAMP __DATE__ " " __TIME__

// Target macro: relying on CONFIG_IDF_TARGET
#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "unknown"
#endif
