/*Descriptson and examples of COptCCParams class found in:
    script/standard.h/cpp 
    class COptCCParams
    
structure of data in vData payload attached to end of CCvout: 
    param 
    OP_1 
    param
    OP_2 ... etc until OP_16
    OP_PUSHDATA4 is the last OP code to tell things its at the end. 
    
    taken from standard.cpp line 22: COptCCParams::COptCCParams(std::vector<unsigned char> &vch)

EXAMPLE taken from Verus how to create scriptPubKey from COptCCParams class:
EXAMPLE taken from Verus how to decode scriptPubKey from COptCCParams class:
*/

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout)
{
    CCcontract_info *cp, C;
    cp = CCinit(&C,EVAL_STAKEGUARD);

    CPubKey ccAddress = CPubKey(ParseHex(cp->CChexstr));

    // return an output that is bound to the 