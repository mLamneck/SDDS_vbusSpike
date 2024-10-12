#ifndef UDHCP_H
#define UDHCP_H

#include "uVbusProtocol.h"
#include "uCommThread.h"
#include "uStrings.h"
#include "uRandom.h"


template<int MAX_ADDRESSES>
class TaddrList {
	private:
		typedef int nativeInt;

		static constexpr int BITS_PER_NATIVE_INT = sizeof(nativeInt) * 8;
		static constexpr int NUM_NATIVE_INTS = (MAX_ADDRESSES + BITS_PER_NATIVE_INT
				- 1) / BITS_PER_NATIVE_INT;
		nativeInt FactiveAddresses[NUM_NATIVE_INTS] = { };
		nativeInt FcollectedAddresses[NUM_NATIVE_INTS] = { };

	public:

		void store(int addr) {
			if (addr < 1 || addr > MAX_ADDRESSES)
				return;
			int index = (addr - 1) / BITS_PER_NATIVE_INT;
			int bitPosition = (addr - 1) % BITS_PER_NATIVE_INT;
			nativeInt bitMask = 1 << bitPosition;
			FactiveAddresses[index] |= bitMask;
			FcollectedAddresses[index] |= bitMask;
		}

		void recycle() {
			for (int i = 0; i < NUM_NATIVE_INTS; ++i) {
				FactiveAddresses[i] = FcollectedAddresses[i];
				FcollectedAddresses[i] = 0;
			}
		}

		int findFree() {
			for (int i = 0; i < NUM_NATIVE_INTS; ++i) {
				nativeInt inverted = ~FactiveAddresses[i];
				if (inverted != 0) {
					int bitPosition = __builtin_ctz(inverted);
					int bitMask = (1 << bitPosition);
					FactiveAddresses[i] |= bitMask;
					FcollectedAddresses[i] |= bitMask;
					return i * BITS_PER_NATIVE_INT + bitPosition + 1;;
				}
			}
			return 0;
		}
};


template <class TprotStream>
class Tdhcp : public TmenuHandle, public TcommThread<TcommThreadDefs::ID_DHCP>{
	public:
		constexpr static int FIRST_FUNC = TvbusProtocoll::dhcp_firstFunc;		
		constexpr static int LAST_FUNC = TvbusProtocoll::dhcp_lastFunc;		
#if MARKI_DEBUG_PLATFORM_XXXXX == 0
		//timing for client in ms 
		constexpr static int KEEP_ALIVE_TIME = 8000;
		constexpr static int SERVER_REQUEST_TIME = 2000;

		//timing for server in ms
		constexpr static int RECYCLE_TIME = 25500;
		constexpr static int COLLECT_TIME = 1000;
		constexpr static int QUERY_REPEAT_TIME = 10;
#else
		//timing for client in ms 
		constexpr static int KEEP_ALIVE_TIME = 1000;
		constexpr static int SERVER_REQUEST_TIME = 100;

		//timing for server in ms
		constexpr static int RECYCLE_TIME = 3000;
		constexpr static int COLLECT_TIME = 500;
		constexpr static int QUERY_REPEAT_TIME = 10;
#endif
	private:
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef typename TprotStream::t_prot_func Tfunc;
		typedef typename TprotStream::t_prot_cs Tcs;
		
		Tevent FclientTimer;
		Tevent FserverTimer;

		TaddrList<64> Faddresses;

		Taddr FmyAddr = TprotStream::ADDR_BROADCAST();

		enum class TtimeoutType : dtypes::uint8 { recycle, decision, query, collect};
		struct{
				bool serverActive : 1;
				bool serverIsActiveButNotReady : 1;
				bool idsCollected : 1;
				TtimeoutType timeoutType : 2;
				dtypes::uint8 queryCnt: 2;
		} Fstatus;

		sdds_struct(
			sdds_var(Tstring,Falias,sdds::opt::saveval,"NUCLEO1")
			sdds_var(Tstring,Fserial,sdds::opt::readonly,"NUCLEO1")
		)

	public:
		typedef TcommThread::TexecRes TexecRes;

		Taddr myAddr(){ return FmyAddr; }

