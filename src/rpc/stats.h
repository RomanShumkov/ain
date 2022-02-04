#ifndef DEFI_RPC_STATS_H
#define DEFI_RPC_STATS_H

#include <map>
#include <stdint.h>
#include <univalue.h>
#include <util/time.h>
#include <util/system.h>

#include <boost/circular_buffer.hpp>

const char * const DEFAULT_STATSFILE = "stats.log";
static const bool DEFAULT_RPC_STATS = false;
static const int RPC_STATS_HISTORY_SIZE = 5;

struct MinMaxStatEntry {
    int64_t min;
    int64_t max;
    int64_t avg;
};

struct StatHistoryEntry {
    int64_t timestamp;
    int64_t latency;
    int64_t payload;
};

struct RPCStats {
    std::string name;
    int64_t lastUsedTime;
    MinMaxStatEntry latency;
    MinMaxStatEntry payload;
    int64_t count;
    boost::circular_buffer<StatHistoryEntry> history;

    static RPCStats fromJSON(UniValue json);
    UniValue toJSON() const;
};

/**
 * DeFi Blockchain RPC Stats class.
 */
class CRPCStats
{
private:
    std::map<std::string, RPCStats> map;
public:
    bool add(const std::string& name, const int64_t latency, const int64_t payload);

    RPCStats get(const std::string& name) { return map[name]; };
    std::map<std::string, RPCStats> getMap() { return map; };

    void save() const {
        fs::path statsPath = GetDataDir() / DEFAULT_STATSFILE;
        fsbridge::ofstream file(statsPath);

        UniValue jsonMap(UniValue::VOBJ);
        for (const auto &[method, stats] : map) {
            jsonMap.pushKV(method, stats.toJSON());
        }
        file << jsonMap.write() << '\n';
        file.close();
    };

    void load() {
        fs::path statsPath = GetDataDir() / DEFAULT_STATSFILE;
        fsbridge::ifstream file(statsPath);
        if (!file.is_open()) return;

        std::string line;
        file >> line;

        UniValue jsonMap(UniValue::VOBJ);
        jsonMap.read((const std::string)line);
        for (const auto &key : jsonMap.getKeys()) {
            map[key] = RPCStats::fromJSON(jsonMap[key]);
        }
        file.close();
    };
};

extern CRPCStats statsRPC;

#endif // DEFI_RPC_STATS_H
