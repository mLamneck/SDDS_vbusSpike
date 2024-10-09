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

		TcommEvent Fevent;
		Taddr FtypeClientAddr; 
		Tport FtypeClientPort;
		TmsgCnt FtypeMsgCnt;
		Tdescr* FtypeCurrItem = nullptr;
		Tdescr* FtypeLastItem = nullptr;
		t_path_entry FtypeCurrIdx;
		dtypes::uint8 FtypeEnumIdx = 0;
		dtypes::uint8 FtypeEnumPos = 0;
	};

	class TdataEvent : public TobjectEvent{

	};

	template <typename Taddr, typename Tport>
	class Tconnection{
		private:
			bool FtxActivity = false;
			bool FrxActivity = false;
		public:
			Taddr FclientAddr;
			Tport FclientPort;
			TdataEvent FobjEvent;

			bool isFree() { return (FclientPort == 0); }
			Taddr clientAddr() { return FclientAddr; }
			Tport clientPort() { return FclientPort; }
			void shutdown(){
				FclientPort = 0;
			}

			void setupLink(Tthread* _thread){
				FobjEvent.setOwner(_thread);
				FobjEvent.signal();
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
		constexpr static int FIRST_PORT = 10;
		constexpr static int MAX_PORT = 2;

		constexpr static int KEEP_ALIVE_TIME = 10000;
		constexpr static int KEEP_ALIVE_DELAY = 10;
		constexpr static int INACTIVITY_CHECK = 6;			//after 6 times KeepAlive time check for inactivitiy

		void init(Tthread* _thread){
			initEvent(FevKa,_thread);
			FevKa.setMsgRequest(true);
			FevKa.signal();
		}

	private:
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef vbusSpike::Tconnection<Taddr,Tport> Tconnection;
		typedef TobjectPool<Tconnection,MAX_PORT> Connections;

		Connections Fconnections;

		//event and iterator to send keepalives
		//FkaEventCnt to shutdown connections without activity
		TcommEvent FevKa;
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
				else if (conn->isFree()){
					freeIdx = i;
				}
			}

			if (freeIdx > -1) idx = freeIdx; 
			else if (idx == -1) return idx;

			auto conn = Fconnections.get(idx);
			conn->FclientAddr = _addr;
			conn->FclientPort = _clientPort;
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
			} else
				_ps.buildErrMsg(TvbusProtocoll::err_invalidPort,clientPort);			
		}

		void checkForInactivConnections(){
			for (auto i = 0; i < Fconnections.MAX_OBJECTS; i++){
				auto conn = Fconnections.get(i);
				if (!conn->isFree() && !conn->hasRxAtivity()){
					conn->shutdown();
				}				
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

		constexpr TexecRes execute(Tevent* _ev, TprotStream& _ps){
			if (_ev == &FevKa){
				while (FkaCurrIdx<Fconnections.MAX_OBJECTS){
					auto conn = Fconnections.get(FkaCurrIdx++);
					if (!conn->isFree() && !conn->hasTxActivity()){
						_ps.setHeader(conn->clientAddr(),0,conn->clientPort(),TvbusProtocoll::port_ka);
						//_ps.setHeader(0xFF,0,conn->clientPort(),TvbusProtocoll::port_ka);
						_ps.writePort(FkaCurrIdx + FIRST_PORT - 1);
						FevKa.setTimeEvent(KEEP_ALIVE_DELAY);
						return TexecRes::sendMessage;
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
			return TexecRes::noMessage;
		}

};

#endif
