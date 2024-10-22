#ifndef UUART_H
#define UUART_H

#if MARKI_DEBUG_PLATFORM==0
	#define DEBUG_UART_ECHO 0//!MARKI_DEBUG_PLATFORM
#else
	#define DEBUG_UART_ECHO 0
#endif

#include "uRandom.h"
#include "uMultask.h"
#include "uCom7.h"

/*
    msgLength(n)   stuffbytes  totalLength (#cs=1)
    1-6            1           n+1+#cs
    7-13           2           n+2+#cs
 */
template <int _plainSize>
class _TCom7messageBufferRX : public TlinkedListElement{
	public:		 
		//volatile _TCom7messageBufferRX<_plainSize>* next;
		bool containsAnswer = false;
		volatile dtypes::uint8 length = 0;
		dtypes::uint8 data[_plainSize + 1+(_plainSize/7+1)];
};

class Tuart;

/*
 * c7start:
 * make some space before the plainMessage. This makes it possible to do the com7 conversion
 * on the same buffer. We need the addional space for the stuffbytes to not overwrite the
 * plain data during conversion
 */
template <int _plainSize>
struct _TCom7messageBufferTX{
		friend class Tuart;
	private:
		constexpr static int cBUF_COM7FILL = 1+(_plainSize/7+1);
		constexpr static int c7SIZE = _plainSize + cBUF_COM7FILL;
	public:
		constexpr static int cSIZE_PLAIN = _plainSize;
		dtypes::uint8 length;
		dtypes::uint8 c7start[cBUF_COM7FILL];
		dtypes::uint8 data[_plainSize];
};


class Tuart : public Tthread{
	public:
		typedef dtypes::uint8 Tbyte;
#if (MARKI_DEBUG_PLATFORM==1)
		const char* name;
#endif
		constexpr static uint8_t ADDR_BROADCAST = 0xFF;
		constexpr static Tbyte ACK = 0x86;
		constexpr static Tbyte cRX_BUF_SIZE = 64;
		constexpr static Tbyte cTX_BUF_SIZE = 64;
		constexpr static Tbyte c_N_RX_BUFFERS = 3;

		typedef _TCom7messageBufferRX<cRX_BUF_SIZE> TmessageBufferRX;
		typedef _TCom7messageBufferTX<cTX_BUF_SIZE> TmessageBufferTX;

		/*****************************************************/
		// status flags

		typedef uint32_t Tstatus;
		struct {

				//receive errors
				//ERROR												CNT				DESCR
				uint32_t bufOvr 			= 0;	//ALL				msg bigger than max msg size
				uint32_t noMoreBufs 	= 0;	//ALL				all rx buffers full and a new message starts
				uint32_t msgTooSmall	= 0;	//ALL				msg received and length < min msg lenght
				uint32_t frameErrors 	= 0;	//MINE			com7 conversion failed

				//transmit errors
				uint32_t txCollisions	= 0;	//collision detected while sending

		} Ferrors;

		/*****************************************************/

	protected:
		volatile Tbyte FmyAddr = 127;
		volatile uint32_t FackReceived = false;
		volatile uint32_t FwaitForAck = false;
		volatile uint32_t Fcollision = false;
		volatile bool FendOfFrameReceived = false;
		volatile bool FendOfFrameEvPending = false;

		void setWaitForAck(){
			FwaitForAck = true;
		}
		void clearWaitForAck(){
			FwaitForAck = false;
		}

		//variables for receiving...
		volatile uint32_t FrxCurr = 0;										//ofs in FpRxCurr used in isr
		TmessageBufferRX  FrxBuffers[c_N_RX_BUFFERS];			//array of buffers available for receiving. Not accessed directly
		TmessageBufferRX* FpRxCurrRead;										//buffer currently used for receive isr
		TmessageBufferRX* FpRxCurrBuf;										//buffer currently used for receive isr

		//variables for sending...
		TmessageBufferTX FtxMsg;
		TmessageBufferTX* FpTxMsg = nullptr;								//current msg to be send
		dtypes::int8 FnTries = 0;

		struct{
				bool inRandonTimeout : 1;
				bool inTransmission : 1;
		} FprivStatus;

		//events send from isr
		multask::TisrEvent FevEndOfFrame;
		Tevent FevTxIdle;

		//events to be send to the user of this object
		Tevent* FevReveiveNotify = nullptr;
		Tevent* FevTxIdleNotify = nullptr;

	public:
		Tuart()
		: FevEndOfFrame(this)
		, FevTxIdle(this)
		{
			FpRxCurrRead = &FrxBuffers[0];
			FpRxCurrBuf = &FrxBuffers[0];
		}

		void begin(Tevent* _rxReceived, Tevent* _txIdle){
			FevReveiveNotify = _rxReceived;
			FevTxIdleNotify = _txIdle;
		}

		void setMyAddr(const Tbyte _myAddr){ FmyAddr = _myAddr; }

