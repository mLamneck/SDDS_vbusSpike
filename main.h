#include "uTypedef.h"
#include "uVbusSpike.h"
#include "uUart.h"

TsimulUart simUart1("1uart");
TsimulUart simUart2("2uart");

class TuserStruct : public TmenuHandle{
    Ttimer timer;
    public:
        sdds_struct(
            
        )
        public:
            TuserStruct(){
                simUart1.FlinkedUart = &simUart2; 
                simUart2.FlinkedUart = &simUart1;    
            }
} userStruct;


TvbusSpike485 vsp(userStruct,&simUart1);
//TvbusSpike485 vsp1(userStruct,&simUart2);
