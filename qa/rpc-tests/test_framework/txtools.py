import time
from test_framework.mininode import CTxOut
from test_framework.script import CScript, OP_RETURN

def grind_nsequence(tx):
    MAX_ALLOWED_NSEQUENCE = 0xffffffff-1
    tx.vin[0].nSequence += 1
    if (tx.vin[0].nSequence >= MAX_ALLOWED_NSEQUENCE):
        tx.vin[0].nSequence = 0

def grind_opreturn(tx):
    tx.vout.append(CTxOut(0, CScript([OP_RETURN])))

def tx_grind_hash(tx, criteria, grinder = grind_nsequence):
    tx.calc_sha256()
    while (not criteria(tx.hash)):
        grinder(tx)
        tx.rehash()
