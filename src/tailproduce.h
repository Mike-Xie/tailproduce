// TODO(dkorolev): Perhaps we should move StreamsRegistry to become the part of the StreamManager base class?

#ifndef TAILPRODUCE_H
#define TAILPRODUCE_H

#include <glog/logging.h>

// TODO(dkorolev): Perhaps move all standard headers for TailProduce into this file?
#include <algorithm>

#include "tpexceptions.h"

#include "storage.h"
#include "streams_registry.h"
#include "stream.h"
#include "entry.h"
#include "order_key.h"
#include "stream_instance.h"
#include "serialize.h"
#include "stream_manager.h"
#include "storage_key_builder.h"
#include "listeners.h"
#include "publishers.h"
#include "stream_manager_params.h"
#include "config_values.h"

// To exlude the macros from being clang-format-ted.
#include "tailproduce.macros"

#endif  // TAILPRODUCE_H