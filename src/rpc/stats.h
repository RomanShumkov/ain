#ifndef DEFI_RPC_STATS_H
#define DEFI_RPC_STATS_H

#include <map>
#include <stdint.h>
#include <univalue.h>
#include <util/time.h>
#include <util/system.h>

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
    std::vector<StatHistoryEntry> history;

    UniValue toJSON() const {
        if (count == 0) return UniValue::VOBJ;

        UniValue stats(UniValue::VOBJ),
                 latencyObj(UniValue::VOBJ),
                 payloadObj(UniValue::VOBJ),
                 historyArr(UniValue::VARR);

        latencyObj.pushKV("min", latency.min);
        latencyObj.pushKV("avg", latency.avg);
        latencyObj.pushKV("max", latency.max);

        payloadObj.pushKV("min", payload.min);
        payloadObj.pushKV("avg", payload.avg);
        payloadObj.pushKV("max", payload.max);

        for (auto const &entry : history) {
            UniValue historyObj(UniValue::VOBJ);
            historyObj.pushKV("timestamp", entry.timestamp);
            historyObj.pushKV("latency", entry.latency);
            historyObj.pushKV("payload", entry.payload);
            historyArr.push_back(historyObj);
        }

        stats.pushKV("name", name);
        stats.pushKV("count", count);
        stats.pushKV("lastUsedTime", lastUsedTime);
        stats.pushKV("latency", latencyObj);
        stats.pushKV("payload", payloadObj);
        stats.pushKV("history", historyArr);
        return stats;
    }

    static RPCStats fromJSON(UniValue json) {
        RPCStats stats;
        std::vector<StatHistoryEntry> history;
        MinMaxStatEntry latencyEntry,
                        payloadEntry;

        auto name = json["name"].get_str();
        auto lastUsedTime = json["lastUsedTime"].get_int64();
        auto count = json["count"].get_int64();

        if (!json["latency"].isNull()) {
            auto latencyObj  = json["latency"].get_obj();
            latencyEntry.min = latencyObj["min"].get_int64();
            latencyEntry.avg = latencyObj["avg"].get_int64();
            latencyEntry.max = latencyObj["max"].get_int64();
        }

        if (!json["payload"].isNull()) {
            auto payloadObj  = json["payload"].get_obj();
            payloadEntry.min = payloadObj["min"].get_int64();
            payloadEntry.avg = payloadObj["avg"].get_int64();
            payloadEntry.max = payloadObj["max"].get_int64();
        }

        if (!json["history"].isNull()) {
            auto historyArr = json["history"].get_array();
            for (const auto &entry : historyArr.getValues()) {
                auto historyObj = entry.get_obj();
                StatHistoryEntry historyEntry;

                historyEntry.timestamp = historyObj["timestamp"].get_int64();
                historyEntry.latency   = historyObj["latency"].get_int64();
                historyEntry.payload   = historyObj["payload"].get_int64();
                history.push_back(historyEntry);
            }
        }

        return { name, lastUsedTime, latencyEntry, payloadEntry, count, history };
    }
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
