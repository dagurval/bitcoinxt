#ifndef BITCOIN_PROOF_H
#define BITCOIN_PROOF_H

#include <memory>

class CCoinsView;
class CTxMemPool;
class CTransaction;
class CBlock;

class InputSpentProver {
    public:
        InputSpentProver(
                CCoinsView& coinstip,
                CTxMemPool& mempool);

        CTransaction proveSpent(const CTransaction& tx);
    protected:
        virtual bool isStandardTx(
                const CTransaction& tx, std::string& reason);

        virtual CBlock readBlockFromDisk(int blockHeight);

    private:
        std::unique_ptr<CCoinsView> view;
        CTxMemPool& mempool;

};

#endif