		void init(Tthread* _thread){			
			initEvent(FclientTimer,_thread);
			initEvent(FserverTimer,_thread);
			setMsgRequest(FclientTimer);
			FclientTimer.signal();
			setMsgRequest(FserverTimer);
			resetDhcpServer();
#if MARKI_DEBUG_PLATFORM == 1
			static bool __first_dhcp = true;
			if (__first_dhcp){
				FserverTimer.setTimeEvent(100);
				__first_dhcp = false;
			}
			else{
				FclientTimer.setTimeEvent(20000);
				FserverTimer.setTimeEvent(20000);
			}
#endif
		}

		/****************************************************
		 * helper functions
		 */

		constexpr bool isMySerial(TprotStream& _msg){
			return (_msg.readString() == Fserial);
		}

		constexpr bool isMyAddressValid(){
			return (FmyAddr != TprotStream::ADDR_BROADCAST()); 
		}


		/****************************************************
		 * build messages
		 */

		constexpr void buildImTheOne(TprotStream& _ans){
			_ans.setHeader(_ans.ADDR_BROADCAST(),FmyAddr,0,TvbusProtocoll::dhcp_imServer);
			_ans.setSendPending();
		}

		constexpr void buildQuery(TprotStream& _ps){
			_ps.setHeader(_ps.ADDR_BROADCAST(),_ps.ADDR_BROADCAST(),0,TvbusProtocoll::dhcp_queryReq);
			_ps.setSendPending();
		}

		constexpr void writeCrcAndSerial(TprotStream& _ps){
			_ps.writeCs(0xCCCC);
			_ps.writeString(Falias);
			_ps.setSendPending();
		}

		constexpr void buildServerRequestOrKeepAlive(TprotStream& _ps){
			Tfunc func = _ps.isBroadcast(FmyAddr) ? TvbusProtocoll::dhcp_req : TvbusProtocoll::dhcp_ka;
			_ps.setHeader(_ps.ADDR_BROADCAST(),FmyAddr,0,func);
			writeCrcAndSerial(_ps);
		}
		

		/****************************************************
		 * handle incoming messages
		 */

		constexpr void handleDhcpSet(TprotStream& _msg){
			Taddr setAddr;
			if (!_msg.readVal(setAddr)) return;
			if (isMySerial(_msg)){
				FmyAddr = setAddr;
				Faddresses.store(FmyAddr);
				FclientTimer.setTimeEvent(1);
			}
		}
		
		void handleDhcpReq(TprotStream& _msg){
			//Fstatus.serverActive = true;
			if (!Fstatus.serverActive){
				if (Fstatus.serverIsActiveButNotReady){
					buildImTheOne(_msg);
					return;
				}
				setDecisionTimeout();
				return;
			}
			
			Tcs cs;
			if (!_msg.readVal(cs)) return;
			auto serial = _msg.readString();
			if (serial.length() < 1) return;

			Taddr freeId = Faddresses.findFree();
			if (!freeId){
				//this should never happen
				buildImTheOne(_msg);
				return;				
			}

			//let the node know his new address			
			_msg.setReturnHeader();
			_msg.writeVal(freeId);
			if (_msg.writeString(serial)) {
				_msg.setSendPending();
			}
		}

		ThandleMessageRes handleMessage(TprotStream& _msg){
			Faddresses.store(_msg.source());

			auto func = _msg.func();
			switch(func){
				case TvbusProtocoll::dhcp_queryReq: 
					FclientTimer.setTimeEvent(Trandom::gen(100));
					break;

				case TvbusProtocoll::dhcp_set:
					handleDhcpSet(_msg);
					setServerInactive();
					break;

				case TvbusProtocoll::dhcp_req: 
					handleDhcpReq(_msg);
					break;

				case TvbusProtocoll::dhcp_whoIsReq: 
					if (isMyAddressValid() && isMySerial(_msg)){
						_msg.setReturnHeader();
						writeCrcAndSerial(_msg);		//needed?
					} 
					break;
				
				case TvbusProtocoll::dhcp_ka: case TvbusProtocoll::dhcp_whoIs:
					break;

				case TvbusProtocoll::dhcp_imServer: 
					setServerInactive();
					break;

				default : return ThandleMessageRes::notMyBusiness;
			}
			return ThandleMessageRes::handled;
		}


		/****************************************************
		 * dhcp server stuff
		 */

