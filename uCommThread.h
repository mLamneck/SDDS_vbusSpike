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

		/**
		 * @brief usage of generic eventArg
		 * 
		 * byte0 = id of commThread
		 * byte1 = msgRequest (the thread with this event wants to send a message)
		 * word1 = port number
		 */
		static void eventCommId(Tevent* _ev, const dtypes::uint8 _val){ _ev->args.byte0 = _val; }
		static dtypes::uint8 eventCommId(Tevent* _ev){ return _ev->args.byte0; }
		static void setMsgRequest(Tevent* _ev, const dtypes::uint8 _val){ _ev->args.byte1 = _val; }
		static dtypes::uint8 msgRequest(Tevent* _ev){ return _ev->args.byte1; }
		static void eventPort(Tevent* _ev, dtypes::uint16 _val){ _ev->args.word1 = _val; }
		static dtypes::uint16 eventPort(Tevent* _ev){ return _ev->args.word1; }
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

		void trigger(Tevent& _ev){
			_ev.setTimeEvent(0);
		}

};

#endif