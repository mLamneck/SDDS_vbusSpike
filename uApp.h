#include "uTypedef.h"
#include "uMultask.h"

//|2|o|n|3|o|f|f|4|h|o|l|d|5|h ..
//|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9| ..
//|0|o|n|0|o|f|f|0|h|o|l|d|0|h|o|l|d|1|0|h|o|l|d|2|0|h|o|l|d|3| ..
sdds_enum(on,off,hold,hold1,hold2,hold3) TonOffState;

class TsubMenu : public TmenuHandle{
	Ttimer timer;
	public:
		sdds_struct(
			sdds_var(Tuint8,var1)
			sdds_var(Tuint8,var2)
			sdds_var(Tuint8,var3)
		)
		public:
			TsubMenu(){
				
			}
};

class TuserStruct : public TmenuHandle{
		Ttimer timer;
		sdds_struct(
				sdds_var(TonOffState,enum0,0,TonOffState::e::off)
				sdds_var(TonOffState,enum1,0,TonOffState::e::hold)
				sdds_var(Tuint8,val,0,3)
				sdds_var(Tstring,str,sdds::opt::showString,"0123456789")
				sdds_var(Tuint16,val1,0,4)
				sdds_var(TsubMenu,sub)
				sdds_var(Ttime,time)
				sdds_var(Tuint32,val2,0,5)
				sdds_var(Tuint32,val4,0,6)
				sdds_var(Tuint32,val5,0,7)
		)
	public:
		TuserStruct(){
			timer.start(1000);
			str = "asdf";
			on(timer){
				TLED::toggle();
				timer.start(1000);
				if (enum0 == TonOffState::e::on){
					//val = 1;
					//val1 = 2;
					str = "hallo";
					auto val = time.value();
					val.tv_sec++;
					//time = val;
					//val++;
					//val2++;
				}
			};
		}
};