		/*
		 * called if we are not the active dhcp server but there might be the
		 * need of us beeing the one.
		 */
		void setDecisionTimeout(){
			if (Fstatus.timeoutType == TtimeoutType::decision)
				return;
			Fstatus.timeoutType = TtimeoutType::decision;
			FserverTimer.setTimeEvent(Trandom::gen(500,1200));
		}

		/*
		 * this function is called whenever we receive a dhcp set
		 * or a "dhcp I am the one" message. In this case we switch to passive
		 */
		void setServerInactive(){
			if (Fstatus.timeoutType != TtimeoutType::recycle){
				Fstatus.timeoutType = TtimeoutType::recycle;
				FserverTimer.setTimeEvent(RECYCLE_TIME);
			}
			Fstatus.serverActive = false;
			Fstatus.serverIsActiveButNotReady = false;
		}

		void resetDhcpServer(){
			Fstatus = {0};
			setDecisionTimeout();
		}

		/*
		 * this function is called from our statemachine if we finally want to
		 * take the role of the active dhcp server. In case we don't have a valid
		 * address so far (on startup), we allocate one and if it fails there's 
		 * something strange and we reset everything and start all over again. 
		 * This should never happen but we try to keep the system alive no matter what.
		 */
		void setServerActive(){
			if (Fstatus.serverActive) return;

			if (!isMyAddressValid()){
				auto id = Faddresses.findFree();
				if (!id){
					resetDhcpServer();
					return;
				};
				FmyAddr = id;
				trigger(FclientTimer);
			}

			Fstatus.timeoutType = TtimeoutType::recycle;
			FserverTimer.setTimeEvent(RECYCLE_TIME);			
			Fstatus.serverActive = true;
		}



		/****************************************************
		 * thread execution
		 */

		constexpr void execute(Tevent* _ev){
			//int i = 0;

		}

		constexpr TexecRes execute(Tevent* _ev, TprotStream& _ps){
 			if (_ev == &FclientTimer){
				buildServerRequestOrKeepAlive(_ps);
				int timeout = isMyAddressValid() ? KEEP_ALIVE_TIME : SERVER_REQUEST_TIME;  
				FclientTimer.setTimeEvent(timeout);
				return TexecRes::sendMessage;
			}

			else if (_ev == &FserverTimer){

				switch(Fstatus.timeoutType){
					/*
					 * this is the regular timeout to check if some nodes have been gone
					 * from this network. Recycle the addresses for further use. We do this
					 * regardless if we are the active server or not. If it's time to take
					 * action as dhcp server, we have already all informations
					 */
					case TtimeoutType::recycle:
						Faddresses.recycle();
						Faddresses.store(FmyAddr);
						FserverTimer.setTimeEvent(RECYCLE_TIME);
						return TexecRes::noMessage;

					/*
					 * decide if we have to do the work of dhcp server
					 * this timeout is triggered on every incoming dhcp request
					 * if we are not already the active dhpc server
					 */
					case TtimeoutType::decision:
						if (Fstatus.idsCollected) setServerActive();
						else {
							Fstatus.serverIsActiveButNotReady = true;
							Fstatus.timeoutType = TtimeoutType::query;
							FserverTimer.setTimeEvent(0);	//send query as soon as possible
						}
						buildImTheOne(_ps);
						return TexecRes::sendMessage;

					/*
					 * here we send a query if we have not collected all addresses.
					 * after 2 querys inlcuding the once that others may have sent 
					 * we set another timeout after which we assume we have collected
					 * all addresses
					 */
					case TtimeoutType::query:
						if (Fstatus.queryCnt < 2){
							Fstatus.queryCnt++;
							FserverTimer.setTimeEvent(QUERY_REPEAT_TIME);
							buildQuery(_ps);
							return TexecRes::sendMessage;
						}
						else{
							Fstatus.timeoutType = TtimeoutType::collect;
							FserverTimer.setTimeEvent(COLLECT_TIME);
							return TexecRes::noMessage;
						}

					/*
					 * This is a timeout after the last of 2 queries have been sent.
					 * From now on we assume we have collected all addresses
					 */
					case TtimeoutType::collect:
						Fstatus.idsCollected = true;
						setServerActive();
						Fstatus.timeoutType = TtimeoutType::recycle;
						FserverTimer.setTimeEvent(RECYCLE_TIME);
						return TexecRes::noMessage;					
				}
			}
			return TexecRes::noMessage;
		}
};


#endif
