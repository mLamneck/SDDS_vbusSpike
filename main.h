#include "uTypedef.h"
#include "uVbusSpike.h"
#include "uUart.h"

TsimulUart simUart1("1uart");
TsimulUart simUart2("2uart");

class TsubMenu : public TmenuHandle{
	Ttimer timer;
	public:
		sdds_struct(
			sdds_var(Tuint8,val1)
			sdds_var(Tuint8,val2)
		)
		public:
			TsubMenu(){
			
			}
};

class TuserStruct : public TmenuHandle{
    Ttimer timer;
    Ttimer timer2;
	int status = 0;
    public:
        sdds_struct(
			sdds_var(Tuint8,val1)
			sdds_var(Tuint8,val2)
			sdds_var(Tuint8,val3)
			sdds_var(Tuint8,val4)
			sdds_var(TsubMenu,sub)
        )
        public:
            TuserStruct(){
                simUart1.FlinkedUart = &simUart2; 
                simUart2.FlinkedUart = &simUart1;    

				timer.start(1000);
				on(timer){
					switch(status){
						case 0 :
							val1++;
							status=1;
							break; 
						case 1 : 
							val2++;
							val3++;
							status=0;
							break; 
						case 2 : 
							break;
						
					}
					timer.start(1000);
				};

            }
} userStruct;


TvbusSpike485 vsp(userStruct,&simUart1);
//TvbusSpike485 vsp1(userStruct,&simUart2);
