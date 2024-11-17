#include "uTypedef.h"
#include "uMultask.h"
#include "uParamSave.h"

//hardware definition
#include "mhal/uGpio.h"
//typedef TgpioPin<GPIOA_BASE, 6> TP_GREEN;		used for pwm
typedef mhal::TgpioPin<GPIOB_BASE, 3> TP_BLUE;
typedef mhal::TgpioPin<GPIOB_BASE, 4> TP_YELLOW;
typedef mhal::TgpioPin<GPIOA_BASE, 5> TledGreen;
typedef mhal::TgpioPin<GPIOA_BASE, 10> ARB_PIN;

#include "mhal/uPwm.h"
typedef mhal::TpwmChannel<TIM3_BASE,1> TpwmVdc0;
typedef mhal::TpwmChannel<TIM3_BASE,1> TpwmVdc1;
typedef mhal::TpwmChannel<TIM3_BASE,1> TpwmVdc2;
typedef mhal::TpwmChannel<TIM3_BASE,1> TpwmVdc3;

#include "uHystBuck.h"

//|2|o|n|3|o|f|f|4|h|o|l|d|5|h ..
//|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9| ..
//|0|o|n|0|o|f|f|0|h|o|l|d|0|h|o|l|d|1|0|h|o|l|d|2|0|h|o|l|d|3| ..
sdds_enum(on,off,hold,hold1,hold2,hold3) TonOffState;

class TuserStruct : public mms::hystDC::Tapp{
	private:
	public:
};