		TmessageBufferTX* getTxBuffer(){
			if (!FpTxMsg) return &FtxMsg;
			else return nullptr;
		}

		TmessageBufferRX* getMessage(){
			if (FpRxCurrRead->length > 0) return FpRxCurrRead;
			return nullptr;
		}

		void ackMessage( TmessageBufferRX* _msg){
			_msg->length = 0;
			_msg->containsAnswer = false;
			if (FpRxCurrRead < &FrxBuffers[c_N_RX_BUFFERS-1]) FpRxCurrRead++;
			else FpRxCurrRead = &FrxBuffers[0];
		}

		/*
		 * in case FrxCurr = cRX_BUF_SIZE+1 we have a noMoreBufferError
		 */
		void handleBufferOvr(){
			if (FrxCurr>cRX_BUF_SIZE) return;
			Ferrors.bufOvr++;
		};

		void handleNoMoreBuffers(){
			Ferrors.noMoreBufs++;
			/*
			 * prevent the following scenario
			 *
			 * frame starts when no buffers available. In the middle of the transmission
			 * a buffer becomes available. If we not set this we would start processing
			 * half of the message.
			 *
			 * we set FrxCurr to cRX_BUF_SIZE+1 to distinguish between noMoreBufs and bufOvr
			 * in handleBufferOvr
			 */
			FrxCurr = cRX_BUF_SIZE+1;
		};

		void handleErrMsgSmall(){
			TP2::pulse();
			TP2::pulse();
			TP2::pulse();
			Ferrors.msgTooSmall++;
		}

		void handleFrameError(){
			Ferrors.frameErrors++;
		};

		void switchToNextRxBuffer(){
			if (FpRxCurrBuf < &FrxBuffers[c_N_RX_BUFFERS-1]) FpRxCurrBuf++;
			else FpRxCurrBuf = &FrxBuffers[0];
		}

		void write(dtypes::uint8* _buf, int _size){
			TP1::pulse();
			if (FpTxMsg) return;

			if (_buf[1] == 0xFF) FnTries = 1;
			else FnTries = 2;
			if (_buf[2] != 0xFF)
				FmyAddr = _buf[2];
			auto len = com7::encrypt(_buf,_size,FtxMsg.c7start,TmessageBufferTX::c7SIZE);
			if (len < 0) return;

			FtxMsg.length = len;
			FpTxMsg = &FtxMsg;

			//start random timer if not receiving, not transmitting and randomTimer not running
			if (!FprivStatus.inRandonTimeout && !FprivStatus.inTransmission && (FrxCurr == 0)){
				setRandomTimeout();
			}
		}

		virtual void disableIsr(){ }
		virtual void enableIsr(){ }
		virtual void switchToRx(){ }
		virtual void initHardware() {}
		virtual void sendAck() {};
		virtual void resetRxBusy() {};
		virtual bool doWrite() = 0;

		void setRandomTimeout(){
			resetRxBusy();
			uint8_t r = (Trandom::gen() & 0x07);
			setTimeEventTicks(r+3);
			FprivStatus.inRandonTimeout = true;
			FendOfFrameEvPending = false;
		}

		void handleError(const Tstatus _errors){

		}

		void setTxIdle(){
			FpTxMsg = nullptr;
			if (FevTxIdleNotify){
				#if MARKI_DEBUG_PLATFORM == 0
					FevTxIdleNotify->signal();	//isr not needed at the moment but it doesn't hurt
				#else
					FevTxIdleNotify->setTimeEvent(2);
				#endif
			}
		}
		//Tbyte msgBuffer[16];

		void resetUart(){
			FprivStatus.inTransmission = false;
			disableIsr();
			initHardware();
			switchToRx();
			enableIsr();
		}

		void handleTimeout(){
			if (FpTxMsg){

				/* something really went wrong
				 *
				 * we have successfully started a transmission and after a timeout
				 * that should be more than enough to finish the transmission we didn't
				 * receive the transmission complete event that should be send in any case
				 * from the isr not matter what happened.
				 *
				 * try to to reinit the hardware?
				 */
				if (FprivStatus.inTransmission){
					//resetUart();
					FprivStatus.inTransmission = false;
				}

				if (FprivStatus.inRandonTimeout){
					FprivStatus.inRandonTimeout = false;

					if (!FendOfFrameEvPending){
						//try to start a transmission
						if (doWrite()){
							#if MARKI_DEBUG_PLATFORM == 0
							clearWaitForAck();
							FackReceived = 0;
							#endif
							FprivStatus.inTransmission = true;
						}
					}
				}

				/*
				 * not in random timeout and no endOfFrame received during a "long" time
				 */
				else if (!FendOfFrameReceived){
					setRandomTimeout();
					/* this is actually not true, but in the unlikely case we receive an incomplete
					 * message during the random timeout we want to long timeout next time */
					FendOfFrameReceived = true;
					return;
				}
			}

			/*
			 * anyway set 10ms to check again
			 *
			 * in case we successfully started a transmission above
			 * 		we will receive the transmission complete interrupt
			 * otherwise there must be an ongoing transmission and
			 * 		we should receive an Receive Complete event
			 */
			FendOfFrameReceived = false;
			setTimeEvent(10);
		}

