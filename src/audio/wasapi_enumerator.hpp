#pragma once

#include <string>
#include <vector>

namespace rx::audio {

struct AudioEndpoint {
    std::string id;          // Endpoint ID or mpv device name (e.g. wasapi/{0.0.0.00000000}.{...})
    std::string name;        // Friendly name (e.g. "Speakers (Realtek Audio)")
    bool isDefault = false;
};

class WasapiEnumerator {
public:
    // Enumerates all active audio output devices on Windows
    static std::vector<AudioEndpoint> enumerateEndpoints();

    // Gets the friendly name of the default output device
    static std::string getDefaultEndpointName();
};

} // namespace rx::audio
