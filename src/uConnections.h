#ifndef UCONNECTIONS_H
#define UCONNECTIONS_H

#include "uCommThread.h"
#include "uVbusProtocol.h"
#include "uObjectPool.h"
#include "uTypedef.h"

namespace vbusSpike{
	template <class TprotStream>
	struct TtypeST{
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef typename TprotStream::t_path_entry t_path_entry; 
		typedef typename TprotStream::t_path_length t_path_length; 
		typedef typename TprotStream::t_prot_msgCnt TmsgCnt; 

		Tevent Fevent;
		Taddr FtypeClientAddr; 
		Tport FtypeClientPort;
		TmsgCnt FtypeMsgCnt;
		Tdescr* FtypeCurrItem = nullptr;
		Tdescr* FtypeLastItem = nullptr;
		t_path_entry FtypeCurrIdx;
		dtypes::uint8 FtypeEnumIdx = 0;
	};

	template <class TprotStream>
	struct TdataST{
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef typename TprotStream::t_path_entry TpathEntry; 
		typedef typename TprotStream::t_prot_msgCnt TmsgCnt; 

		Tevent Fevent;
		TmsgCnt FmsgCnt = 0;
		TpathEntry FcurrIdx;
		TpathEntry FlastIdx;
		bool busy() { return FmsgCnt != 0; }
		void resetBusy() { FmsgCnt = 0; }
	};

	template <class TprotStream>
	class Tconnection{
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef typename TprotStream::t_prot_port Tport;
		private:
			bool FtxActivity = false;
			bool FrxActivity = false;
		public:
			Taddr FclientAddr;
			Tport FclientPort;
			sdds::TlinkTime FlinkTime;
			TobjectEvent FobjEvent;
			TdataST<TprotStream> FdataThread;

			bool isDataEvent(Tevent* _ev){ return FobjEvent.event() == _ev; }
			bool isFree() { return (FclientPort == 0); }
			Taddr clientAddr() { return FclientAddr; }
			Tport clientPort() { return FclientPort; }
			void shutdown(){
				FclientPort = 0;
				FdataThread.Fevent.reclaim();
				FdataThread.Fevent.setOwner(nullptr);
				FobjEvent.setOwner(nullptr);
				Tdescr* observedObj = FobjEvent.observedObj();
				if (observedObj){
					if (observedObj->isStruct()) 
						static_cast<Tstruct*>(observedObj)->value()->events()->remove(&FobjEvent);
					else
						static_cast<TarrayBase*>(observedObj)->events()->remove(&FobjEvent);
				}
				FobjEvent.cleanup();
			}

			template <class _Tlocator>
			void setupLink(Tthread* _thread, Tport _servPort, _Tlocator& l, sdds::TlinkTime _linkTime){
				FobjEvent.setOwner(_thread);
				TcommThreadDefs::eventPort(FobjEvent.event(),_servPort);
				FdataThread.Fevent.setOwner(_thread);
				TcommThreadDefs::eventPort(&FdataThread.Fevent,_servPort);
				FlinkTime = _linkTime;
				FobjEvent.setObservedRange(l);

				/* if we extend the option, we can specify a linkTime within the option
				   and we can iterate over all items be observed and decide what linkTime to use
				auto f = l.firstItem();
				do{
					if (f->option().linkTime().isTimed()){

					}
					f = f->next();
				} while (f != l.lastItem());			
				*/

				if (_linkTime != sdds::TlinkTime::ON_CHANGE) return;

				if (l.result()->isStruct()){
					l.menuHandle()->events()->push_first(&FobjEvent);
				}
				else if (l.result()->isArray()){
					static_cast<TarrayBase*>(l.result())->events()->push_first(&FobjEvent);
				}
			}

			void setTxActivity(bool _val){ FtxActivity = _val; }
			void setRxActivity(bool _val){ FrxActivity = _val; }
			bool hasTxActivity(){ return FtxActivity; }
			bool hasRxAtivity(){ return FrxActivity; }
	};
}

template <class TprotStream>
class Tconnections : public TmenuHandle, public TcommThread<TcommThreadDefs::ID_CONNECTIONS>{
	public:
		constexpr static int FIRST_PORT = 0x10;
		constexpr static int MAX_PORT = 22;

		constexpr static int KEEP_ALIVE_TIME = 10000;
		constexpr static int KEEP_ALIVE_DELAY = 10;
		constexpr static int INACTIVITY_CHECK = 6;			//after 6 times KeepAlive time check for inactivitiy

		void init(Tthread* _thread){
			initEvent(FevKa,_thread);
			TcommThreadDefs::setMsgRequest(&FevKa,true);
			FevKa.setTimeEvent(KEEP_ALIVE_TIME);
		}

