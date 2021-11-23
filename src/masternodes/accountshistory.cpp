// Copyright (c) DeFi Blockchain Developers
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.

#include <masternodes/accountshistory.h>
#include <masternodes/accounts.h>
#include <masternodes/masternodes.h>
#include <masternodes/vaulthistory.h>
#include <key_io.h>

struct AccountHistoryKeyOld {
    CScript owner;
    uint32_t blockHeight;
    uint32_t txn;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(owner);

        if (ser_action.ForRead()) {
            READWRITE(WrapBigEndian(blockHeight));
            blockHeight = ~blockHeight;
            READWRITE(WrapBigEndian(txn));
            txn = ~txn;
        }
        else {
            uint32_t blockHeight_ = ~blockHeight;
            READWRITE(WrapBigEndian(blockHeight_));
            uint32_t txn_ = ~txn;
            READWRITE(WrapBigEndian(txn_));
        }
    }
};

constexpr int CAccountsHistoryView::DbVersion;

void CAccountsHistoryView::MigrateHistoryIfNeeded()
{
    int version;
    if (Read(ByAccountHistoryDb::prefix(), version) && version == DbVersion) {
        return;
    }

    AccountHistoryKeyOld beginKey{}, endKey{};
    auto it = LowerBound<ByAccountHistoryOldKey>(beginKey);

    bool needMigration = it.Valid();
    if (needMigration) {
        beginKey = it.Key();
        LogPrintf("History migration in progress...\n");
    }

    auto startTime = GetTimeMillis();

    for (; it.Valid(); it.Next()) {
        endKey = it.Key();
        const auto& key = endKey;
        WriteAccountHistory({key.blockHeight, key.owner, key.txn}, it.Value());
        EraseBy<ByAccountHistoryOldKey>(key); // erase old key
    }

    Write(ByAccountHistoryDb::prefix(), DbVersion);

    bool dbSync = needMigration;
    Flush(dbSync);

    if (needMigration) {
        CompactBy<ByAccountHistoryOldKey>(beginKey, endKey);
    }

    if (needMigration) {
        LogPrint(BCLog::BENCH, "    - History migration took: %dms\n", GetTimeMillis() - startTime);
    }
}

void CAccountsHistoryView::ForEachAccountHistory(std::function<bool(AccountHistoryKey const &, CLazySerialize<AccountHistoryValue>)> callback, AccountHistoryKey const & start)
{
    ForEach<ByAccountHistoryKey, AccountHistoryKey, AccountHistoryValue>(callback, start);
}

Res CAccountsHistoryView::WriteAccountHistory(const AccountHistoryKey& key, const AccountHistoryValue& value)
{
    WriteBy<ByAccountHistoryKey>(key, value);
    return Res::Ok();
}

Res CAccountsHistoryView::EraseAccountHistory(const AccountHistoryKey& key)
{
    EraseBy<ByAccountHistoryKey>(key);
    return Res::Ok();
}

Res CAccountsHistoryView::EraseAccountHistoryHeight(uint32_t height)
{
    std::vector<AccountHistoryKey> keysToDelete;

    auto it = LowerBound<ByAccountHistoryKey>(AccountHistoryKey{height});
    for (; it.Valid() && it.Key().blockHeight == height; it.Next()) {
        keysToDelete.push_back(it.Key());
    }

    for (const auto& key : keysToDelete) {
        EraseAccountHistory(key);
    }
    return Res::Ok();
}

CAccountHistoryStorage::CAccountHistoryStorage(const fs::path& dbName, std::size_t cacheSize, bool fMemory, bool fWipe)
    : CStorageView(new CStorageLevelDB(dbName, cacheSize, fMemory, fWipe))
{
}

CBurnHistoryStorage::CBurnHistoryStorage(const fs::path& dbName, std::size_t cacheSize, bool fMemory, bool fWipe)
    : CStorageView(new CStorageLevelDB(dbName, cacheSize, fMemory, fWipe))
{
}

