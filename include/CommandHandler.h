#pragma once

#include <string>
#include "KeyValueStore.h"

// Returns true to continue the session.
// Returns false when a valid EXIT command is received.
bool processCommand(
    KeyValueStore& store,
    const std::string& line,
    std::string& response
);