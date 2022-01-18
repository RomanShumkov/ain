#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) DeFi Blockchain Developers
# Distributed under the MIT software license, see the accompanying
# file LICENSE or http://www.opensource.org/licenses/mit-license.php.
"""Test Loan - empty vault."""

from test_framework.test_framework import DefiTestFramework

from test_framework.util import assert_equal, assert_raises_rpc_error

import calendar
import time
from decimal import Decimal

class LoanZeroLoanTest (DefiTestFramework):
    symbolDFI = "DFI"
    symbolBTC = "BTC"
    symboldUSD = "DUSD"
    symbolGOOGL = "GOOGL"

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [
            ['-txnotokens=0', '-amkheight=50', '-bayfrontheight=50', '-bayfrontgardensheight=1', '-fortcanningheight=50', '-fortcanninghillheight=50', '-eunosheight=50', '-txindex=1']
        ]

    def setup(self):
        print("Generating initial chain...")
        self.nodes[0].generate(150)

        self.account0 = self.nodes[0].get_genesis_keys().ownerAuthAddress

        self.nodes[0].createtoken({
            "symbol": self.symbolBTC,
            "name": "BTC token",
            "isDAT": True,
            "collateralAddress": self.account0
        })
        self.nodes[0].generate(1)

        idDFI = list(self.nodes[0].gettoken(self.symbolDFI).keys())[0]
        idBTC = list(self.nodes[0].gettoken(self.symbolBTC).keys())[0]

        self.nodes[0].utxostoaccount({self.account0: "2000@" + self.symbolDFI})

        oracle_address1 = self.nodes[0].getnewaddress("", "legacy")
        price_feeds1 = [{"currency": "USD", "token": "DFI"},
                        {"currency": "USD", "token": "BTC"},
                        {"currency": "USD", "token": "GOOGL"}]

        self.oracle_id = self.nodes[0].appointoracle(
            oracle_address1, price_feeds1, 10)
        self.nodes[0].generate(1)

        # feed oracle
        oracle1_prices = [{"currency": "USD", "tokenAmount": "10@GOOGL"},
                          {"currency": "USD", "tokenAmount": "10@DFI"},
                          {"currency": "USD", "tokenAmount": "10@BTC"}]
        timestamp = calendar.timegm(time.gmtime())
        self.nodes[0].setoracledata(self.oracle_id, timestamp, oracle1_prices)
        self.nodes[0].generate(1)

        self.nodes[0].setcollateraltoken({
            'token': idDFI,
            'factor': 1,
            'fixedIntervalPriceId': "DFI/USD"})
        self.nodes[0].generate(1)

        self.nodes[0].setcollateraltoken({
            'token': idBTC,
            'factor': 1,
            'fixedIntervalPriceId': "BTC/USD"})
        self.nodes[0].generate(1)

        self.nodes[0].setloantoken({
            'symbol': self.symbolGOOGL,
            'name': "Tesla stock token",
            'fixedIntervalPriceId': "GOOGL/USD",
                                    'mintable': True,
                                    'interest': 1})
        self.nodes[0].setloantoken({
            'symbol': self.symboldUSD,
            'name': "DUSD stable token",
            'fixedIntervalPriceId': "DUSD/USD",
                                    'mintable': True,
                                    'interest': 1})
        self.nodes[0].generate(1)

        self.nodes[0].createloanscheme(150, 5, 'LOAN150')
        self.nodes[0].generate(1)

        iddUSD = list(self.nodes[0].gettoken(self.symboldUSD).keys())[0]
        idGOOGL = list(self.nodes[0].gettoken(self.symbolGOOGL).keys())[0]

        poolOwner = self.nodes[0].getnewaddress("", "legacy")
        self.nodes[0].createpoolpair({
            "tokenA": iddUSD,
            "tokenB": idDFI,
            "commission": Decimal('0.002'),
            "status": True,
            "ownerAddress": poolOwner,
            "pairSymbol": "DUSD-DFI",
        })
        self.nodes[0].createpoolpair({
            "tokenA": idGOOGL,
            "tokenB": self.symboldUSD,
            "commission": Decimal('0.002'),
            "status": True,
            "ownerAddress": poolOwner,
            "pairSymbol": "GOOGL-DUSD",
        })
        self.nodes[0].generate(1)

        self.nodes[0].minttokens(
            ["1@" + self.symbolGOOGL, "1000@" + self.symboldUSD])
        self.nodes[0].generate(1)

        self.nodes[0].addpoolliquidity({
            self.account0: ["300@" + self.symboldUSD, "100@" + self.symbolDFI]
        }, self.account0, [])
        self.nodes[0].addpoolliquidity(
            {self.account0: ["1@" + self.symbolGOOGL, "300@" + self.symboldUSD]}, self.account0, [])

    def test_should_fail_set_zero_loan_value(self):
        oracle_prices = [{"currency": "USD", "tokenAmount": "0@GOOGL"}]
        timestamp = calendar.timegm(time.gmtime())
        assert_raises_rpc_error(-22, 'Amount out of range', self.nodes[0].setoracledata, self.oracle_id, timestamp, oracle_prices)

    def test_should_fail_set_negative_loan_value(self):
        oracle_prices = [{"currency": "USD", "tokenAmount": "-1@GOOGL"}]
        timestamp = calendar.timegm(time.gmtime())
        assert_raises_rpc_error(-22, 'Amount out of range', self.nodes[0].setoracledata, self.oracle_id, timestamp, oracle_prices)

    def test_should_fail_when_value_overflows(self):
        pass

    def test_should_increase_interest_with_small_loan(self):
        vaultId = self.nodes[0].createvault(self.account0, '')
        self.nodes[0].generate(1)

        self.nodes[0].deposittovault(vaultId, self.account0, '1@DFI')
        self.nodes[0].generate(1)

        # take small loan amount
        self.nodes[0].takeloan({
                    'vaultId': vaultId,
                    'amounts': "0.008@GOOGL"})
        self.nodes[0].generate(1)

        oracle_price = [{"currency": "USD", "tokenAmount": "2747.81@GOOGL"}, # Using GOOGL price on 18/01/2022
                        {"currency": "USD", "tokenAmount": "2747.81@DFI"}]
        timestamp = calendar.timegm(time.gmtime())
        self.nodes[0].setoracledata(self.oracle_id, timestamp, oracle_price)
        self.nodes[0].generate(120) # Update to next price

        vault = self.nodes[0].getvault(vaultId)
        [interestAmount, _] = vault['interestAmounts'][0].split("@")

        assert_equal(float(interestAmount), 0.0)
        assert_equal(float(vault['loanValue']), 21.98248)

    def run_test(self):
        self.setup()
        self.test_should_fail_set_zero_loan_value()
        self.test_should_fail_set_negative_loan_value()
        self.test_should_fail_when_value_overflows()
        self.test_should_increase_interest_with_small_loan()


if __name__ == '__main__':
    LoanZeroLoanTest().main()