CAccountsHistoryWriter::CAccountsHistoryWriter(CCustomCSView & storage, uint32_t height, uint32_t txn, const uint256& txid, uint8_t type,
                                               CHistoryWriters* writers)
    : CStorageView(new CFlushableStorageKV(static_cast<CStorageKV&>(storage.GetStorage()))), height(height), txn(txn),
    txid(txid), type(type), writers(writers)
{
}

Res CAccountsHistoryWriter::AddBalance(CScript const & owner, CTokenAmount amount)
{
    auto res = CCustomCSView::AddBalance(owner, amount);
    if (writers && amount.nValue != 0 && res.ok) {
        writers->AddBalance(owner, amount, vaultID);
    }

    return res;
}

Res CAccountsHistoryWriter::SubBalance(CScript const & owner, CTokenAmount amount)
{
    auto res = CCustomCSView::SubBalance(owner, amount);
    if (writers && res.ok && amount.nValue != 0) {
        writers->SubBalance(owner, amount, vaultID);
    }

    return res;
}

bool CAccountsHistoryWriter::Flush()
{
    if (writers) {
        writers->Flush(height, txid, txn, type, vaultID);
    }
    return CCustomCSView::Flush();
}

CHistoryWriters::CHistoryWriters(CAccountHistoryStorage* historyView, CBurnHistoryStorage* burnView, CVaultHistoryStorage* vaultView)
    : historyView(historyView), burnView(burnView), vaultView(vaultView) {}

void CHistoryWriters::AddBalance(const CScript& owner, const CTokenAmount amount, const uint256& vaultID)
{
    if (historyView) {
        diffs[owner][amount.nTokenId] += amount.nValue;
    }
    if (burnView && owner == Params().GetConsensus().burnAddress) {
        burnDiffs[owner][amount.nTokenId] += amount.nValue;
    }
    if (vaultView && !vaultID.IsNull()) {
        vaultDiffs[vaultID][owner][amount.nTokenId] += amount.nValue;
    }
}

void CHistoryWriters::AddFeeBurn(const CScript& owner, const CAmount amount)
{
    if (burnView && amount != 0) {
        burnDiffs[owner][DCT_ID{0}] += amount;
    }
}

void CHistoryWriters::SubBalance(const CScript& owner, const CTokenAmount amount, const uint256& vaultID)
{
    if (historyView) {
        diffs[owner][amount.nTokenId] -= amount.nValue;
    }
    if (burnView && owner == Params().GetConsensus().burnAddress) {
        burnDiffs[owner][amount.nTokenId] -= amount.nValue;
    }
    if (vaultView && !vaultID.IsNull()) {
        vaultDiffs[vaultID][owner][amount.nTokenId] -= amount.nValue;
    }
}

void CHistoryWriters::Flush(const uint32_t height, const uint256& txid, const uint32_t txn, const uint8_t type, const uint256& vaultID)
{
    if (historyView) {
        for (const auto& diff : diffs) {
            historyView->WriteAccountHistory({height, diff.first, txn}, {txid, type, diff.second});
        }
    }
    if (burnView) {
        for (const auto& diff : burnDiffs) {
            burnView->WriteAccountHistory({height, diff.first, txn}, {txid, type, diff.second});
        }
    }
    if (vaultView) {
        for (const auto& diff : vaultDiffs) {
            for (const auto& addresses : diff.second) {
                vaultView->WriteVaultHistory({height, diff.first, txn, addresses.first}, {txid, type, addresses.second});
            }
        }
        if (!schemeID.empty()) {
            vaultView->WriteVaultScheme({vaultID, height}, {type, txid, schemeID, txn});
        }
        if (!globalLoanScheme.identifier.empty()) {
            vaultView->WriteGlobalScheme({height, txn, globalLoanScheme.schemeCreationTxid}, {globalLoanScheme, type, txid});
        }
    }
}

std::unique_ptr<CAccountHistoryStorage> paccountHistoryDB;
std::unique_ptr<CBurnHistoryStorage> pburnHistoryDB;
