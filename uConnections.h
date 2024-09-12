#ifndef UCONNECTIONS_H
#define UCONNECTIONS_H

#include "uCommThread.h"
#include "uVbusProtocol.h"

namespace vbusSpike{
	template <typename Taddr, typename Tport>
	class Tconnection{
		public:
			Taddr FclientAddr;
			Tport FclientPort;
			bool isFree() { return (FclientPort == 0); }
			Taddr clientAddr() { return FclientAddr; }
			Tport clientPort() { return FclientPort; }
			void shutdown(){
				FclientPort = 0;
			}
			bool hasTxActivity(){
				return false;
			}

			bool hasRxAtivity(){
				return true;
			}
	};


}

template<class T, int MAX_OBJECTS>
class TobjectPool{
	private:
		typedef T Tobjects[MAX_OBJECTS];
		Tobjects Fobjects;

		class _Titerator{
			private:
				int Fidx = 0;
				Tobjects* Fobjs;
			public:
				_Titerator(){ }
				_Titerator(Tobjects* _objects){
					Fobjs = _objects;
					Fidx = 0;
				}

				int idx() { return Fidx - 1; }
		
				bool hasNext(){
					return (Fidx < MAX_OBJECTS);
				}

				T* next(){
					return &((*Fobjs)[Fidx++]);
				}
		};

	public:
		typedef _Titerator Titerator;

		_Titerator iterator(){
			Titerator it(&Fobjects);
			return it;
		}

		constexpr T* get(int _idx){ 
			if (_idx < 0 || _idx >= MAX_OBJECTS) return nullptr;
			return &Fobjects[_idx];
		}



};

template <class TprotStream>
class Tconnections : public TmenuHandle, public TcommThread<TcommThreadDefs::ID_CONNECTIONS>{
	public:
		constexpr static int FIRST_PORT = 10;
		constexpr static int MAX_PORT = 2;

		constexpr static int KEEP_ALIVE_TIME = 1000;
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
			auto it = Fconnections.iterator();
			while (it.hasNext()){
				auto conn = it.next();
				if (conn->clientAddr() == _addr && conn->clientPort()==_clientPort) {
					idx = it.idx();
					break;
				}
				else if (conn->isFree()){
					freeIdx = it.idx();
				}
			}

			if (freeIdx > -1) idx = freeIdx; 
			else if (idx == -1) return idx;

			auto conn = Fconnections.get(idx);
			conn->FclientAddr = _addr;
			conn->FclientPort = _clientPort;
			return idx;
		}

		Tconnection* getConnectionByPort(const Tport _port){
			return Fconnections.get((int)_port-FIRST_PORT);
		}

		ThandleMessageRes handleOpenPortReq(TprotStream& _ps){
			Tport clientPort;
			if (!_ps.readVal(clientPort)) return ThandleMessageRes::noAnswer; 
			auto connIdx = allocateConnection(_ps.source(),clientPort);
			if (connIdx > -1){
				_ps.setReturnHeader(_ps.destiny(),clientPort);
				Tport serverPort = connIdx + FIRST_PORT;	
				_ps.writeVal(serverPort);			
			} else
				 _ps.buildErrMsg(TvbusProtocoll::err_noMorePorts,clientPort);
			return ThandleMessageRes::answer;
		}

		ThandleMessageRes handleClosePortReq(TprotStream& _ps){
			Tport clientPort;
			if (!_ps.readVal(clientPort)) return ThandleMessageRes::noAnswer;
			
			auto conn = getConnectionByPort(_ps.port());
			if (conn){
				if (conn->clientPort() == clientPort && _ps.source() == conn->clientAddr()){
					conn->shutdown();
					_ps.setReturnHeader(_ps.destiny(),clientPort);
					clientPort = 0;
					_ps.writeVal(clientPort);
				} else
				_ps.buildErrMsg(TvbusProtocoll::err_invalidPort,clientPort);			//different error msg?
			} else
				_ps.buildErrMsg(TvbusProtocoll::err_invalidPort,clientPort);			
			return ThandleMessageRes::answer;
		}

		ThandleMessageRes handlePortKeepAlive(TprotStream& _ps){
			return ThandleMessageRes::noAnswer;
		}

		void checkForInactivConnections(){
			auto it = Fconnections.iterator();
			while (it.hasNext()){
				auto conn = it.next();
				if (!conn->hasRxAtivity()){
					conn->shutdown();
				}
			}
		}

	public:
		ThandleMessageRes handleMessage(TprotStream& _ps){
			switch (_ps.func()){
				case TvbusProtocoll::port_open: return handleOpenPortReq(_ps);
				case TvbusProtocoll::port_close: return handleClosePortReq(_ps);
				case TvbusProtocoll::port_ka: return handlePortKeepAlive(_ps);
			}
			return ThandleMessageRes::notMyBusiness;
		}

		constexpr TexecRes execute(Tevent* _ev, TprotStream& _ps){
			if (_ev == &FevKa){
				auto conn = Fconnections.get(FkaCurrIdx++);
				if (!conn){

					//after n cycles of sending keepalives, check for inactive
					//connections from the other side
					if (FkaEventCnt++ == INACTIVITY_CHECK){
						checkForInactivConnections();
						FkaEventCnt = 0;
					}

					FevKa.setTimeEvent(KEEP_ALIVE_TIME);
					FkaCurrIdx = 0;

					return TexecRes::noMessage;
				}

				FevKa.setTimeEvent(KEEP_ALIVE_DELAY);
				if (!conn->isFree() && !conn->hasTxActivity()){
					//_ps.setHeader(conn->clientAddr(),0,conn->clientPort(),TvbusProtocoll::port_ka);
					_ps.setHeader(0xFF,0,conn->clientPort(),TvbusProtocoll::port_ka);
					_ps.writePort(FkaCurrIdx + FIRST_PORT - 1);
					return TexecRes::sendMessage;
				}
			}
			return TexecRes::noMessage;
		}

};

#endif
