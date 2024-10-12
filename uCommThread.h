#ifndef UCOMMTHREAD_H
#define UCOMMTHREAD_H

#include "uMultask.h"

/*
class TcommEvent : public Tevent{
	using Tevent::Tevent;
	public:
		int id(){ return Fid; } void setId( int _id ) { Fid = _id; }
		constexpr bool msgRequest() { return FmsgRequest; }
		void setMsgRequest(bool _val){ FmsgRequest = _val; }
		void trigger(){ setTimeEvent(0); }
	private:
		dtypes::uint8 Fid;
		bool FmsgRequest = false;
};
*/

class TcommThreadDefs{
	public:
		constexpr static int MSG_REQ_FLAG = 0x80;

		constexpr static int ID_DHCP = 5;
		constexpr static int ID_CONNECTIONS = 6;
		constexpr static int ID_DATASERVER = 7;
		enum class TexecRes {idle,noMessage,sendMessage};
		enum class ThandleMessageRes : int {handled, notMyBusiness};
};

template <int _ID>
class TcommThread : public TcommThreadDefs{
	public:
		constexpr static int ID = _ID;

		constexpr static bool isHandler(Tevent* _ev){ return _ev->args.byte0 == ID; }
		constexpr static void initEvent(Tevent& _ev, Tthread* _owner){
			_ev.args.byte0 = ID;
			_ev.setOwner(_owner);
		}

		void setMsgRequest(Tevent& _ev){
			_ev.args.byte0 = ID | 0x80;
		}
		void setMsgRequest(Tevent* _ev){
			_ev->args.byte0 = ID | 0x80;
		}

		bool isMsgRequest(Tevent& _ev){
			return (_ev.args.byte0 && 0x80);
		}

		void trigger(Tevent& _ev){
			_ev.setTimeEvent(0);
		}

};

#endif