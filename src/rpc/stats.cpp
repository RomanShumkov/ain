#include <rpc/stats.h>

#include <rpc/server.h>
#include <rpc/util.h>

UniValue RPCStats::toJSON() const {
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

RPCStats RPCStats::fromJSON(UniValue json) {
    RPCStats stats;
    boost::circular_buffer<StatHistoryEntry> history(RPC_STATS_HISTORY_SIZE);
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

bool CRPCStats::add(const std::string& name, const int64_t latency, const int64_t payload)
{
    MinMaxStatEntry latencyEntry,
                    payloadEntry;

    boost::circular_buffer<StatHistoryEntry> history(RPC_STATS_HISTORY_SIZE);
    StatHistoryEntry historyEntry;

    int64_t  min_latency = latency,
             avg_latency = latency,
             max_latency = latency,
             min_payload = payload,
             avg_payload = payload,
             max_payload = payload,
             count       = 1,
             timestamp   = GetSystemTimeInSeconds();

    RPCStats stats;
    auto it = map.find(name);
    if (it != map.end()) {
        stats = it->second;

        count = stats.count + 1;

        latencyEntry = stats.latency;
        min_latency  = std::min(latency, latencyEntry.min);
        max_latency  = std::max(latency, latencyEntry.max);
        avg_latency  = latencyEntry.avg + (latency - latencyEntry.avg) / count;

        payloadEntry = stats.payload;
        min_payload  = std::min(payload, payloadEntry.min);
        max_payload  = std::max(payload, payloadEntry.max);
        avg_payload  = payloadEntry.avg + (payload - payloadEntry.avg) / count;

        history = stats.history;
    }

    latencyEntry.min = min_latency;
    latencyEntry.avg = avg_latency;
    latencyEntry.max = max_latency;

    payloadEntry.min = min_payload;
    payloadEntry.avg = avg_payload;
    payloadEntry.max = max_payload;

    historyEntry.timestamp = timestamp;
    historyEntry.latency   = latency;
    historyEntry.payload   = payload;
    history.push_back(historyEntry);

    map[name] = { name, timestamp, latencyEntry, payloadEntry, count, history };
}

static UniValue getrpcstats(const JSONRPCRequest& request)
{
    RPCHelpMan{"getrpcstats",
        "\nGet RPC stats for selected command.\n",
        {
            {"command", RPCArg::Type::STR, RPCArg::Optional::NO, "The command to get stats for."}
        },
        RPCResult{
            " {\n"
            "  \"name\":               (string) The RPC command name.\n"
            "  \"latency\":            (json object) Min, max and average latency.\n"
            "  \"payload\":            (json object) Min, max and average payload size in bytes.\n"
            "  \"count\":              (numeric) The number of times this command as been used.\n"
            "  \"lastUsedTime\":       (numeric) Last used time as timestamp.\n"
            "  \"history\":            (json array) History of last 5 RPC calls.\n"
            "  [\n"
            "       {\n"
            "           \"timestamp\": (numeric)\n"
            "           \"latency\":   (numeric)\n"
            "           \"payload\":   (numeric)\n"
            "       }\n"
            "  ]\n"
            "}"
        },
        RPCExamples{
            HelpExampleCli("getrpcstats", "getblockcount") +
            HelpExampleRpc("getrpcstats", "\"getblockcount\"")
        },
    }.Check(request);

    if (!gArgs.GetBoolArg("-rpcstats", DEFAULT_RPC_STATS)) {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Rpcstats flag is not set. Activate it by restarting node with -rpcstats.");
    }

    auto command = request.params[0].get_str();
    return statsRPC.get(command).toJSON();
}

static UniValue listrpcstats(const JSONRPCRequest& request)
{
    RPCHelpMan{"listrpcstats",
        "\nList used RPC commands.\n",
        {},
        RPCResult{
            "[\n"
            " {\n"
            "  \"name\":               (string) The RPC command name.\n"
            "  \"latency\":            (json object) Min, max and average latency.\n"
            "  \"payload\":            (json object) Min, max and average payload size in bytes.\n"
            "  \"count\":              (numeric) The number of times this command as been used.\n"
            "  \"lastUsedTime\":       (numeric) Last used time as timestamp.\n"
            "  \"history\":            (json array) History of last 5 RPC calls.\n"
            "  [\n"
            "       {\n"
            "           \"timestamp\": (numeric)\n"
            "           \"latency\":   (numeric)\n"
            "           \"payload\":   (numeric)\n"
            "       }\n"
            "  ]\n"
            " }\n"
            "]"
        },
        RPCExamples{
            HelpExampleCli("listrpcstats", "") +
            HelpExampleRpc("listrpcstats", "")
        },
    }.Check(request);

    if (!gArgs.GetBoolArg("-rpcstats", DEFAULT_RPC_STATS)) {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Rpcstats flag is not set. Activate it by restarting node with -rpcstats.");
    }

    UniValue ret(UniValue::VARR);
    for (const auto &[_, stats] : statsRPC.getMap()) {
        ret.push_back(stats.toJSON());
    }
    return ret;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "stats",            "getrpcstats",              &getrpcstats,          {"command"} },
    { "stats",            "listrpcstats",             &listrpcstats,         {} },
};
// clang-format on

void RegisterStatsRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
