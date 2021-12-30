#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) DeFi Blockchain Developers
# Distributed under the MIT software license, see the accompanying
# file LICENSE or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC stats."""

from test_framework.test_framework import DefiTestFramework

from test_framework.util import (
    assert_equal,
)
from test_framework.authproxy import JSONRPCException

class RPCstats(DefiTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ['-acindex=1', '-txnotokens=0', '-amkheight=50', '-bayfrontheight=50', '-bayfrontgardensheight=50', '-rpcstats'],
            ['-acindex=1', '-txnotokens=0', '-amkheight=50', '-bayfrontheight=50', '-bayfrontgardensheight=50'],
        ]

    def run_test(self):
        self.nodes[0].generate(101)

        self.nodes[0].getnewaddress("", "legacy")
        self.nodes[0].getnewaddress("", "legacy")

        self.nodes[0].listunspent()
        self.nodes[0].listunspent()

        listrpcstats = self.nodes[0].listrpcstats()
        assert(any(elem for elem in listrpcstats if elem["name"] == "getnewaddress"))
        assert(any(elem for elem in listrpcstats if elem["name"] == "listunspent"))

        getrpcstats = self.nodes[0].getrpcstats("listunspent")
        assert_equal(getrpcstats["name"], "listunspent")
        assert_equal(getrpcstats["count"], 2)

        try:
            self.nodes[1].getrpcstats("listunspent")
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Rpcstats flag is not set." in errorString)

        try:
            self.nodes[1].listrpcstats()
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Rpcstats flag is not set." in errorString)

if __name__ == '__main__':
    RPCstats().main ()
