#include "../uCom7.h"
#include "uTestCase.h"

class Tcom7Test : public TtestCase{
	typedef dtypes::uint8 uint8;
    private:
        uint8 Fbuffer[64];
    public:
        using TtestCase::TtestCase;

        bool testCase(uint8* _input, int _inLen, com7::Tres _expRes, uint8* _expOut=nullptr, int _expOutLen = 0, bool decrypt = true){
            memset(&Fbuffer,0,sizeof(Fbuffer));
            com7::Tres res;
            if (decrypt) res = com7::decrypt(&_input[0],_inLen,&Fbuffer[0],sizeof(Fbuffer));
            else res = com7::encrypt(&_input[0],_inLen,&Fbuffer[0],sizeof(Fbuffer));
            
            debug::log("inp: %s",binToHex(_input,std::min(_inLen,16)));
            debug::log("exp: %s",binToHex(_expOut,std::min(_expOutLen,16)));
            debug::log("out: %s",binToHex(Fbuffer,16));

            if (res != _expRes){
                debug::log("test failed res=%d <> expRes=%d",res,_expRes);
                return false;
            }

            if (res > 0){
                int len = res;
                if (len != _expOutLen){
                    debug::log("test failed outLen=%d <> expOutLen=%d",len,_expOutLen);
                    return false;
                }
                for (auto i = 0; i<len; i++){
                    if (Fbuffer[i] != _expOut[i]){
                        debug::log("test failed res[%d]=0x%2X <> expRes[%d]=0x%.2X",i,Fbuffer[i],i,_expOut[i]);
                        return false;
                    }
                }
            }
            debug::log("case successful");
            return true;
        }

		/*
        bool testCase1(uint8* _input, int _inLen, bool _expRes, uint8* _expOut=nullptr, int _expOutLen = 0, bool decrypt = true){
            memset(&Fbuffer,0,sizeof(Fbuffer));
            Tcom7 c7;
            bool res;
            if (decrypt) res = c7.decrypt(&_input[0],_inLen,&Fbuffer[0],sizeof(Fbuffer));
            else res = c7.encrypt(&_input[0],_inLen,&Fbuffer[0],sizeof(Fbuffer));
            
            debug::log("inp: %s",binToHex(_input,std::min(_inLen,16)));
            debug::log("exp: %s",binToHex(_expOut,std::min(_expOutLen,16)));
            debug::log("out: %s",binToHex(Fbuffer,16));

            if (res != _expRes){
                debug::log("test failed res=%d <> expRes=%d",res,_expRes);
                return false;
            }

            if (res){
                int len = c7.len();
                if (len != _expOutLen){
                    debug::log("test failed outLen=%d <> expOutLen=%d",len,_expOutLen);
                    return false;
                }
                for (auto i = 0; i<len; i++){
                    if (Fbuffer[i] != _expOut[i]){
                        debug::log("test failed res[%d]=0x%2X <> expRes[%d]=0x%.2X",i,Fbuffer[i],i,_expOut[i]);
                        return false;
                    }
                }
            }
            debug::log("case successful");
            return true;
        }
		*/

        int makeRes(int value){
            return value;
            return (value > 0);
        }

        void testDecrypt(){
            debug::log("---------------------------------");
            debug::log("running tests for edge cases");

            //msg must at least contain 3bytes -> fail
            doTest([this](){
                uint8 input[] = {};
                return testCase(&input[0],sizeof(input),makeRes(com7::decryptErrs::INP_TOO_SHORT));
            },"edge case 1");

            //msg must at least contain 3bytes -> fail
            doTest([this](){
                uint8 input[] = {0x00};
                return testCase(&input[0],sizeof(input),makeRes(com7::decryptErrs::INP_TOO_SHORT));
            },"edge case 2");

            //msg must at least contain 3bytes -> fail
            doTest([this](){
                uint8 input[] = {0x00,0x00};
                return testCase(&input[0],sizeof(input),makeRes(com7::decryptErrs::INP_TOO_SHORT));
            },"edge case 3");

            //valid length and cs but no EOM -> fail
            doTest([this](){
                uint8 input[] = {0x00,0x00,0x00};
                return testCase(&input[0],sizeof(input),makeRes(com7::decryptErrs::NO_EOM));
            },"edge case 4");

            debug::log("---------------------------------");
            debug::log("running tests for valid msgs");
            doTest([this](){
                uint8 input[] = {0x32, 0x34, 0x32, 0x36, 0x36, 0x37, 0x89};
                uint8 expRes[] = {0x68, 0x65, 0x6C, 0x6C, 0x6F};
                return testCase(&input[0],sizeof(input),makeRes(sizeof(expRes)),&expRes[0],sizeof(expRes));
            },"convert hello");

            doTest([this](){
                uint8 expRes[] = {0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x61, 0x73, 0x64, 0x68, 0x66, 0x6C, 0x6B, 0x6A, 0x61, 0x68, 0x73, 0x64, 0x6A, 0x6B, 0xF6, 0x66, 0x68, 0x61, 0x73, 0x6A, 0x6B, 0x6C, 0x64, 0x68, 0x66, 0x6A, 0x6B, 0x6C, 0x61, 0x73, 0x68, 0x6A, 0x6B, 0x61, 0x64, 0x73, 0x6C, 0x66};
                uint8 input[] = {0x72, 0x34, 0x32, 0x36, 0x36, 0x37, 0x30, 0x39, 0x50, 0x32, 0x34, 0x33, 0x36, 0x35, 0x35, 0x30, 0x12, 0x34, 0x39, 0x32, 0x35, 0x35, 0x7B, 0x33, 0x16, 0x34, 0x30, 0x39, 0x35, 0x35, 0x36, 0x32, 0x68, 0x34, 0x33, 0x35, 0x35, 0x36, 0x30, 0x39, 0x2C, 0x34, 0x35, 0x35, 0x30, 0x32, 0x39, 0x36, 0x00, 0x33, 0xD1};
                return testCase(&input[0],sizeof(input),makeRes(sizeof(expRes)),&expRes[0],sizeof(expRes));
            },"convert long msg");
        }

