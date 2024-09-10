/*
 * uart_485.h
 *
 *  Created on: Aug 22, 2024
 *      Author: mark.lamneck
 */

#ifndef UART_485_H_
#define UART_485_H_

/*
 * Flags and timing sequences
 *
 * FwaitForAck & FackReceived
 * 		FwaitForAck is set after each transmission in isr regardless if ack is required or not
 * 		FwaitForAck is reset on reception of a byte that is not a Ack
 * 		considerations:
 * 			FwaitForAck is set for a message that doesn't need to be acked:
 * 				on transmission complete we stop repeating this message anyway because nTries=0
 * 			FwaitForAck is set but no messages will be received and the flag stays alive forever
 * 				doesn't matter
 *
 */
class Tuart_com7: public ThUart<USART1_BASE>, public Tuart {

#ifndef ARB_PIN
		typedef TgpioPin<GPIOB_BASE, LL_GPIO_PIN_0> ARB_PIN;
#endif

		volatile Tbyte* FtxPtr = nullptr;
    volatile Tbyte* FtxEnd = nullptr;

    bool doWrite() override {
			auto txPtr = FpTxMsg->c7start;
			FtxEnd = txPtr + FpTxMsg->length;
			disableIsr();
			if (!rxBusy()) {
				transmit(*txPtr++);
				transmit(*txPtr++);
				FtxPtr = txPtr;
				isr_rxne_disable();
				flushRx();
				isr_txfe_enable();
				enableIsr();
				return true;
			}
			enableIsr();
			return false;
		}

		void sendAck() override {
			if (!rxBusy()){
				transmit (ACK);
				isr_rxne_disable();
				isr_tc_enable();
			}
		}

		void initHardware() override {
			ThUart<USART1_BASE>::init();
			ARB_PIN::init(ARB_PIN::PIN_MODE::it_rising_falling);
		}

		void resetRxBusy() override {
			LL_EXTI_ClearFallingFlag_0_31 (LL_EXTI_LINE_0);
			LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_0);
		}

		void switchToRx() override {
			resetRxBusy();
			isr_tc_disable();
			flushRx();
			isr_rxne_enable();
		}

		virtual void disableIsr() override {
			isr_disable();
		}
		virtual void enableIsr() override {
			isr_enable();
		}

	public:
		void init() {
			initHardware();
		}

		bool rxBusy() {
			return (LL_EXTI_ReadFallingFlag_0_31(LL_EXTI_LINE_0) > 0)
				|| (LL_EXTI_ReadRisingFlag_0_31(LL_EXTI_LINE_0) > 0);
			/*
			if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_0) != RESET){
				return true;
			}
			if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_0) != RESET){
				return true;
			}
			return false;
		*/
		}

		void handleErrors(uint32_t _errors) {
			uart_hardware_clearErrors();

			if (_errors & USART_ISR_ORE) {
			}

			// Handle Framing Error
			if (_errors & USART_ISR_FE) {
			}

			// Handle Noise Error
			if (_errors & USART_ISR_NE) {
			}

			// Handle Parity Error
			if (_errors & USART_ISR_PE) {
			}

			//why???
			/*
			if (isr_tc_enabled()) {
				isr_tc_disable();
				isr_rxne_enable();
			}
			*/
		}

		void initWaitForLastByteTransmission(){
			isr_txfe_disable();
			if (tdr_notEmpty()) {
				isr_tc_enable();
			}
			else {
				lastByteTransmitted();
			}
		}

		void lastByteTransmitted(){
			switchToRx();
			FrxCurr = 0;
			if (FtxPtr){
				setWaitForAck();
				FevTxIdle.signalFromIsr();
			}
			FtxPtr = nullptr;
		}

		void checkHandleCollision(uint32_t _ddrNotEmpty){
			if (_ddrNotEmpty){
				Ferrors.txCollisions++;
				initWaitForLastByteTransmission();
			}

			/*
			 * if last sent byte is not received yet allow once to
			 * wait for the next tdr empty interrupt
			 */
			else{
				//if (Fstatus & STAT_fERR_COLLSION){
				if (Fcollision){
					Ferrors.txCollisions++;
					initWaitForLastByteTransmission();
				}
				else Fcollision=true;
			}
		}

		void isr() {

			/***************************************************/
			// clear error flags
			auto errors = uart_hardware_error();
			if (errors)
				handleErrors(errors);

			/***************************************************/
			// receive interrupt

			if (isr_rxne_enabled()) {
				while (ddr_notEmpty()) {
					isr_readByte(ddr_read());
				}
			}

			/***************************************************/
			// transmit register empty interrupt

			else if (isr_txfe_enabled()) {
				auto notEmpty = ddr_notEmpty();
				if (notEmpty && (*(FtxPtr-2) == ddr_read())){
					if (FtxPtr < FtxEnd) {
						transmit(*FtxPtr++);
					}
					else {
						initWaitForLastByteTransmission();
					}
					//Fstatus &= (~STAT_fERR_COLLSION);
					Fcollision=false;
				}
				else {
					checkHandleCollision(notEmpty);
				}
			}

			/***************************************************/
			// transmission complete

			else if (isr_tc_enabled()) {
				lastByteTransmitted();
			}

			/***************************************************/
			// receiver timeout

			else if ( isr_rto_flagSet() ) {
				/*
				if (FtxMsg){
					if ((FnTries < 1) || (Fstatus & STAT_fACK_RECEIVED)){
	        	FtxMsg = nullptr;
	    			FtxIdle.signal();
	  			}
					else{

					}
				}
				*/
			}

			/***************************************************/

		}

};

#endif /* UART_485_H_ */