		void execute(Tevent* _ev) override{

			/*********************************************/
			//rxBufFul

			//end of frame received
			if (_ev == &FevEndOfFrame) {
				setRandomTimeout();
				FendOfFrameEvPending = false;

				if (FwaitForAck){
					if(FackReceived){
						setTxIdle();
						FackReceived = 0;
					}
					clearWaitForAck();
				}

				//notify user we have a message
				if (getMessage() && FevReveiveNotify)
					FevReveiveNotify->signal();
#if DEBUG_UART_ECHO == 1
				auto msg = getMessage();
				while (msg){
					auto func = msg->data[4];
					if (func == 0xCE){
						auto tx = getTxBuffer();
						if (tx){
							tx->data[0] = 0x01;
							tx->data[1] = msg->data[2];
							tx->data[2] = FmyAddr;
							tx->data[3] = 0x00;
							tx->data[4] = 0xCC;
							memcpy(&tx->data[5],&Ferrors,sizeof(Ferrors));  		//uart_com7.write(&msg->data, msg->length);
							//write(tx,5 + sizeof(Ferrors));
						}
					}
					//event or callback!?
					ackMessage(msg);
					msg = getMessage();
				}
#endif
			}


			/*********************************************/
			//EtxIdle

			else if (_ev == &FevTxIdle){
				FprivStatus.inTransmission = false;
				setRandomTimeout();
				if (FnTries > 1){
					FnTries--;
				}
				else {
					setTxIdle();
				}
			}


			/*********************************************/
			//timeout

			else{
				handleTimeout();
			}

			/*********************************************/

		}


		/***********************************************************************/
		/* readByte
		 *
		 * to be called from ISR
		 */
		/***********************************************************************/
		void isr_readByte(Tbyte _byte){

			//not end of message
			if ((_byte & 0x80) == 0){
				clearWaitForAck();
				if (FrxCurr >= cRX_BUF_SIZE) return handleBufferOvr();
				else if (FpRxCurrBuf->length > 0) return handleNoMoreBuffers();
				FpRxCurrBuf->data[FrxCurr++] = _byte;
				return;
			}

			//end of message
			else{
				TP3::high();
				if ((_byte == ACK) && (FrxCurr == 0)){
					FackReceived = 1;
				}

				else{
					clearWaitForAck();
					if (FrxCurr < cRX_BUF_SIZE){
						if (FrxCurr >= 6){	//minimum msg size=5 + cs + stuffbyte
							Tbyte destiny = com7::decryptByteAtPos<1>(FpRxCurrBuf->data);
							if (destiny==FmyAddr || destiny==ADDR_BROADCAST){
								resetRxBusy();
								if (FpRxCurrBuf->length==0){
									FpRxCurrBuf->data[FrxCurr++] = _byte;
									auto len = com7::decrypt(FpRxCurrBuf->data,FrxCurr,FpRxCurrBuf->data,cRX_BUF_SIZE);
									if (len > 0){
										if (destiny != ADDR_BROADCAST)
											sendAck();
										FpRxCurrBuf->length = len;
										switchToNextRxBuffer();
									} else handleFrameError();
								} else handleNoMoreBuffers();
							}
						} else handleErrMsgSmall();
					} else handleBufferOvr();
				}

				//anyway reset RxBusy, init FrxCurr for next message and wakeup thread
				resetRxBusy();
				FrxCurr = 0;
				FendOfFrameReceived = true;
				FendOfFrameEvPending = true;
				FevEndOfFrame.signal();
				TP3::low();
			}
		}

#if MARKI_DEBUG_PLATFORM
		void debugReadMessage(dtypes::uint8* _buf, int _size){
			static dtypes::uint8 _buffer[64];
			auto len = com7::encrypt(_buf,_size,&_buffer[0],64);
			if (len < 0) return;

			_buf = &_buffer[0];
			while (len > 0){
				isr_readByte(*_buf++);
				len--;
			}
		}
#endif

};

#if MARKI_DEBUG_PLATFORM == 1
class TsimulUart : public Tuart{
	public:
		Tuart* FlinkedUart;

		TsimulUart(const char* _name) : Tuart() {
			name = _name;
		}

		void sendAck() override{
			FlinkedUart->isr_readByte(ACK);
		}

		virtual bool doWrite(){
			if (FlinkedUart){
				setWaitForAck();
				Tbyte* p = FpTxMsg->c7start;
				int _size = FpTxMsg->length;
				while (_size > 0){
					FlinkedUart->isr_readByte(*p++);
					_size--;
				}
			}
			FevTxIdle.signal();
			return true;
		}
};
#endif

#endif



