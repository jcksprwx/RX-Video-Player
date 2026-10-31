#include "wasapi_enumerator.hpp"
#include "../core/utils.hpp"
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>

namespace rx::audio {

std::vector<AudioEndpoint> WasapiEnumerator::enumerateEndpoints() {
    std::vector<AudioEndpoint> endpoints;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coInitialized = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnum = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

    if (SUCCEEDED(hr) && pEnum) {
        // Get Default Device ID for comparison
        std::wstring defaultId;
        IMMDevice* pDefDevice = nullptr;
        if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDefDevice)) && pDefDevice) {
            LPWSTR pDefIdStr = nullptr;
            if (SUCCEEDED(pDefDevice->GetId(&pDefIdStr)) && pDefIdStr) {
                defaultId = pDefIdStr;
                CoTaskMemFree(pDefIdStr);
            }
            pDefDevice->Release();
        }

        IMMDeviceCollection* pCollection = nullptr;
        if (SUCCEEDED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection)) && pCollection) {
            UINT count = 0;
            pCollection->GetCount(&count);

            for (UINT i = 0; i < count; ++i) {
                IMMDevice* pDevice = nullptr;
                if (SUCCEEDED(pCollection->Item(i, &pDevice)) && pDevice) {
                    LPWSTR pIdStr = nullptr;
                    std::wstring devId;
                    if (SUCCEEDED(pDevice->GetId(&pIdStr)) && pIdStr) {
                        devId = pIdStr;
                        CoTaskMemFree(pIdStr);
                    }

                    IPropertyStore* pProps = nullptr;
                    std::wstring friendlyName = L"Audio Endpoint";
                    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                            if (varName.vt == VT_LPWSTR && varName.pwszVal) {
                                friendlyName = varName.pwszVal;
                            }
                        }
                        PropVariantClear(&varName);
                        pProps->Release();
                    }

                    AudioEndpoint ep;
                    ep.id = "wasapi/" + rx::utils::wideToUtf8(devId);
                    ep.name = rx::utils::wideToUtf8(friendlyName);
                    ep.isDefault = (!defaultId.empty() && devId == defaultId);

                    endpoints.push_back(ep);
                    pDevice->Release();
                }
            }
            pCollection->Release();
        }
        pEnum->Release();
    }

    if (coInitialized) {
        CoUninitialize();
    }

    // Always ensure an "Auto / Default Device" entry exists at the front
    AudioEndpoint autoEp;
    autoEp.id = "auto";
    autoEp.name = "Default System Audio Device";
    autoEp.isDefault = true;
    endpoints.insert(endpoints.begin(), autoEp);

    return endpoints;
}

std::string WasapiEnumerator::getDefaultEndpointName() {
    auto endpoints = enumerateEndpoints();
    for (const auto& ep : endpoints) {
        if (ep.isDefault && ep.id != "auto") return ep.name;
    }
    return "Default Audio Device";
}

} // namespace rx::audio
