#include <rpc/stats.h>

#include <rpc/server.h>
#include <rpc/util.h>

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
