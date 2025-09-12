#pragma once
// FanControl.Release JSON importer (best effort mapping).
// Reads a JSON file, maps fans/sensors by substring labels, and creates channels with curves/hyst/tau via RPC.

#include <QString>

class RpcClient;

namespace Importer {
    bool importFanControlJson(RpcClient* rpc, const QString& filePath, QString* errorOut = nullptr);
}
