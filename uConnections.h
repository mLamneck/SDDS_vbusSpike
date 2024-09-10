#ifndef UCONNECTIONS_H
#define UCONNECTIONS_H

#include "uCommThread.h"
#include "uVbusProtocol.h"

namespace vbusSpike{
	template <typename Taddr, typename Tport>
	class Tconnection{
		private:
			Taddr FclientAddr;
			Tport FclientPort;
	};

}

template<class T, int MAX_OBJECTS>
class TobjectPool{
	private:
		typedef T Tobjects[MAX_OBJECTS];
		Tobjects Fobjects;

		class Titerator{
			int Fidx = 0;
			Tobjects* Fobjects;

			bool hasNext(){
				return (Fidx < MAX_OBJECTS);
			}

			T* next(){
				return Fobjects[Fidx++];
			}
		};

	public:
		Titerator iterator(){
			Titerator it;
			it.Fobjects = &Fobjects;
			it.Fidx = 0;
			return it;
		}

};

template <class TprotStream>
class Tconnections : public TmenuHandle, public TcommThread<TcommThreadDefs::ID_CONNECTIONS>{
	private:
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef vbusSpike::Tconnection<Taddr,Tport> Tconnection;

		TobjectPool<Tconnection,16> Fconnections;

		bool allocateConnection(Taddr _addr, Tport _clientPort){
			auto it = Fconnections.iterator();
			while (it.hasNext()){
				it.next();
			}
			return false;
		}

		ThandleMessageRes handleOpenPortReq(TprotStream& _ps){
			Tport port;
			if (!_ps.readVal(port)) return ThandleMessageRes::noAnswer; 
			return ThandleMessageRes::notMyBusiness;
		}
	public:
		ThandleMessageRes handleMessage(TprotStream& _ps){
			switch (_ps.func()){
				case TvbusProtocoll::port_open:
					break;
				
				case TvbusProtocoll::port_close:
					break;
				case TvbusProtocoll::port_ka:
					break;
				}
			return ThandleMessageRes::notMyBusiness;
		}
};

#endif