		typedef vbusSpike::Tconnection<TprotStream> Tconnection;
	private:
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef TobjectPool<Tconnection,MAX_PORT> Connections;

		Connections Fconnections;

		//event and iterator to send keepalives
		//FkaEventCnt to shutdown connections without activity
		Tevent FevKa;
		int FkaCurrIdx = 0;
		int FkaEventCnt = 0;

		int allocateConnection(Taddr _addr, Tport _clientPort){
			int idx = -1;
			int freeIdx = -1;

			for (auto i = 0; i < Fconnections.MAX_OBJECTS; i++){
				auto conn = Fconnections.get(i);
				if (conn->clientAddr() == _addr && conn->clientPort()==_clientPort) {
					idx = i;
					break;
				}
				else if (conn->isFree() && (freeIdx == -1)){
					freeIdx = i;
				}
			}

			if (freeIdx > -1) idx = freeIdx; 
			else if (idx == -1) return idx;

			auto conn = Fconnections.get(idx);
			conn->FclientAddr = _addr;
			conn->FclientPort = _clientPort;
			conn->setRxActivity(true);
			conn->setTxActivity(true);
			return idx;
		}

		void handleOpenPortReq(TprotStream& _ps){
			Tport clientPort;
			if (!_ps.readVal(clientPort)) return;
			auto connIdx = allocateConnection(_ps.source(),clientPort);
			if (connIdx > -1){
				_ps.setReturnHeader(clientPort);
				Tport serverPort = connIdx + FIRST_PORT;	
				_ps.writeVal(serverPort);	
				_ps.setSendPending();		
			} else
				 _ps.buildErrMsg(TvbusProtocoll::err_noMorePorts,clientPort);
		}

		void handleClosePortReq(TprotStream& _ps, Tconnection* _conn){
			Tport clientPort;
			if (!_ps.readVal(clientPort)) return;
			
			if (_conn->clientPort() == clientPort && _ps.source() == _conn->clientAddr()){
				_conn->shutdown();
				_ps.setReturnHeader(clientPort);
				clientPort = 0;
				_ps.writeVal(clientPort);
				_ps.setSendPending();
			} else
				_ps.buildErrMsg(TvbusProtocoll::err_invalidPort,clientPort);			
		}

		void checkForInactivConnections(){
			for (auto i = 0; i < Fconnections.MAX_OBJECTS; i++){
				auto conn = Fconnections.get(i);
				if (!conn->isFree() && !conn->hasRxAtivity()){
					conn->shutdown();
				}
				else
					conn->setRxActivity(false);
			}
		}

	public:
		Tconnection* getConnection(const Tport _port, const Taddr _addr){
			auto conn = Fconnections.get((int)_port-FIRST_PORT); 
			if (!conn) return nullptr;
			if (conn->isFree() || conn->clientAddr() != _addr) return nullptr;
			conn->setRxActivity(true);
			return conn;
		}

		Tconnection* getConnection(const Tport _port){
			auto conn = Fconnections.get((int)_port-FIRST_PORT); 
			if (!conn) return nullptr;
			if (conn->isFree()) return nullptr;
			return conn;
		}

		ThandleMessageRes handleMessage(TprotStream& _ps, Tconnection* _conn){
			switch (_ps.func()){
				case TvbusProtocoll::port_close: 
					handleClosePortReq(_ps,_conn);
					break;
				
				default : return ThandleMessageRes::notMyBusiness; 
			}
			return ThandleMessageRes::handled;
		}

		ThandleMessageRes handleMessage(TprotStream& _ps){
			switch (_ps.func()){
				case TvbusProtocoll::port_open: 
					handleOpenPortReq(_ps);
					break;
				
				default : return ThandleMessageRes::notMyBusiness; 
			}
			return ThandleMessageRes::handled;
		}

		void execute(Tevent* _ev, TprotStream& _msg){
			if (_ev == &FevKa){
				while (FkaCurrIdx<Fconnections.MAX_OBJECTS){
					auto conn = Fconnections.get(FkaCurrIdx++);
					if (!conn->isFree()){
						 if(!conn->hasTxActivity()){
							_msg.setHeader(conn->clientAddr(),0,conn->clientPort(),TvbusProtocoll::port_ka);
							_msg.writePort(FkaCurrIdx + FIRST_PORT - 1);
							FevKa.setTimeEvent(KEEP_ALIVE_DELAY);
							_msg.setSendPending();
						 }
						 else
							 conn->setTxActivity(false);
					}
				}

				//after n cycles of sending keepalives, check for inactive
				//connections from the other side
				if (FkaEventCnt++ == INACTIVITY_CHECK){
					checkForInactivConnections();
					FkaEventCnt = 0;
				}

				FevKa.setTimeEvent(KEEP_ALIVE_TIME);
				FkaCurrIdx = 0;
			}
		}

};

#endif