        void testEncrypt(){
            debug::log("---------------------------------");
            debug::log("running tests for edge cases");

            //lengh(input)=0 -> fail
            doTest([this](){
                uint8 input[] = {};
                uint8 expRes[] = {0x55, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x33, 0x01, 0xFF};
                return testCase(&input[0],sizeof(input),makeRes(com7::encryptErrs::INP_TOO_SHORT),&expRes[0],sizeof(expRes),false);
            },"encrypt 0byte msg");

            //edge case 7byte msg -> extra stuffbyte needs to be placed
            doTest([this](){
                uint8 input[] = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67};
                uint8 expRes[] = {0x55, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x33, 0x01, 0xFF};
                return testCase(&input[0],sizeof(input),makeRes(sizeof(expRes)),&expRes[0],sizeof(expRes),false);
            },"encrypt 7byte msg");

            //edge case 14byte msg -> extra stuffbyte needs to be placed
            doTest([this](){
                uint8 input[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};
                uint8 expRes[] = {0x2A, 0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x55, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x01, 0xF6};
                return testCase(&input[0],sizeof(input),makeRes(sizeof(expRes)),&expRes[0],sizeof(expRes),false);
            },"encrypt 14byte msg");

            debug::log("---------------------------------");
            debug::log("running tests for for some msgs");

            doTest([this](){
                uint8 input[] = {0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x61, 0x73, 0x64, 0x68, 0x66, 0x6C, 0x6B, 0x6A, 0x61, 0x68, 0x73, 0x64, 0x6A, 0x6B, 0xF6, 0x66, 0x68, 0x61, 0x73, 0x6A, 0x6B, 0x6C, 0x64, 0x68, 0x66, 0x6A, 0x6B, 0x6C, 0x61, 0x73, 0x68, 0x6A, 0x6B, 0x61, 0x64, 0x73, 0x6C, 0x66};
                uint8 expRes[] = {0x72, 0x34, 0x32, 0x36, 0x36, 0x37, 0x30, 0x39, 0x50, 0x32, 0x34, 0x33, 0x36, 0x35, 0x35, 0x30, 0x12, 0x34, 0x39, 0x32, 0x35, 0x35, 0x7B, 0x33, 0x16, 0x34, 0x30, 0x39, 0x35, 0x35, 0x36, 0x32, 0x68, 0x34, 0x33, 0x35, 0x35, 0x36, 0x30, 0x39, 0x2C, 0x34, 0x35, 0x35, 0x30, 0x32, 0x39, 0x36, 0x00, 0x33, 0xD1};
                return testCase(&input[0],sizeof(input),makeRes(sizeof(expRes)),&expRes[0],sizeof(expRes),false);
            },"encrypt long msg");

            debug::log("---------------------------------");
            debug::log("running tests for for some auto generated msgs");

            //init random seed to be reproducible;
            std::srand(55);
            for (int msgLen=3; msgLen<56; msgLen++){
                doTest([this,msgLen](){
                    uint8 bufPlain[128];
                    uint8 bufEncrypted[128];
                    for (int i=0; i<msgLen-1; i++){
                        bufPlain[i] = std::rand();
                    }
                    int encLen =com7::encrypt(&bufPlain[0],msgLen,&bufEncrypted[0],sizeof(bufEncrypted));
                    return testCase(&bufEncrypted[0],encLen,makeRes(msgLen),&bufPlain[0],msgLen,true);
                },"decrypt");
            }
        }

        bool test() override{
            testDecrypt();
            testEncrypt();
            return true;
    }
} c7test("uCom7");


void test(){
    c7test.runTests();
}
