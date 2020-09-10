#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) DeFi Blockchain Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test pool's RPC.

- verify basic accounts operation
"""

from test_framework.test_framework import DefiTestFramework

from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, \
    connect_nodes_bi

import random

class PoolSwapTest (DefiTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        # node0: main
        # node1: secondary tester
        # node2: revert create (all)
        self.setup_clean_chain = True
        self.extra_args = [['-txnotokens=0'], ['-txnotokens=0'], ['-txnotokens=0']]

        # Set parameters for create tokens and pools
        self.count_pools = 10
        self.count_account = 1000
        self.commission = 0.001

    def generate_accounts(self):
        acc = []
        for i in range(self.count_account):
            acc.append(self.nodes[0].getnewaddress("", "legacy"))
        return acc

    def create_token(self, symbol, address):
        self.nodes[0].createtoken([], {
            "symbol": symbol,
            "name": "Token " + symbol,
            "isDAT": False,
            "collateralAddress": address
        })
        self.nodes[0].generate(1)

        self.nodes[0].sendmany("", { address : 1, address : 1 })
        self.nodes[0].generate(1)

        self.nodes[0].minttokens([], str(self.count_account * 1000) + "@" + symbol)
        self.nodes[0].generate(1)


    def create_pool(self, tokenA, tokenB, owner):
        self.nodes[0].createpoolpair({
            "tokenA": tokenA,
            "tokenB": tokenB,
            "commission": self.commission,
            "status": True,
            "ownerFeeAddress": owner
        }, [])
        self.nodes[0].generate(1)

    def create_pools(self, owner):
        for i in range(self.count_pools):
            tokenA = "GOLD" + str(i)
            tokenB = "SILVER" + str(i)
            self.create_token(tokenA, owner)
            self.create_token(tokenB, owner)
            self.create_pool(tokenA, tokenB, owner)

    def add_liquidity(self, account, amountA, amountB):
        self.nodes[0].addpoolliquidity({
            account: [amountA, amountB]
        }, account, [])
        self.nodes[0].generate(1)

    def add_pools_liquidity(self, accounts):
        for i in range(self.count_pools):
            tokenA = "GOLD" + str(i)
            tokenB = "SILVER" + str(i)
            for acc in accounts:
                amountA = str(random.randint(1, 500)) + "@" + tokenA
                amountB = str(random.randint(1, 500)) + "@" + tokenB
                self.add_liquidity(acc, amountA, amountB)

    def run_test(self):
        assert_equal(len(self.nodes[0].listtokens()), 1) # only one token == DFI

        print("Generating initial chain...")
        owner = self.nodes[0].getnewaddress("", "legacy")

        self.nodes[0].generate(25)
        self.nodes[1].generate(25)
        self.sync_all()

        self.nodes[0].generate(100)
        self.sync_all()

        # Stop node #2 for future revert
        self.stop_node(2)

        # TODO        
        print("Generating accounts...")
        accounts = self.generate_accounts()
        assert_equal(len(accounts), self.count_account)
        print("Generate " + str(self.count_account) + " accounts")

        print("Generating pools...")
        self.create_pools(owner)
        assert_equal(len(self.nodes[0].getaccount(owner, {}, True)), self.count_pools * 2)
        assert_equal(len(self.nodes[0].listtokens({}, False)), self.count_pools * 3)
        assert_equal(len(self.nodes[0].listpoolpairs({}, False)), self.count_pools)
        print("Generate " + str(self.count_pools) + " pools and " + str(self.count_pools * 2) + " tokens")

        # TODO send tokens

        #print("Generating liquidity...")
        #self.add_pools_liquidity(accounts)
        # TODO

        # REVERTING:
        #========================
        print ("Reverting...")
        self.start_node(2)
        self.nodes[2].generate(20)

        connect_nodes_bi(self.nodes, 1, 2)
        self.sync_blocks()

        #assert_equal(self.nodes[0].getaccount(accountGold, {}, True)[idGold], initialGold)
        #assert_equal(self.nodes[0].getaccount(accountSilver, {}, True)[idSilver], initialSilver)

        assert_equal(len(self.nodes[0].getrawmempool()), 0) # 4 txs


if __name__ == '__main__':
    PoolSwapTest ().main ()
