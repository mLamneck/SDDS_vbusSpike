#ifndef UCOMMTHREAD_H
#define UCOMMTHREAD_H

#include "uMultask.h"

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

class TcommThreadDefs{
	public:
		constexpr static int ID_DHCP = 5;
		constexpr static int ID_CONNECTIONS = 6;
		enum class TexecRes {idle,noMessage,sendMessage};
		enum class ThandleMessageRes {answer,noAnswer,notMyBusiness};
};

template <int _ID>
class TcommThread : public TcommThreadDefs{
	public:
		constexpr static int ID = _ID;

		constexpr static bool isHandler(Tevent* _ev){ return _ev->tag() == ID; }
		constexpr static void initEvent(TcommEvent& _ev, Tthread* _owner){
			_ev.setId(ID);
			_ev.setOwner(_owner);
		}

};

#endif